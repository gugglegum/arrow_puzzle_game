#include "StaticBoardGeometry.hpp"
#include "BoardPalette.hpp"
#include "ArrowStroke.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
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
            const int rotation = primitive >= 7 ? primitive - 7 : primitive - 2;
            for (int turn = 0; turn < rotation; ++turn) {
                point = {-point.y, point.x};
            }
            return Vector2{center.x + point.x, center.y + point.y};
        };
        geometry.addTriangle(
            rotate({13.0F, 0.0F}),
            rotate({-7.0F, -9.5F}),
            rotate({-7.0F, 9.5F}),
            BLACK,
            primitive >= 7
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
        for (float zoom : {0.05F, 0.139434F, 0.25F, 0.5F, 1.0F, 3.5F}) {
            for (int primitive = 0; primitive < 11; ++primitive) {
                geometry.begin();
                if (!dynamic) addPrimitive(geometry, primitive, zoom);
                geometry.finish();
                if (!dynamic && geometry.gpuBytes() != 108) {
                    throw std::runtime_error("Primitive vertex memory changed");
                }
                const bool line = primitive == 0 || primitive == 6;
                const double side = std::hypot(20.0, 9.5);
                const double roundedArea = 190.0 - 1.6*1.6 *
                    (20.0/9.5 + 2.0*(side+9.5)/20.0 - kPi);
                const double expected = line
                    ? 6.4 * zoom
                    : primitive == 1
                        ? kPi * (3.2 * zoom) * (3.2 * zoom)
                        : (primitive >= 7 ? roundedArea : 190.0) * zoom * zoom;
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

void checkRoundedStrokes(RenderTexture2D target) {
    arrow::StaticBoardGeometry geometry;
    const double area = (68.0 - 16.0 + kPi*4.0)*6.4 + kPi*3.2*3.2;
    for (bool dynamic : {false, true}) {
        for (float zoom : {0.05F, 0.139434F, 0.25F, 0.5F, 1.0F, 2.0F}) {
            for (int turn = 0; turn < 4; ++turn) {
                std::array<Vector2,3> points{{{-17,-17}, {17,-17}, {17,17}}};
                for (Vector2& point : points) {
                    for (int rotate = 0; rotate < turn; ++rotate) point = {-point.y,point.x};
                    point.x += 64.0F/zoom;
                    point.y += 48.0F/zoom;
                }
                const auto add = [&] {
                    arrow::addArrowStroke(geometry, points.size(),
                        [&points](std::size_t index) { return points[index]; }, 6.4F, BLACK);
                };
                geometry.begin();
                if (!dynamic) add();
                geometry.finish();
                // Caps are integrated into the runs. The bend still uses one
                // quad and the stroke needs fewer bytes than the old five.
                if (!dynamic && geometry.gpuBytes() > 5 * 108) {
                    throw std::runtime_error("Rounded strokes increased geometry memory");
                }
                double minimum = 1e9;
                double maximum = 0.0;
                for (int phase = 0; phase <= 40; ++phase) {
                    BeginTextureMode(target);
                    ClearBackground(WHITE);
                    BeginMode2D({{phase/40.0F, phase/53.0F}, {0,0}, 0, zoom});
                    if (dynamic) {
                        geometry.beginDynamic(zoom);
                        add();
                        geometry.endDynamic();
                    } else geometry.draw(zoom);
                    EndMode2D();
                    EndTextureMode();
                    const double actual = coveredPixels(target, 0);
                    const double expected = area*zoom*zoom;
                    minimum = std::min(minimum, actual);
                    maximum = std::max(maximum, actual);
                    if (std::abs(actual-expected) > std::max(0.12, expected*0.006)) {
                        std::cerr << "Bend coverage mismatch: dynamic=" << dynamic
                                  << " zoom=" << zoom << " turn=" << turn << " phase=" << phase
                                  << " expected=" << expected << " actual=" << actual << '\n';
                        throw std::runtime_error("Rounded bend coverage changed with pixel alignment");
                    }
                }
                if (maximum-minimum > std::max(0.15, area*zoom*zoom*0.008)) {
                    throw std::runtime_error("Rounded stroke weight changes during panning");
                }
            }
        }
    }
}

void checkDotRecolouring(RenderTexture2D target, arrow::BoardTheme theme) {
    const RenderTexture2D reference = LoadRenderTexture(kWidth, kHeight);
    arrow::StaticBoardGeometry geometry;
    const auto& palette = arrow::boardPalette(theme);
    const Color base = palette.dot;
    const Color highlighted = palette.guide;
    const Color difference{
        static_cast<unsigned char>(std::abs(base.r - highlighted.r)),
        static_cast<unsigned char>(std::abs(base.g - highlighted.g)),
        static_cast<unsigned char>(std::abs(base.b - highlighted.b)), 255};
    for (float zoom : {0.05F, 0.139434F, 0.25F, 1.0F, 3.5F}) {
        for (int phase = 0; phase <= 8; ++phase) {
            const Camera2D camera{{phase / 8.0F, phase / 11.0F}, {}, 0.0F, zoom};
            const Vector2 center{64.0F / zoom, 48.0F / zoom};
            const auto render = [&](RenderTexture2D destination, bool overlay) {
                BeginTextureMode(destination);
                ClearBackground(palette.surface);
                BeginMode2D(camera);
                geometry.beginDynamic(zoom);
                geometry.addCircle(center, 3.2F, overlay ? base : highlighted);
                geometry.endDynamic();
                if (overlay) {
                    if (palette.brightenGuides) geometry.beginBrightening(zoom);
                    else geometry.beginDarkening(zoom);
                    geometry.addCircle(center, 3.2F, difference);
                    if (palette.brightenGuides) geometry.endBrightening();
                    else geometry.endDarkening();
                }
                // Normal blending must be restored for subsequent moving arrows.
                geometry.beginDynamic(zoom);
                geometry.addCircle({100.0F / zoom, 48.0F / zoom}, 3.2F, palette.flight);
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

void checkShortTails(RenderTexture2D target) {
    const RenderTexture2D reference = LoadRenderTexture(kWidth,kHeight);
    arrow::StaticBoardGeometry geometry;
    for (float zoom : {0.05F,0.139434F,1.0F,3.5F}) {
        for (int turn = 0; turn < 4; ++turn) {
            for (float tail : {0.002F,0.1F,1.0F,6.4F,15.99F,16.01F,33.99F}) {
                // Includes a collinear cell and bends whose inner radius falls
                // through zero as flight/rewind trims the last segment.
                std::array<Vector2,4> points{{{-16,-8},{0,-8},{16,-8},{16,-8+tail}}};
                for (Vector2& point : points) {
                    for (int rotate = 0; rotate < turn; ++rotate) point = {-point.y,point.x};
                    point.x += 64.0F/zoom;
                    point.y += 48.0F/zoom;
                }
                const auto add = [&] {
                    arrow::addArrowStroke(geometry,points.size(),
                        [&points](std::size_t index) { return points[index]; },6.4F,BLACK);
                };
                geometry.begin();
                add();
                geometry.finish();
                const auto bytes = geometry.gpuBytes();
                if (bytes != 3*108) throw std::runtime_error("Straight cells were not merged");
                for (float phase : {0.0F,0.33F,0.66F}) {
                    const Camera2D camera{{phase,phase*0.7F},{0,0},0,zoom};
                    for (bool dynamic : {false,true}) {
                        BeginTextureMode(dynamic ? target : reference);
                        ClearBackground(WHITE);
                        BeginMode2D(camera);
                        if (dynamic) {
                            geometry.beginDynamic(zoom);
                            add();
                            geometry.endDynamic();
                        } else geometry.draw(zoom);
                        EndMode2D();
                        EndTextureMode();
                    }
                    Image expected = LoadImageFromTexture(reference.texture);
                    Image actual = LoadImageFromTexture(target.texture);
                    Color* expectedPixels = LoadImageColors(expected);
                    Color* actualPixels = LoadImageColors(actual);
                    bool matches = true;
                    for (int pixel=0; pixel<kWidth*kHeight; ++pixel) {
                        if (std::abs(int(expectedPixels[pixel].r)-int(actualPixels[pixel].r)) > 1) {
                            matches = false;
                            break;
                        }
                    }
                    UnloadImageColors(expectedPixels);
                    UnloadImageColors(actualPixels);
                    UnloadImage(expected);
                    UnloadImage(actual);
                    if (!matches || geometry.gpuBytes() != bytes) {
                        throw std::runtime_error("Moving/rewinding short tail differs from cached geometry");
                    }
                }
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
        checkRoundedStrokes(target);
        checkShortTails(target);
        for (const auto theme : {arrow::BoardTheme::Light, arrow::BoardTheme::Dark}) checkDotRecolouring(target, theme);
        std::cout << "Rendering tests passed: 5412 primitive and 1968 rounded-stroke coverage samples, 336 short-tail cached/streamed comparisons, bounded geometry memory, streaming batch flush and 90 dot recolouring samples across both themes.\n";
    } catch (const std::exception& error) {
        std::cerr << "Rendering test failure: " << error.what() << '\n';
        result = 1;
    }
    UnloadRenderTexture(target);
    CloseWindow();
    return result;
}
