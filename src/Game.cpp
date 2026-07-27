#include "arrow/Game.hpp"

#include "arrow/Generator.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arrow {
namespace {

constexpr int kInitialWidth = 1100;
constexpr int kInitialHeight = 900;
constexpr float kCellSize = 34.0F;
constexpr float kBoardPadding = 34.0F;
constexpr float kHudHeight = 82.0F;
constexpr float kStepDuration = 0.055F;
constexpr float kReturnStepDuration = 0.035F;
constexpr float kCollisionPause = 0.22F;

constexpr Color kInk{22, 29, 48, 255};
constexpr Color kBlue{16, 132, 255, 255};
constexpr Color kRed{238, 67, 74, 255};
constexpr Color kSoft{226, 232, 241, 255};
constexpr Color kMuted{104, 115, 132, 255};

enum class Motion {
    Idle,
    Forward,
    CollisionPause,
    Returning
};

struct HistoryState {
    std::uint64_t tick{};
    std::vector<Cell> cells;
};

struct RuntimeArrow {
    std::vector<Cell> cells;
    std::vector<Cell> targetCells;
    std::vector<HistoryState> history;
    Cell direction{};
    Motion motion{Motion::Idle};
    float progress{};
    bool present{true};
};

struct ArrowHeadTriangle {
    Vector2 tip;
    Vector2 left;
    Vector2 right;
};

Vector2 cellCenter(Cell cell) {
    return {
        kBoardPadding + (static_cast<float>(cell.x) + 0.5F) * kCellSize,
        kBoardPadding + (static_cast<float>(cell.y) + 0.5F) * kCellSize
    };
}

Vector2 lerp(Vector2 from, Vector2 to, float amount) {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount
    };
}

float distanceToSegment(Vector2 point, Vector2 start, Vector2 end) {
    const Vector2 line{end.x - start.x, end.y - start.y};
    const Vector2 relative{point.x - start.x, point.y - start.y};
    const float lengthSquared = line.x * line.x + line.y * line.y;
    const float t = lengthSquared > 0.0F
        ? std::clamp(
              (relative.x * line.x + relative.y * line.y) / lengthSquared,
              0.0F,
              1.0F
          )
        : 0.0F;
    const Vector2 closest{
        start.x + line.x * t,
        start.y + line.y * t
    };
    return std::hypot(point.x - closest.x, point.y - closest.y);
}

std::vector<Vector2> displayPoints(const RuntimeArrow& value) {
    std::vector<Vector2> result;
    result.reserve(value.cells.size());
    const bool interpolating =
        !value.targetCells.empty() &&
        value.targetCells.size() == value.cells.size() &&
        (value.motion == Motion::Forward || value.motion == Motion::Returning);
    const float eased = value.progress * value.progress *
                        (3.0F - 2.0F * value.progress);

    for (std::size_t index = 0; index < value.cells.size(); ++index) {
        Vector2 point = cellCenter(value.cells[index]);
        if (interpolating) {
            point = lerp(point, cellCenter(value.targetCells[index]), eased);
        }
        result.push_back(point);
    }
    return result;
}

ArrowHeadTriangle arrowHeadTriangle(
    Vector2 head,
    Cell cellDirection
) {
    const Vector2 direction{
        static_cast<float>(cellDirection.x),
        static_cast<float>(cellDirection.y)
    };
    const Vector2 normal{-direction.y, direction.x};
    const Vector2 tip{
        head.x + direction.x * 13.0F,
        head.y + direction.y * 13.0F
    };
    const Vector2 base{
        head.x - direction.x * 7.0F,
        head.y - direction.y * 7.0F
    };
    return {
        tip,
        {base.x - normal.x * 9.5F, base.y - normal.y * 9.5F},
        {base.x + normal.x * 9.5F, base.y + normal.y * 9.5F}
    };
}

