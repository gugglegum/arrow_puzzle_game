#include "StaticBoardGeometry.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 96;
constexpr double kPi = 3.141592653589793;

void addPrimitive(arrow::StaticBoardGeometry& geometry, int primitive, float zoom) {
    const Vector2 center{64.0F / zoom, 48.0F / zoom};
    if (primitive == 0) {
        geometry.addLine({center.x, 16.0F/zoom}, {center.x, 80.0F/zoom}, 6.4F, BLACK);
    } else if (primitive == 6) {
        geometry.addLine({16.0F/zoom, center.y}, {112.0F/zoom, center.y}, 6.4F, BLACK);
    } else if (primitive == 1) {
        geometry.addCircle(center, 3.2F, BLACK);
    } else {
        const auto rotate = [&](Vector2 point) {
            for (int turn = 0; turn < primitive - 2; ++turn) {
                point = {-point.y, point.x};
            }
            return Vector2{center.x + point.x, center.y + point.y};
        };
        geometry.addTriangle(
            rotate({13.0F, 0.0F}),
            rotate({-7.0F, -9.5F}),
            rotate({-7.0F, 9.5F}),
            BLACK
        );
    }
}

double coveredPixels(RenderTexture2D target, int lineAxis) {
    Image image = LoadImageFromTexture(target.texture);
    Color* pixels = LoadImageColors(image);
    double covered = 0.0;
    const int count = lineAxis == 1 ? kWidth : lineAxis == 2 ? kHeight : kWidth*kHeight;
    for (int index = 0; index < count; ++index) {
        const int pixel = lineAxis == 1 ? 48*kWidth + index
                        : lineAxis == 2 ? index*kWidth + 64 : index;
        covered += (255.0 - pixels[pixel].r) / 255.0;
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    return covered;
}

void checkCoverage(RenderTexture2D target) {
    arrow::StaticBoardGeometry geometry;
    for (bool dynamic : {false, true}) {
        for (float zoom : {0.139434F, 0.25F, 0.5F, 1.0F, 3.5F}) {
            for (int primitive = 0; primitive < 7; ++primitive) {
                geometry.begin();
                if (!dynamic) addPrimitive(geometry, primitive, zoom);
                geometry.finish();
                const bool line = primitive == 0 || primitive == 6;
                const double expected = line
                    ? 6.4 * zoom
                    : primitive == 1
                        ? kPi * (3.2 * zoom) * (3.2 * zoom)
                        : 0.5 * 20.0 * 19.0 * zoom * zoom;
                const double tolerance = line
                    ? 0.012
                    : std::max(0.035, expected * 0.0015);
                double minimum = 1e9;
                double maximum = 0.0;
                for (int phase = 0; phase <= 40; ++phase) {
                    const Camera2D camera{
                        {phase / 40.0F, phase / 53.0F}, {0.0F, 0.0F}, 0.0F, zoom
                    };
                    BeginTextureMode(target);
                    ClearBackground(WHITE);
                    BeginMode2D(camera);
                    if (dynamic) {
                        geometry.beginDynamic(zoom);
                        addPrimitive(geometry, primitive, zoom);
                        geometry.endDynamic();
                    } else {
                        geometry.draw(zoom);
                    }
                    EndMode2D();
                    EndTextureMode();
                    const double actual = coveredPixels(
                        target, primitive == 0 ? 1 : primitive == 6 ? 2 : 0
                    );
                    minimum = std::min(minimum, actual);
                    maximum = std::max(maximum, actual);
                    if (std::abs(actual - expected) > tolerance) {
                        std::cerr << "Coverage mismatch: dynamic=" << dynamic
                                  << " zoom=" << zoom << " primitive=" << primitive
                                  << " phase=" << phase << " expected=" << expected
                                  << " actual=" << actual << '\n';
                        throw std::runtime_error("Subpixel coverage changed with pixel alignment");
                    }
                }
                if (line && maximum - minimum > 0.02) {
                    throw std::runtime_error("Stroke weight changes during subpixel panning");
                }
            }
        }
    }

    // Cross the streaming batch boundary with clipped primitives. The final
    // visible line must retain the same shader, camera and pixel-size uniform.
    BeginTextureMode(target);
    ClearBackground(WHITE);
    BeginMode2D({{0.0F, 0.0F}, {0.0F, 0.0F}, 0.0F, 0.25F});
    geometry.beginDynamic(0.25F);
    for (int index = 0; index < 10000; ++index) {
        geometry.addCircle({-1000.0F, -1000.0F}, 3.2F, BLACK);
    }
    addPrimitive(geometry, 0, 0.25F);
    geometry.endDynamic();
    EndMode2D();
    EndTextureMode();
    if (std::abs(coveredPixels(target, 1) - 1.6) > 0.012) {
        throw std::runtime_error("Coverage was lost after a streaming batch flush");
    }
}

} // namespace

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kWidth, kHeight, "Arrow Puzzle rendering tests");
    const RenderTexture2D target = LoadRenderTexture(kWidth, kHeight);
    int result = 0;
    try {
        checkCoverage(target);
        std::cout << "Rendering tests passed: 2870 coverage samples and streaming batch flush.\n";
    } catch (const std::exception& error) {
        std::cerr << "Rendering test failure: " << error.what() << '\n';
        result = 1;
    }
    UnloadRenderTexture(target);
    CloseWindow();
    return result;
}
