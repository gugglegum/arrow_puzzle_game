#include "arrow/Game.hpp"

#include "arrow/Generator.hpp"
#include "arrow/MotionResolver.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <optional>
#include <span>
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
constexpr float kReturnStepDuration = 0.035F;
constexpr float kCollisionPause = 0.22F;
constexpr float kMovingCollisionPause = 0.5F;
constexpr float kInitialForwardSpeed = 2.25F;
constexpr float kForwardAcceleration = 22.0F;
constexpr float kGridDotRadius = 1.9F;

constexpr Color kInk{22, 29, 48, 255};
constexpr Color kBlue{16, 132, 255, 255};
constexpr Color kRed{238, 67, 74, 255};
constexpr Color kSoft{226, 232, 241, 255};
constexpr Color kMuted{104, 115, 132, 255};
constexpr Color kGridDot{220, 225, 234, 255};

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
    bool failed{};
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

float distance(Vector2 from, Vector2 to) {
    return std::hypot(to.x - from.x, to.y - from.y);
}

void appendDistinct(
    std::vector<Vector2>& points,
    Vector2 point
) {
    constexpr float epsilon = 0.001F;
    if (points.empty() ||
        distance(points.back(), point) > epsilon) {
        points.push_back(point);
    }
}

void trimPolylineTail(
    std::vector<Vector2>& points,
    float trimDistance
) {
    constexpr float epsilon = 0.001F;
    while (trimDistance > epsilon && points.size() >= 2) {
        const Vector2 previous = points[points.size() - 2];
        const Vector2 tail = points.back();
        const float segmentLength = distance(previous, tail);
        if (segmentLength <= epsilon) {
            points.pop_back();
            continue;
        }
        if (trimDistance >= segmentLength - epsilon) {
            trimDistance -= segmentLength;
            points.pop_back();
            continue;
        }
        points.back() = lerp(
            tail,
            previous,
            trimDistance / segmentLength
        );
        trimDistance = 0.0F;
    }
}

