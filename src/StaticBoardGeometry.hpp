#pragma once

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace arrow {

// Camera-independent geometry for the idle board. Vertex data uses a bounded
// RAM upload batch, then lives on the GPU; raylib retains only compact indices.
class StaticBoardGeometry {
public:
    StaticBoardGeometry() = default;
    ~StaticBoardGeometry();
    StaticBoardGeometry(const StaticBoardGeometry&) = delete;
    StaticBoardGeometry& operator=(const StaticBoardGeometry&) = delete;

    void begin();
    void addCircle(Vector2 center, float radius, Color color);
    void addLine(Vector2 from, Vector2 to, float thickness, Color color);
    void addTriangle(Vector2 a, Vector2 b, Vector2 c, Color color);
    void finish();
    void draw(float zoom) const;
    // Reuse the same coverage shader for animated primitives without creating
    // or uploading new persistent meshes each frame.
    void beginDynamic(float zoom);
    void endDynamic();
    void release();

    [[nodiscard]] std::size_t gpuBytes() const noexcept;

private:
    void ensureMaterial();
    void reserveVertices(std::size_t count);
    void addVertex(Vector2 point, Vector2 local, Color color, float kind);
    void addQuad(Rectangle bounds, Color color, float kind);
    void flush();
    void clearMeshes();

    Material material_{};
    int pixelSizeLocation_{-1};
    bool drawingDynamic_{};
    std::vector<Mesh> meshes_;
    std::vector<float> positions_;
    std::vector<float> texcoords_;
    std::vector<unsigned char> colors_;
    std::vector<unsigned short> indices_;
    std::size_t gpuBytes_{};
};

} // namespace arrow
