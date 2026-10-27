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
    void draw() const;
    void release();

    [[nodiscard]] std::size_t gpuBytes() const noexcept;

private:
    void ensureMaterial();
    void reserveVertices(std::size_t count);
    void addVertex(Vector2 point, Vector2 uv, Color color);
    void flush();
    void clearMeshes();

    Material material_{};
    std::vector<Mesh> meshes_;
    std::vector<float> positions_;
    std::vector<float> texcoords_;
    std::vector<unsigned char> colors_;
    std::vector<unsigned short> indices_;
    std::size_t gpuBytes_{};
};

} // namespace arrow
