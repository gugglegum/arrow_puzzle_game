#include "StaticBoardGeometry.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace arrow {
namespace {

constexpr int kDiscSize = 128;
constexpr int kAtlasWidth = kDiscSize + 4;
constexpr float kDiscRadius = 62.0F;
// Stay well below the 16-bit index limit, with a bounded CPU upload buffer.
constexpr std::size_t kBatchVertices = 16384;
constexpr Vector2 kWhiteUv{
    (static_cast<float>(kDiscSize) + 2.0F) / kAtlasWidth,
    0.5F
};

} // namespace

StaticBoardGeometry::~StaticBoardGeometry() {
    release();
}

void StaticBoardGeometry::ensureMaterial() {
    if (material_.maps != nullptr) {
        return;
    }

    // All dots and round line joins share this tiny antialiased disc. The white
    // strip supplies solid strokes and arrowheads in the same draw calls.
    Image atlas = GenImageColor(kAtlasWidth, kDiscSize, BLANK);
    if (atlas.data == nullptr) {
        throw std::runtime_error("Could not allocate board geometry atlas");
    }
    auto* pixels = static_cast<Color*>(atlas.data);
    for (int y = 0; y < kDiscSize; ++y) {
        for (int x = 0; x < kAtlasWidth; ++x) {
            const float dx = static_cast<float>(x) + 0.5F -
                             kDiscSize * 0.5F;
            const float dy = static_cast<float>(y) + 0.5F -
                             kDiscSize * 0.5F;
            const float coverage = x >= kDiscSize
                ? 1.0F
                : std::clamp(
                      kDiscRadius + 0.5F - std::hypot(dx, dy),
                      0.0F,
                      1.0F
                  );
            pixels[y * kAtlasWidth + x] = {
                255, 255, 255,
                static_cast<unsigned char>(std::round(coverage * 255.0F))
            };
        }
    }
    const Texture2D texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (texture.id == 0) {
        throw std::runtime_error("Could not upload board geometry atlas");
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    material_ = LoadMaterialDefault();
    SetMaterialTexture(&material_, MATERIAL_MAP_DIFFUSE, texture);
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

void StaticBoardGeometry::addVertex(Vector2 point, Vector2 uv, Color color) {
    positions_.insert(positions_.end(), {point.x, point.y, 0.0F});
    texcoords_.insert(texcoords_.end(), {uv.x, uv.y});
    colors_.insert(colors_.end(), {color.r, color.g, color.b, color.a});
}

void StaticBoardGeometry::addCircle(Vector2 center, float radius, Color color) {
    reserveVertices(4);
    const auto first = static_cast<unsigned short>(positions_.size() / 3);
    const float extent = radius * (kDiscSize * 0.5F) / kDiscRadius;
    constexpr float rightUv = static_cast<float>(kDiscSize) / kAtlasWidth;
    addVertex({center.x - extent, center.y - extent}, {0.0F, 0.0F}, color);
    addVertex({center.x - extent, center.y + extent}, {0.0F, 1.0F}, color);
    addVertex({center.x + extent, center.y + extent}, {rightUv, 1.0F}, color);
    addVertex({center.x + extent, center.y - extent}, {rightUv, 0.0F}, color);
    for (int offset : {0, 1, 2, 0, 2, 3}) {
        indices_.push_back(static_cast<unsigned short>(first + offset));
    }
}

void StaticBoardGeometry::addLine(
    Vector2 from,
    Vector2 to,
    float thickness,
    Color color
) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    if (length <= 0.0F) {
        return;
    }
    const Vector2 radius{
        -dy * thickness * 0.5F / length,
        dx * thickness * 0.5F / length
    };
    reserveVertices(4);
    const auto first = static_cast<unsigned short>(positions_.size() / 3);
    addVertex({from.x - radius.x, from.y - radius.y}, kWhiteUv, color);
    addVertex({from.x + radius.x, from.y + radius.y}, kWhiteUv, color);
    addVertex({to.x + radius.x, to.y + radius.y}, kWhiteUv, color);
    addVertex({to.x - radius.x, to.y - radius.y}, kWhiteUv, color);
    for (int offset : {0, 1, 2, 0, 2, 3}) {
        indices_.push_back(static_cast<unsigned short>(first + offset));
    }
}

void StaticBoardGeometry::addTriangle(
    Vector2 a,
    Vector2 b,
    Vector2 c,
    Color color
) {
    reserveVertices(3);
    const auto first = static_cast<unsigned short>(positions_.size() / 3);
    addVertex(a, kWhiteUv, color);
    addVertex(b, kWhiteUv, color);
    addVertex(c, kWhiteUv, color);
    for (int offset : {0, 1, 2}) {
        indices_.push_back(static_cast<unsigned short>(first + offset));
    }
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

void StaticBoardGeometry::draw() const {
    // Preserve ordering with the background and other queued 2D shapes.
    rlDrawRenderBatchActive();
    for (Mesh mesh : meshes_) {
        DrawMesh(mesh, material_, MatrixIdentity());
    }
}

void StaticBoardGeometry::release() {
    clearMeshes();
    if (material_.maps != nullptr) {
        // The material owns its atlas; the default shader belongs to raylib.
        UnloadMaterial(material_);
        material_ = {};
    }
}

std::size_t StaticBoardGeometry::gpuBytes() const noexcept {
    return gpuBytes_;
}

} // namespace arrow