std::vector<Vector2> forwardPolyline(
    std::span<const Cell> cells,
    Cell direction,
    float progress
) {
    std::vector<Vector2> result;
    if (cells.empty()) {
        return result;
    }

    progress = std::clamp(progress, 0.0F, 1.0F);
    const Vector2 head = cellCenter(cells.front());
    const Vector2 movingHead{
        head.x + static_cast<float>(direction.x) *
                     kCellSize * progress,
        head.y + static_cast<float>(direction.y) *
                     kCellSize * progress
    };
    result.reserve(cells.size() + 1);
    appendDistinct(result, movingHead);
    for (Cell cell : cells) {
        appendDistinct(result, cellCenter(cell));
    }
    trimPolylineTail(result, kCellSize * progress);
    return result;
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
    const bool interpolating =
        !value.targetCells.empty() &&
        value.targetCells.size() == value.cells.size();
    if (value.motion == Motion::Forward && interpolating) {
        return forwardPolyline(
            value.cells,
            value.direction,
            value.progress
        );
    }
    if (value.motion == Motion::Returning && interpolating) {
        return forwardPolyline(
            value.targetCells,
            value.direction,
            1.0F - value.progress
        );
    }

    std::vector<Vector2> result;
    result.reserve(value.cells.size());
    for (Cell cell : value.cells) {
        result.push_back(cellCenter(cell));
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

    Color color = value.failed ? kRed : kInk;
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

std::optional<std::uint32_t> unsignedFromEnvironment(
    const char* name
) {
    const std::string value = environmentValue(name);
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

    std::uint32_t seed = unsignedFromEnvironment(
        "ARROWPUZZLE_SEED"
    ).value_or(
        static_cast<std::uint32_t>(std::time(nullptr))
    );
    const auto generateDefaultBoard = [](std::uint32_t boardSeed) {
        return generateBoard({20, 20, 2, 30, boardSeed});
    };
    Board board = generateDefaultBoard(seed);
    std::vector<RuntimeArrow> arrows;
    std::vector<std::uint8_t> movingContacts;
    bool forwardStepActive = false;
    float forwardStepProgress = 0.0F;
    bool forwardMovingContactPending = false;
    bool forwardSoftPenaltyPending = false;
    float movingCollisionPauseRemaining = 0.0F;
    float forwardSpeed = 0.0F;
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
                true,
                false
            });
        }
        movingContacts.assign(arrows.size() * arrows.size(), 0);
        forwardStepActive = false;
        forwardStepProgress = 0.0F;
        forwardMovingContactPending = false;
        forwardSoftPenaltyPending = false;
        movingCollisionPauseRemaining = 0.0F;
        forwardSpeed = 0.0F;
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
    const int automatedScreenshotFrame = static_cast<int>(std::max(
        std::uint32_t{1},
        unsignedFromEnvironment(
            "ARROWPUZZLE_SCREENSHOT_FRAME"
        ).value_or(30)
    ));
    if (!automatedScreenshot.empty()) {
        const std::size_t removeCount = std::min(
            static_cast<std::size_t>(
                unsignedFromEnvironment(
                    "ARROWPUZZLE_SCREENSHOT_REMOVE"
                ).value_or(0)
            ),
            arrows.size()
        );
        for (std::size_t index = 0; index < removeCount; ++index) {
            arrows[index].present = false;
        }
        const std::optional<std::uint32_t> autoLaunch =
            unsignedFromEnvironment(
                "ARROWPUZZLE_SCREENSHOT_LAUNCH"
            );
        if (autoLaunch.has_value() &&
            *autoLaunch < arrows.size() &&
            arrows[*autoLaunch].present) {
            arrows[*autoLaunch].motion = Motion::Forward;
        }
    }
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

    const auto hasForwardMotion = [&arrows]() {
        return std::ranges::any_of(
            arrows,
            [](const RuntimeArrow& value) {
                return value.present &&
                       value.motion == Motion::Forward;
            }
        );
    };

    const auto setForwardProgress = [&arrows](float progress) {
        for (RuntimeArrow& value : arrows) {
            if (value.motion == Motion::Forward &&
                !value.targetCells.empty()) {
                value.progress = progress;
            }
        }
    };

    const auto commitForwardStep = [&]() {
        bool committed = false;
        for (RuntimeArrow& value : arrows) {
            if (value.motion != Motion::Forward ||
                value.targetCells.empty()) {
                continue;
            }
            value.cells = std::move(value.targetCells);
            value.targetCells.clear();
            value.progress = 0.0F;
            committed = true;

            if (allOutside(value, board)) {
                value.present = false;
                value.history.clear();
                value.motion = Motion::Idle;
            }
        }
        if (committed) {
            ++forwardTick;
        }
        forwardStepActive = false;
        forwardStepProgress = 0.0F;
    };

    const auto prepareForwardStep = [&]() {
        std::vector<std::vector<Cell>> proposals(arrows.size());
        std::vector<std::uint8_t> moving(arrows.size(), 0);
        std::vector<MotionBodyView> bodies;
        bodies.reserve(arrows.size());
        bool hasMovingArrow = false;

        for (std::size_t index = 0; index < arrows.size(); ++index) {
            const RuntimeArrow& value = arrows[index];
            if (!value.present || value.motion != Motion::Forward) {
                continue;
            }
            moving[index] = 1;
            hasMovingArrow = true;
            proposals[index] = advancedCells(value);
        }

        for (std::size_t index = 0; index < arrows.size(); ++index) {
            const RuntimeArrow& value = arrows[index];
            const bool isMoving = moving[index] != 0;
            const std::span<const Cell> proposal = isMoving
                ? std::span<const Cell>{proposals[index]}
                : std::span<const Cell>{};
            const std::span<const Cell> reservedTarget = !isMoving
                ? std::span<const Cell>{value.targetCells}
                : std::span<const Cell>{};
            bodies.push_back({
                value.cells,
                proposal,
                reservedTarget,
                value.present,
                isMoving,
                isMoving &&
                    (proposals[index].empty() ||
                     collidesWithSelf(proposals[index]))
            });
        }

        const MotionResolution resolution = resolveMotionStep(
            bodies,
            movingContacts
        );
        movingContacts = resolution.movingContacts;
        const bool hardCollision =
            std::ranges::any_of(
                resolution.hardFailures,
                [](std::uint8_t value) { return value != 0; }
            );
        forwardMovingContactPending =
            resolution.hasNewMovingContact;
        if (hardCollision) {
            lives = std::max(0, lives - 1);
            // A logical step costs at most one life even when an unrelated
            // pair of moving arrows also meets during that step.
            forwardSoftPenaltyPending = false;
        } else {
            // Moving arrows are allowed to pass through one another. Apply
            // the penalty when this contacting logical step is committed,
            // rather than removing a heart before any visible movement.
            forwardSoftPenaltyPending =
                forwardMovingContactPending;
        }

        forwardStepActive = false;
        if (hasMovingArrow) {
            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                if (!moving[index]) {
                    continue;
                }
                RuntimeArrow& value = arrows[index];
                if (resolution.hardFailures[index]) {
                    value.failed = true;
                    value.motion = Motion::CollisionPause;
                    value.progress = 0.0F;
                    value.targetCells.clear();
                    continue;
                }
                value.history.push_back({
                    forwardTick,
                    value.cells
                });
                value.targetCells = std::move(proposals[index]);
                value.progress = 0.0F;
                forwardStepActive = true;
            }
        }
        return forwardStepActive;
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
            lives > 0 &&
            !anyArrowRecovering &&
            movingCollisionPauseRemaining <= 0.0F) {
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

        const bool hasForwardArrow = hasForwardMotion();

        // Recovery waits until every still-safe forward arrow has either left
        // the board or stopped as well. All failed arrows then rewind through
        // the exact accepted global states, identified by their logical tick.
        // This preserves safe tail-following without allowing red animations
        // to change occupancy in the middle of a blue step.
        bool enteredRecoveryThisFrame = false;
        float recoveryTimeAfterPause = delta;
        for (RuntimeArrow& value : arrows) {
            if (!value.present) {
                continue;
            }
            if (value.motion == Motion::CollisionPause) {
                if (hasForwardArrow) {
                    value.progress = 0.0F;
                    continue;
                }
                const float pauseTimeRemaining =
                    std::max(
                        0.0F,
                        (1.0F - value.progress) *
                            kCollisionPause
                    );
                value.progress += delta / kCollisionPause;
                if (pauseTimeRemaining <= delta) {
                    if (value.history.empty()) {
                        value.motion = Motion::Idle;
                        value.progress = 0.0F;
                    } else {
                        value.motion = Motion::Returning;
                        value.progress = 0.0F;
                        value.targetCells.clear();
                        const float remainder =
                            delta - pauseTimeRemaining;
                        recoveryTimeAfterPause =
                            enteredRecoveryThisFrame
                            ? std::min(
                                  recoveryTimeAfterPause,
                                  remainder
                              )
                            : remainder;
                        enteredRecoveryThisFrame = true;
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
        bool recoveryClockStartedThisFrame = false;
        if (hasReturningArrow && !recoveryClockActive) {
            recoveryClockActive = true;
            recoveryClockStartedThisFrame = true;
            recoveryStepActive = false;
            recoveryStepProgress = 0.0F;
            recoveryTick = forwardTick;
        }

        float remainingRecoveryProgress =
            (recoveryClockStartedThisFrame &&
                     enteredRecoveryThisFrame
                 ? recoveryTimeAfterPause
                 : delta) /
            kReturnStepDuration;
        int recoveredSteps = 0;
        constexpr float recoveryEpsilon = 0.0001F;
        while (recoveryClockActive &&
               remainingRecoveryProgress > recoveryEpsilon &&
               recoveredSteps < 64) {
            if (!recoveryStepActive) {
                bool scheduled = false;
                while (!scheduled && recoveryTick > 0) {
                    const std::uint64_t targetTick = recoveryTick - 1;
                    for (RuntimeArrow& value : arrows) {
                        if (value.motion != Motion::Returning ||
                            value.history.empty() ||
                            value.history.back().tick != targetTick) {
                            continue;
                        }
                        value.targetCells =
                            value.history.back().cells;
                        value.progress = 0.0F;
                        scheduled = true;
                    }
                    if (!scheduled) {
                        --recoveryTick;
                    }
                }
                recoveryStepActive = scheduled;
                recoveryStepProgress = 0.0F;

                const bool stillReturning =
                    std::ranges::any_of(
                        arrows,
                        [](const RuntimeArrow& value) {
                            return value.present &&
                                   value.motion ==
                                       Motion::Returning;
                        }
                    );
                if (!stillReturning) {
                    recoveryClockActive = false;
                    recoveryTick = 0;
                    break;
                }
                if (!recoveryStepActive) {
                    break;
                }
            }

            const float toBoundary =
                1.0F - recoveryStepProgress;
            const float advance = std::min(
                remainingRecoveryProgress,
                toBoundary
            );
            recoveryStepProgress += advance;
            remainingRecoveryProgress -= advance;
            for (RuntimeArrow& value : arrows) {
                if (value.motion == Motion::Returning &&
                    !value.targetCells.empty()) {
                    value.progress = recoveryStepProgress;
                }
            }

            if (recoveryStepProgress <
                1.0F - recoveryEpsilon) {
                break;
            }

            for (RuntimeArrow& value : arrows) {
                if (value.motion != Motion::Returning ||
                    value.targetCells.empty()) {
                    continue;
                }
                value.cells = std::move(value.targetCells);
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
            ++recoveredSteps;
        }
        if (recoveryClockActive &&
            std::ranges::none_of(
                arrows,
                [](const RuntimeArrow& value) {
                    return value.present &&
                           value.motion == Motion::Returning;
                }
            )) {
            recoveryClockActive = false;
            recoveryStepActive = false;
            recoveryStepProgress = 0.0F;
            recoveryTick = 0;
        }

        bool hasForward = hasForwardMotion();
        if (!hasForward) {
            forwardStepActive = false;
            forwardStepProgress = 0.0F;
            forwardMovingContactPending = false;
            forwardSoftPenaltyPending = false;
            movingCollisionPauseRemaining = 0.0F;
            forwardSpeed = 0.0F;
            std::ranges::fill(
                movingContacts,
                std::uint8_t{0}
            );
        } else {
            float forwardDelta = delta;
            if (movingCollisionPauseRemaining > 0.0F) {
                const float pausedTime = std::min(
                    forwardDelta,
                    movingCollisionPauseRemaining
                );
                movingCollisionPauseRemaining -= pausedTime;
                forwardDelta -= pausedTime;
            }

            if (forwardDelta > 0.0F && !forwardStepActive) {
                forwardStepProgress = 0.0F;
                prepareForwardStep();
            }

            if (forwardDelta > 0.0F && forwardStepActive) {
                if (forwardSpeed <= 0.0F) {
                    forwardSpeed = kInitialForwardSpeed;
                }
                float remainingDistance =
                    forwardSpeed * forwardDelta +
                    0.5F * kForwardAcceleration *
                        forwardDelta * forwardDelta;
                forwardSpeed +=
                    kForwardAcceleration * forwardDelta;

                constexpr float epsilon = 0.0001F;
                int crossedCells = 0;
                while (forwardStepActive &&
                       remainingDistance > epsilon &&
                       crossedCells < 64) {
                    const float toBoundary =
                        1.0F - forwardStepProgress;
                    if (remainingDistance < toBoundary) {
                        forwardStepProgress += remainingDistance;
                        setForwardProgress(forwardStepProgress);
                        remainingDistance = 0.0F;
                        break;
                    }

                    forwardStepProgress = 1.0F;
                    setForwardProgress(1.0F);
                    const bool movingCollision =
                        forwardMovingContactPending;
                    if (forwardSoftPenaltyPending) {
                        lives = std::max(0, lives - 1);
                    }
                    forwardMovingContactPending = false;
                    forwardSoftPenaltyPending = false;
                    remainingDistance -= toBoundary;
                    commitForwardStep();
                    ++crossedCells;

                    if (movingCollision &&
                        hasForwardMotion()) {
                        movingCollisionPauseRemaining =
                            kMovingCollisionPause;
                        break;
                    }
                    if (!hasForwardMotion()) {
                        break;
                    }
                    prepareForwardStep();
                }
            }

            hasForward = hasForwardMotion();
            if (!hasForward) {
                forwardStepActive = false;
                forwardStepProgress = 0.0F;
                forwardMovingContactPending = false;
                forwardSoftPenaltyPending = false;
                movingCollisionPauseRemaining = 0.0F;
                forwardSpeed = 0.0F;
                std::ranges::fill(
                    movingContacts,
                    std::uint8_t{0}
                );
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
        for (int y = 0; y < board.rows(); ++y) {
            for (int x = 0; x < board.columns(); ++x) {
                DrawCircleV(
                    cellCenter({x, y}),
                    kGridDotRadius,
                    kGridDot
                );
            }
        }
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

        if (!automatedScreenshot.empty() &&
            ++renderedFrames == automatedScreenshotFrame) {
            TakeScreenshot(automatedScreenshot.c_str());
            closeRequested = true;
        }
    }

    CloseWindow();
    return 0;
}

} // namespace arrow
