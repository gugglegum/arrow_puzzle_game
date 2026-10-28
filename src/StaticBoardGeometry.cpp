#include "StaticBoardGeometry.hpp"
#include "BoardCoverageShader.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace arrow {
namespace {

// Stay well below the 16-bit index limit, with a bounded CPU upload buffer.
constexpr std::size_t kBatchVertices = 16384;

} // namespace

StaticBoardGeometry::~StaticBoardGeometry() {
    release();
}

void StaticBoardGeometry::ensureMaterial() {
    if (material_.maps != nullptr) {
        return;
    }

    Shader shader = LoadShaderFromMemory(
        render::kBoardCoverageVertexShader,
        render::kBoardCoverageFragmentShader
    );
    if (shader.id == 0 || shader.id == rlGetShaderIdDefault()) {
        throw std::runtime_error("Could not load board antialiasing shader");
    }
    material_ = LoadMaterialDefault();
    material_.shader = shader;
    pixelSizeLocation_ = GetShaderLocation(shader, "pixelSize");
}

void StaticBoardGeometry::clearMeshes() {
    for (Mesh mesh : meshes_) {
        UnloadMesh(mesh);
    }
    meshes_.clear();
    gpuBytes_ = 0;
}

void StaticBoardGeometry::begin() {
    clearMeshes();
    ensureMaterial();
    positions_.clear();
    texcoords_.clear();
    colors_.clear();
    indices_.clear();
    positions_.reserve(kBatchVertices * 3);
    texcoords_.reserve(kBatchVertices * 2);
    colors_.reserve(kBatchVertices * 4);
    indices_.reserve(kBatchVertices * 3 / 2);
}

void StaticBoardGeometry::reserveVertices(std::size_t count) {
    if (positions_.size() / 3 + count > kBatchVertices) {
        flush();
    }
}

void StaticBoardGeometry::addVertex(
    Vector2 point,
    Vector2 local,
    Color color,
    float kind
) {
    positions_.insert(positions_.end(), {point.x, point.y, kind});
    texcoords_.insert(texcoords_.end(), {local.x, local.y});
    colors_.insert(colors_.end(), {color.r, color.g, color.b, color.a});
}

void StaticBoardGeometry::addQuad(Rectangle bounds, Color color, float kind) {
    const float x = bounds.x;
    const float y = bounds.y;
    const float w = bounds.width;
    const float h = bounds.height;
    if (drawingDynamic_) {
        rlCheckRenderBatchLimit(4);
        rlSetTexture(rlGetTextureIdDefault());
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(-w*0.5F, -h*0.5F); rlVertex3f(x, y, kind);
        rlTexCoord2f(-w*0.5F, h*0.5F); rlVertex3f(x, y+h, kind);
        rlTexCoord2f(w*0.5F, h*0.5F); rlVertex3f(x+w, y+h, kind);
        rlTexCoord2f(w*0.5F, -h*0.5F); rlVertex3f(x+w, y, kind);
        rlEnd();
        rlSetTexture(0);
        return;
    }
    reserveVertices(4);
    const auto first = static_cast<unsigned short>(positions_.size() / 3);
    addVertex({x, y}, {-w*0.5F, -h*0.5F}, color, kind);
    addVertex({x, y+h}, {-w*0.5F, h*0.5F}, color, kind);
    addVertex({x+w, y+h}, {w*0.5F, h*0.5F}, color, kind);
    addVertex({x+w, y}, {w*0.5F, -h*0.5F}, color, kind);
    for (int offset : {0, 1, 2, 0, 2, 3}) {
        indices_.push_back(static_cast<unsigned short>(first + offset));
    }
}

void StaticBoardGeometry::addCircle(Vector2 center, float radius, Color color) {
    addQuad({center.x-radius, center.y-radius, 2*radius, 2*radius}, color, 1.0F);
}

void StaticBoardGeometry::addLine(
    Vector2 from,
    Vector2 to,
    float thickness,
    Color color
) {
    if (from.x == to.x && from.y == to.y) {
        return;
    }
    if (from.x != to.x && from.y != to.y) {
        throw std::invalid_argument("Static board segments must be orthogonal");
    }
    const float half = thickness * 0.5F;
    const Rectangle bounds = from.x == to.x
        ? Rectangle{from.x-half, std::min(from.y, to.y), thickness, std::abs(to.y-from.y)}
        : Rectangle{std::min(from.x, to.x), from.y-half, std::abs(to.x-from.x), thickness};
    addQuad(bounds, color, from.x == to.x ? 6.0F : 0.0F);
}