void drawArrow(const RuntimeArrow& value) {
    const std::vector<Vector2> points = displayPoints(value);
    if (points.empty()) {
        return;
    }

    Color color = kInk;
    if (value.motion == Motion::Forward) {
        color = kBlue;
    } else if (
        value.motion == Motion::CollisionPause ||
        value.motion == Motion::Returning
    ) {
        color = kRed;
    }

    constexpr float thickness = 6.4F;
    for (std::size_t index = 1; index < points.size(); ++index) {
        DrawLineEx(points[index - 1], points[index], thickness, color);
        DrawCircleV(points[index], thickness * 0.5F, color);
    }
    DrawCircleV(points.front(), thickness * 0.5F, color);

    const ArrowHeadTriangle head = arrowHeadTriangle(
        points.front(),
        value.direction
    );
    DrawTriangle(
        head.tip,
        head.left,
        head.right,
        color
    );
}

bool allOutside(const RuntimeArrow& value, const Board& board) {
    return std::ranges::none_of(value.cells, [&board](Cell cell) {
        return board.isInside(cell);
    });
}

bool containsCell(const std::vector<Cell>& cells, Cell cell) {
    return std::ranges::find(cells, cell) != cells.end();
}

bool cellsOverlap(
    const std::vector<Cell>& lhs,
    const std::vector<Cell>& rhs
) {
    return std::ranges::any_of(lhs, [&rhs](Cell cell) {
        return containsCell(rhs, cell);
    });
}

std::vector<Cell> advancedCells(const RuntimeArrow& value) {
    std::vector<Cell> next = value.cells;
    if (next.empty()) {
        return next;
    }
    for (std::size_t index = next.size() - 1; index > 0; --index) {
        next[index] = value.cells[index - 1];
    }
    next[0] = value.cells[0] + value.direction;
    return next;
}

bool collidesWithSelf(
    const std::vector<Cell>& proposed
) {
    if (proposed.size() < 2) {
        return false;
    }
    return std::find(
               proposed.begin() + 1,
               proposed.end(),
               proposed.front()
           ) != proposed.end();
}

void drawHeart(Vector2 center, bool filled) {
    const Color color = filled ? kRed : kSoft;
    DrawCircleV({center.x - 7.0F, center.y - 3.0F}, 8.0F, color);
    DrawCircleV({center.x + 7.0F, center.y - 3.0F}, 8.0F, color);
    DrawTriangle(
        {center.x - 14.0F, center.y},
        {center.x, center.y + 18.0F},
        {center.x + 14.0F, center.y},
        color
    );
}

std::optional<std::size_t> hitTest(
    Vector2 world,
    const std::vector<RuntimeArrow>& arrows
) {
    constexpr float tolerance = 12.0F;

    std::optional<std::size_t> bestHead;
    float bestHeadDistance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < arrows.size(); ++index) {
        const RuntimeArrow& value = arrows[index];
        if (!value.present || value.motion != Motion::Idle ||
            value.cells.empty()) {
            continue;
        }
        const Vector2 center = cellCenter(value.cells.front());
        const ArrowHeadTriangle head = arrowHeadTriangle(
            center,
            value.direction
        );
        if (!CheckCollisionPointTriangle(
                world,
                head.tip,
                head.left,
                head.right
            )) {
            continue;
        }
        const float distance = std::hypot(
            world.x - center.x,
            world.y - center.y
        );
        if (distance < bestHeadDistance) {
            bestHeadDistance = distance;
            bestHead = index;
        }
    }
    if (bestHead.has_value()) {
        return bestHead;
    }

    std::optional<std::size_t> best;
    float bestDistance = tolerance;

    for (std::size_t index = 0; index < arrows.size(); ++index) {
        const RuntimeArrow& value = arrows[index];
        if (!value.present || value.motion != Motion::Idle) {
            continue;
        }
        const std::vector<Vector2> points = displayPoints(value);
        for (std::size_t point = 1; point < points.size(); ++point) {
            const float distance = distanceToSegment(
                world,
                points[point - 1],
                points[point]
            );
            if (distance < bestDistance) {
                bestDistance = distance;
                best = index;
            }
        }
    }
    return best;
}

std::string environmentValue(const char* name) {
#if defined(_MSC_VER)
    char* rawValue = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&rawValue, &length, name) != 0 ||
        rawValue == nullptr) {
        return {};
    }
    std::string value(rawValue);
    std::free(rawValue);
    return value;
#else
    const char* rawValue = std::getenv(name);
    return rawValue == nullptr ? std::string{} : std::string{rawValue};
#endif
}

