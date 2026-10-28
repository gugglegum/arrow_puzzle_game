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

void checkDotRecolouring(RenderTexture2D target) {
    const RenderTexture2D reference = LoadRenderTexture(kWidth, kHeight);
    arrow::StaticBoardGeometry geometry;
    constexpr Color base{195, 202, 214, 255};
    constexpr Color highlighted{148, 158, 176, 255};
    constexpr Color difference{47, 44, 38, 255};
    for (float zoom : {0.05F, 0.139434F, 0.25F, 1.0F, 3.5F}) {
        for (int phase = 0; phase <= 8; ++phase) {
            const Camera2D camera{{phase / 8.0F, phase / 11.0F}, {}, 0.0F, zoom};
            const Vector2 center{64.0F / zoom, 48.0F / zoom};
            const auto render = [&](RenderTexture2D destination, bool overlay) {
                BeginTextureMode(destination);
                ClearBackground({248, 250, 253, 255});
                BeginMode2D(camera);
                geometry.beginDynamic(zoom);
                geometry.addCircle(center, 3.2F, overlay ? base : highlighted);
                geometry.endDynamic();
                if (overlay) {
                    geometry.beginDarkening(zoom);
                    geometry.addCircle(center, 3.2F, difference);
                    geometry.endDarkening();
                }
                // Normal blending must be restored for subsequent moving arrows.
                geometry.beginDynamic(zoom);
                geometry.addCircle({100.0F / zoom, 48.0F / zoom}, 3.2F, BLUE);
                geometry.endDynamic();
                EndMode2D();
                EndTextureMode();
            };
            render(reference, false);
            render(target, true);
            Image expected = LoadImageFromTexture(reference.texture);
            Image actual = LoadImageFromTexture(target.texture);
            Color* expectedPixels = LoadImageColors(expected);
            Color* actualPixels = LoadImageColors(actual);
            bool matches = true;
            for (int index = 0; index < kWidth * kHeight; ++index) {
                const Color a = actualPixels[index];
                const Color e = expectedPixels[index];
                matches &= std::abs(a.r - e.r) <= 1 && std::abs(a.g - e.g) <= 1 &&
                           std::abs(a.b - e.b) <= 1 && a.a == e.a;
            }
            UnloadImageColors(expectedPixels);
            UnloadImageColors(actualPixels);
            UnloadImage(expected);
            UnloadImage(actual);
            if (!matches) {
                throw std::runtime_error("Dot highlighting changed antialiasing or subsequent blending");
            }
        }
    }
    geometry.release();
    UnloadRenderTexture(reference);
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
        checkDotRecolouring(target);
        std::cout << "Rendering tests passed: 2870 coverage samples, streaming batch flush and 45 dot recolouring samples.\n";
    } catch (const std::exception& error) {
        std::cerr << "Rendering test failure: " << error.what() << '\n';
        result = 1;
    }
    UnloadRenderTexture(target);
    CloseWindow();
    return result;
}