void StaticBoardGeometry::addTriangle(
    Vector2 a,
    Vector2 b,
    Vector2 c,
    Color color
) {
    // Board arrowheads are cardinal isosceles triangles, with a as the tip.
    const Vector2 direction{a.x - (b.x+c.x)*0.5F, a.y - (b.y+c.y)*0.5F};
    const float kind = std::abs(direction.x) > std::abs(direction.y)
        ? (direction.x > 0.0F ? 2.0F : 4.0F)
        : (direction.y > 0.0F ? 3.0F : 5.0F);
    const float left = std::min({a.x, b.x, c.x});
    const float top = std::min({a.y, b.y, c.y});
    const float right = std::max({a.x, b.x, c.x});
    const float bottom = std::max({a.y, b.y, c.y});
    addQuad({left, top, right-left, bottom-top}, color, kind);
}

void StaticBoardGeometry::flush() {
    if (indices_.empty()) {
        return;
    }
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(positions_.size() / 3);
    mesh.triangleCount = static_cast<int>(indices_.size() / 3);
    mesh.vertices = positions_.data();
    mesh.texcoords = texcoords_.data();
    mesh.colors = colors_.data();
    // DrawMesh uses a non-null CPU index pointer to select indexed drawing,
    // even after upload. Retain only this small array, not the vertex data.
    mesh.indices = static_cast<unsigned short*>(MemAlloc(
        static_cast<unsigned int>(indices_.size() * sizeof(unsigned short))
    ));
    if (mesh.indices == nullptr) {
        throw std::runtime_error("Could not allocate board geometry indices");
    }
    std::copy(indices_.begin(), indices_.end(), mesh.indices);
    UploadMesh(&mesh, false);
    // UploadMesh copies the data into static GPU buffers. These CPU pointers
    // belong to the reusable upload batch, never to UnloadMesh().
    mesh.vertices = nullptr;
    mesh.texcoords = nullptr;
    mesh.colors = nullptr;
    meshes_.push_back(mesh);
    gpuBytes_ += positions_.size() * sizeof(float) +
                 texcoords_.size() * sizeof(float) + colors_.size() +
                 indices_.size() * sizeof(unsigned short);
    positions_.clear();
    texcoords_.clear();
    colors_.clear();
    indices_.clear();
}

void StaticBoardGeometry::finish() {
    flush();
    // Do not retain a second copy of the idle board in ordinary RAM.
    std::vector<float>{}.swap(positions_);
    std::vector<float>{}.swap(texcoords_);
    std::vector<unsigned char>{}.swap(colors_);
    std::vector<unsigned short>{}.swap(indices_);
}

void StaticBoardGeometry::draw(float zoom) const {
    // Preserve ordering with the background and other queued 2D shapes.
    rlDrawRenderBatchActive();
    const float pixelSize = 1.0F / std::max(zoom, 0.0001F);
    SetShaderValue(material_.shader, pixelSizeLocation_, &pixelSize, SHADER_UNIFORM_FLOAT);
    for (Mesh mesh : meshes_) {
        DrawMesh(mesh, material_, MatrixIdentity());
    }
}

void StaticBoardGeometry::beginDynamic(float zoom) {
    ensureMaterial();
    BeginShaderMode(material_.shader);
    const float pixelSize = 1.0F / std::max(zoom, 0.0001F);
    SetShaderValue(material_.shader, pixelSizeLocation_, &pixelSize, SHADER_UNIFORM_FLOAT);
    drawingDynamic_ = true;
}

void StaticBoardGeometry::endDynamic() {
    EndShaderMode();
    drawingDynamic_ = false;
}

void StaticBoardGeometry::beginDarkening(float zoom) {
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE, RL_ZERO, RL_ONE,
        RL_FUNC_REVERSE_SUBTRACT, RL_FUNC_ADD
    );
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    beginDynamic(zoom);
}

void StaticBoardGeometry::endDarkening() {
    endDynamic();
    EndBlendMode();
}

void StaticBoardGeometry::release() {
    clearMeshes();
    if (material_.maps != nullptr) {
        // The material owns its shader; its default white texture is shared.
        UnloadMaterial(material_);
        material_ = {};
    }
}

std::size_t StaticBoardGeometry::gpuBytes() const noexcept {
    return gpuBytes_;
}

} // namespace arrow