std::optional<std::uint32_t> seedFromEnvironment() {
    const std::string value = environmentValue("ARROWPUZZLE_SEED");
    if (value.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(
        value.c_str(),
        &end,
        10
    );
    if (end == value.c_str() || *end != '\0' ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(parsed);
}

} // namespace

int Game::run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kInitialWidth, kInitialHeight, "Arrow Puzzle");
    SetWindowMinSize(720, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    std::uint32_t seed = seedFromEnvironment().value_or(
        static_cast<std::uint32_t>(std::time(nullptr))
    );
    const auto generateDefaultBoard = [](std::uint32_t boardSeed) {
        return generateBoard({20, 20, 2, 30, boardSeed});
    };
    Board board = generateDefaultBoard(seed);
    std::vector<RuntimeArrow> arrows;
    bool forwardStepActive = false;
    float forwardStepProgress = 0.0F;
    std::uint64_t forwardTick = 0;
    bool recoveryClockActive = false;
    bool recoveryStepActive = false;
    float recoveryStepProgress = 0.0F;
    std::uint64_t recoveryTick = 0;
    auto resetRuntime = [&]() {
        arrows.clear();
        arrows.reserve(board.arrows().size());
        for (const Arrow& source : board.arrows()) {
            arrows.push_back({
                source.cells,
                {},
                {},
                source.direction,
                Motion::Idle,
                0.0F,
                true
            });
        }
        forwardStepActive = false;
        forwardStepProgress = 0.0F;
        forwardTick = 0;
        recoveryClockActive = false;
        recoveryStepActive = false;
        recoveryStepProgress = 0.0F;
        recoveryTick = 0;
    };
    resetRuntime();

    int lives = 3;
    bool closeRequested = false;
    const std::string automatedScreenshot =
        environmentValue("ARROWPUZZLE_SCREENSHOT");
    int renderedFrames = 0;

    const float boardWidth =
        kBoardPadding * 2.0F + board.columns() * kCellSize;
    const float boardHeight =
        kBoardPadding * 2.0F + board.rows() * kCellSize;
    Camera2D camera{
        {
            static_cast<float>(GetScreenWidth()) * 0.5F,
            kHudHeight +
                (static_cast<float>(GetScreenHeight()) - kHudHeight) * 0.5F
        },
        {boardWidth * 0.5F, boardHeight * 0.5F},
        0.0F,
        std::min(
            (GetScreenWidth() - 60.0F) / boardWidth,
            (GetScreenHeight() - kHudHeight - 50.0F) / boardHeight
        )
    };

    while (!WindowShouldClose() && !closeRequested) {
        const float delta = std::min(GetFrameTime(), 0.05F);

        if (IsKeyPressed(KEY_F11) ||
            (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
            ToggleFullscreen();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (IsWindowFullscreen()) {
                ToggleFullscreen();
            } else {
                closeRequested = true;
            }
        }
        if (IsKeyPressed(KEY_F12)) {
            TakeScreenshot("arrow-puzzle-screenshot.png");
        }

        camera.offset = {
            static_cast<float>(GetScreenWidth()) * 0.5F,
            kHudHeight +
                (static_cast<float>(GetScreenHeight()) - kHudHeight) * 0.5F
        };

        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0F) {
            const Vector2 mouse = GetMousePosition();
            const Vector2 before = GetScreenToWorld2D(mouse, camera);
            camera.zoom = std::clamp(
                camera.zoom * std::pow(1.12F, wheel),
                0.25F,
                3.5F
            );
            const Vector2 after = GetScreenToWorld2D(mouse, camera);
            camera.target.x += before.x - after.x;
            camera.target.y += before.y - after.y;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            const Vector2 movement = GetMouseDelta();
            camera.target.x -= movement.x / camera.zoom;
            camera.target.y -= movement.y / camera.zoom;
        }

        const bool anyArrowInMotion = std::ranges::any_of(
            arrows,
            [](const RuntimeArrow& value) {
                return value.present && value.motion != Motion::Idle;
            }
        );
        const bool anyArrowRecovering = std::ranges::any_of(
            arrows,
            [](const RuntimeArrow& value) {
                return value.present &&
                       (value.motion == Motion::CollisionPause ||
                        value.motion == Motion::Returning);
            }
        );
        if (IsKeyPressed(KEY_R) && !anyArrowInMotion) {
            ++seed;
            board = generateDefaultBoard(seed);
            resetRuntime();
            lives = 3;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            lives > 0 && !anyArrowRecovering) {
            const Vector2 world = GetScreenToWorld2D(
                GetMousePosition(),
                camera
            );
            const std::optional<std::size_t> launched =
                hitTest(world, arrows);
            if (launched.has_value()) {
                RuntimeArrow& value = arrows[*launched];
                value.motion = Motion::Forward;
                value.progress = 0.0F;
                value.history.clear();
                value.targetCells.clear();
            }
        }

        const bool hasForwardArrow = std::ranges::any_of(
            arrows,
            [](const RuntimeArrow& value) {
                return value.present && value.motion == Motion::Forward;
            }
        );

        // Recovery waits until every still-safe forward arrow has either left
        // the board or stopped as well. All failed arrows then rewind through
        // the exact accepted global states, identified by their logical tick.
        // This preserves safe tail-following without allowing red animations
        // to change occupancy in the middle of a blue step.
        for (RuntimeArrow& value : arrows) {
            if (!value.present) {
                continue;
            }
            if (value.motion == Motion::CollisionPause) {
                if (hasForwardArrow) {
                    value.progress = 0.0F;
                    continue;
                }
                value.progress += delta / kCollisionPause;
                if (value.progress >= 1.0F) {
                    if (value.history.empty()) {
                        value.motion = Motion::Idle;
                        value.progress = 0.0F;
                    } else {
                        value.motion = Motion::Returning;
                        value.progress = 0.0F;
                        value.targetCells.clear();
                    }
                }
            }
        }

        const bool hasReturningArrow = std::ranges::any_of(
            arrows,
            [](const RuntimeArrow& value) {
                return value.present && value.motion == Motion::Returning;
            }
        );
        if (hasReturningArrow && !recoveryClockActive) {
            recoveryClockActive = true;
            recoveryStepActive = false;
            recoveryStepProgress = 0.0F;
            recoveryTick = forwardTick;
        }

        if (recoveryStepActive) {
            recoveryStepProgress += delta / kReturnStepDuration;
            const float displayedProgress =
                std::min(recoveryStepProgress, 1.0F);
            for (RuntimeArrow& value : arrows) {
                if (value.motion == Motion::Returning &&
                    !value.targetCells.empty()) {
                    value.progress = displayedProgress;
                }
            }

            if (recoveryStepProgress >= 1.0F) {
                for (RuntimeArrow& value : arrows) {
                    if (value.motion != Motion::Returning ||
                        value.targetCells.empty()) {
                        continue;
                    }
                    value.cells = value.targetCells;
                    value.targetCells.clear();
                    value.progress = 0.0F;
                    value.history.pop_back();
                    if (value.history.empty()) {
                        value.motion = Motion::Idle;
                    }
                }
                if (recoveryTick > 0) {
                    --recoveryTick;
                }
                recoveryStepActive = false;
                recoveryStepProgress = 0.0F;
            }
        }

        if (recoveryClockActive && !recoveryStepActive) {
            bool scheduled = false;
            while (!scheduled && recoveryTick > 0) {
                const std::uint64_t targetTick = recoveryTick - 1;
                for (RuntimeArrow& value : arrows) {
                    if (value.motion != Motion::Returning ||
                        value.history.empty() ||
                        value.history.back().tick != targetTick) {
                        continue;
                    }
                    value.targetCells = value.history.back().cells;
                    value.progress = 0.0F;
                    scheduled = true;
                }
                if (!scheduled) {
                    --recoveryTick;
                }
            }
            recoveryStepActive = scheduled;
            recoveryStepProgress = 0.0F;

            const bool stillReturning = std::ranges::any_of(
                arrows,
                [](const RuntimeArrow& value) {
                    return value.present &&
                           value.motion == Motion::Returning;
                }
            );
            if (!stillReturning) {
                recoveryClockActive = false;
                recoveryTick = 0;
            }
        }

        if (forwardStepActive) {
            forwardStepProgress += delta / kStepDuration;
            const float displayedProgress =
                std::min(forwardStepProgress, 1.0F);
            for (RuntimeArrow& value : arrows) {
                if (value.motion == Motion::Forward &&
                    !value.targetCells.empty()) {
                    value.progress = displayedProgress;
                }
            }

            if (forwardStepProgress >= 1.0F) {
                bool committedStep = false;
                for (RuntimeArrow& value : arrows) {
                    if (value.motion != Motion::Forward ||
                        value.targetCells.empty()) {
                        continue;
                    }
                    value.cells = value.targetCells;
                    value.targetCells.clear();
                    value.progress = 0.0F;
                    committedStep = true;

                    if (allOutside(value, board)) {
                        value.present = false;
                        value.history.clear();
                        value.motion = Motion::Idle;
                    }
                }
                if (committedStep) {
                    ++forwardTick;
                }
                forwardStepActive = false;
                forwardStepProgress = 0.0F;
            }
        }

        if (!forwardStepActive) {
            std::vector<std::vector<Cell>> proposals(arrows.size());
            std::vector<std::uint8_t> moving(arrows.size(), 0);
            std::vector<std::uint8_t> colliding(arrows.size(), 0);
            bool hasMovingArrow = false;

            for (std::size_t index = 0; index < arrows.size(); ++index) {
                const RuntimeArrow& value = arrows[index];
                if (!value.present || value.motion != Motion::Forward) {
                    continue;
                }
                moving[index] = 1;
                hasMovingArrow = true;
                proposals[index] = advancedCells(value);
                colliding[index] =
                    proposals[index].empty() ||
                    collidesWithSelf(proposals[index]);
            }

            // Compare moving arrows by their simultaneous proposed states.
            // A tail cell is therefore free during the same step in which it
            // leaves that cell.  Head swaps need a separate swept-edge test.
            for (std::size_t first = 0; first < arrows.size(); ++first) {
                if (!moving[first]) {
                    continue;
                }
                for (std::size_t second = first + 1;
                     second < arrows.size();
                     ++second) {
                    if (!moving[second]) {
                        continue;
                    }
                    if (proposals[first].empty() ||
                        proposals[second].empty()) {
                        colliding[first] = 1;
                        colliding[second] = 1;
                        continue;
                    }
                    const bool sameTarget =
                        proposals[first].front() ==
                        proposals[second].front();
                    const bool headSwap =
                        proposals[first].front() ==
                            arrows[second].cells.front() &&
                        proposals[second].front() ==
                            arrows[first].cells.front();
                    if (sameTarget || headSwap ||
                        cellsOverlap(
                            proposals[first],
                            proposals[second]
                        )) {
                        colliding[first] = 1;
                        colliding[second] = 1;
                    }
                }
            }

            // Non-forward arrows are stationary obstacles. Returning cannot
            // coexist with forward motion, but reserve its interpolation
            // target defensively as well.
            for (std::size_t movingIndex = 0;
                 movingIndex < arrows.size();
                 ++movingIndex) {
                if (!moving[movingIndex]) {
                    continue;
                }
                for (std::size_t obstacle = 0;
                     obstacle < arrows.size();
                     ++obstacle) {
                    if (obstacle == movingIndex ||
                        moving[obstacle] ||
                        !arrows[obstacle].present) {
                        continue;
                    }
                    const RuntimeArrow& obstacleArrow = arrows[obstacle];
                    if (cellsOverlap(
                            proposals[movingIndex],
                            obstacleArrow.cells
                        ) ||
                        (!obstacleArrow.targetCells.empty() &&
                         cellsOverlap(
                             proposals[movingIndex],
                             obstacleArrow.targetCells
                         ))) {
                        colliding[movingIndex] = 1;
                    }
                }
            }

            // A colliding arrow does not commit its proposal, so its old tail
            // is not actually vacated. Propagate that fact to movers which
            // had provisionally planned to enter such a tail cell.
            bool collisionExpanded = true;
            while (collisionExpanded) {
                collisionExpanded = false;
                for (std::size_t movingIndex = 0;
                     movingIndex < arrows.size();
                     ++movingIndex) {
                    if (!moving[movingIndex] || colliding[movingIndex]) {
                        continue;
                    }
                    for (std::size_t obstacle = 0;
                         obstacle < arrows.size();
                         ++obstacle) {
                        if (!moving[obstacle] || !colliding[obstacle] ||
                            obstacle == movingIndex) {
                            continue;
                        }
                        if (cellsOverlap(
                                proposals[movingIndex],
                                arrows[obstacle].cells
                            )) {
                            colliding[movingIndex] = 1;
                            collisionExpanded = true;
                            break;
                        }
                    }
                }
            }

            const bool collisionBatch =
                std::ranges::any_of(colliding, [](std::uint8_t value) {
                    return value != 0;
                });
            if (collisionBatch) {
                lives = std::max(0, lives - 1);
            }

            if (hasMovingArrow) {
                for (std::size_t index = 0;
                     index < arrows.size();
                     ++index) {
                    if (!moving[index]) {
                        continue;
                    }
                    RuntimeArrow& value = arrows[index];
                    if (colliding[index]) {
                        value.motion = Motion::CollisionPause;
                        value.progress = 0.0F;
                        value.targetCells.clear();
                    } else {
                        value.history.push_back({
                            forwardTick,
                            value.cells
                        });
                        value.targetCells = std::move(proposals[index]);
                        value.progress = 0.0F;
                        forwardStepActive = true;
                    }
                }
                forwardStepProgress = 0.0F;
            }
        }

        const int remaining = static_cast<int>(std::ranges::count_if(
            arrows,
            [](const RuntimeArrow& value) { return value.present; }
        ));

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(kHudHeight), RAYWHITE);
        DrawLine(
            0,
            static_cast<int>(kHudHeight),
            GetScreenWidth(),
            static_cast<int>(kHudHeight),
            kSoft
        );
        DrawText("ARROW PUZZLE", 28, 20, 26, kInk);
        const std::string countText = "ARROWS  " + std::to_string(remaining);
        DrawText(
            countText.c_str(),
            GetScreenWidth() / 2 - MeasureText(countText.c_str(), 22) / 2,
            24,
            22,
            kMuted
        );
        for (int index = 0; index < 3; ++index) {
            drawHeart(
                {
                    static_cast<float>(GetScreenWidth() - 128 + index * 38),
                    34.0F
                },
                index < lives
            );
        }

        BeginMode2D(camera);
        DrawRectangleRounded(
            {0.0F, 0.0F, boardWidth, boardHeight},
            0.035F,
            8,
            {248, 250, 253, 255}
        );
        DrawRectangleRoundedLinesEx(
            {0.0F, 0.0F, boardWidth, boardHeight},
            0.035F,
            8,
            2.0F,
            kSoft
        );
        for (const RuntimeArrow& value : arrows) {
            if (value.present) {
                drawArrow(value);
            }
        }
        EndMode2D();

        if (remaining == 0) {
            const char* text = "FIELD CLEARED!  Press R for a new puzzle";
            const int fontSize = 28;
            const int width = MeasureText(text, fontSize);
            DrawRectangleRounded(
                {
                    GetScreenWidth() * 0.5F - width * 0.5F - 24.0F,
                    GetScreenHeight() - 84.0F,
                    static_cast<float>(width + 48),
                    52.0F
                },
                0.25F,
                8,
                Fade(RAYWHITE, 0.94F)
            );
            DrawText(
                text,
                GetScreenWidth() / 2 - width / 2,
                GetScreenHeight() - 70,
                fontSize,
                kBlue
            );
        } else if (lives == 0) {
            const char* text = "NO LIVES LEFT  -  Press R to restart";
            const int fontSize = 26;
            const int width = MeasureText(text, fontSize);
            DrawText(
                text,
                GetScreenWidth() / 2 - width / 2,
                GetScreenHeight() - 48,
                fontSize,
                kRed
            );
        } else {
            const char* help =
                "LMB launch   Wheel zoom   MMB pan   F11 fullscreen   R new";
            DrawText(
                help,
                GetScreenWidth() / 2 - MeasureText(help, 17) / 2,
                GetScreenHeight() - 30,
                17,
                kMuted
            );
        }

        EndDrawing();

        if (!automatedScreenshot.empty() && ++renderedFrames == 30) {
            TakeScreenshot(automatedScreenshot.c_str());
            closeRequested = true;
        }
    }

    CloseWindow();
    return 0;
}

} // namespace arrow
