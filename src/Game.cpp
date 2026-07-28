#include "arrow/Game.hpp"

#include "arrow/Generator.hpp"
#include "arrow/MotionKinematics.hpp"
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
constexpr int kDefaultColumns = 40;
constexpr int kDefaultRows = 40;
constexpr int kDefaultMaximumArrowLength = 60;
constexpr float kCellSize = 34.0F;
constexpr float kBoardPadding = 34.0F;
constexpr float kHudHeight = 82.0F;
constexpr float kReturnStepDuration = 0.035F;
constexpr float kCollisionPause = 0.22F;
constexpr float kMovingCollisionPause = 0.5F;
constexpr double kInitialForwardSpeed = 8.0;
constexpr double kForwardAcceleration = 22.0;
constexpr std::size_t kMaxForwardEventsPerFrame = 1024;
constexpr std::size_t kMaxRecoveryEventsPerFrame = 1024;
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
    double progress{};
    double speed{};
    // Keeps a click at its real simulation timestamp when older event-budget
    // debt still has to be resolved first.
    double launchDelay{};
    bool present{true};
    bool failed{};
    bool movingCollisionTint{};
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
            static_cast<float>(value.progress)
        );
    }
    if (value.motion == Motion::Returning && interpolating) {
        return forwardPolyline(
            value.targetCells,
            value.direction,
            1.0F - static_cast<float>(value.progress)
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
    if (value.motion == Motion::Forward &&
        value.launchDelay <= 0.0) {
        color = value.movingCollisionTint
            ? kRed
            : kBlue;
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
        return generateBoard({
            kDefaultColumns,
            kDefaultRows,
            2,
            kDefaultMaximumArrowLength,
            boardSeed
        });
    };
    Board board = generateDefaultBoard(seed);
    std::vector<RuntimeArrow> arrows;
    std::vector<std::uint8_t> movingContacts;
    double movingCollisionPauseRemaining = 0.0;
    double forwardTimeDebt = 0.0;
    double recoveryTimeDebt = 0.0;
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
                0.0,
                0.0,
                0.0,
                true,
                false,
                false
            });
        }
        movingContacts.assign(arrows.size() * arrows.size(), 0);
        movingCollisionPauseRemaining = 0.0;
        forwardTimeDebt = 0.0;
        recoveryTimeDebt = 0.0;
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
            RuntimeArrow& value = arrows[*autoLaunch];
            value.motion = Motion::Forward;
            value.progress = 0.0;
            value.speed = kInitialForwardSpeed;
            value.launchDelay = 0.0;
            value.movingCollisionTint = false;
            value.targetCells.clear();
        }
    }
    const std::optional<std::uint32_t> automatedSecondLaunch =
        unsignedFromEnvironment(
            "ARROWPUZZLE_SCREENSHOT_LAUNCH_2"
        );
    const int automatedSecondLaunchFrame =
        static_cast<int>(
            unsignedFromEnvironment(
                "ARROWPUZZLE_SCREENSHOT_LAUNCH_2_FRAME"
            ).value_or(10)
        );
    bool automatedSecondLaunchDone = false;
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

    const auto isActiveForward =
        [](const RuntimeArrow& value) {
            return value.present &&
                   value.motion == Motion::Forward &&
                   value.launchDelay <= 0.0;
        };

    const auto clearMovingContactsFor =
        [&movingContacts, &arrows](std::size_t arrowIndex) {
            const std::size_t count = arrows.size();
            for (std::size_t other = 0; other < count; ++other) {
                movingContacts[arrowIndex * count + other] = 0;
                movingContacts[other * count + arrowIndex] = 0;
            }
        };

    const auto prepareForwardTransitions = [&]() {
        std::vector<std::vector<Cell>> proposals(arrows.size());
        std::vector<std::uint8_t> attempting(arrows.size(), 0);
        std::vector<MotionBodyView> bodies;
        bodies.reserve(arrows.size());
        bool hasForwardCandidate = false;

        for (std::size_t index = 0; index < arrows.size(); ++index) {
            const RuntimeArrow& value = arrows[index];
            if (!isActiveForward(value)) {
                continue;
            }
            hasForwardCandidate = true;
            if (!value.targetCells.empty()) {
                continue;
            }
            attempting[index] = 1;
            proposals[index] = advancedCells(value);
        }
        if (!hasForwardCandidate) {
            return false;
        }

        for (std::size_t index = 0; index < arrows.size(); ++index) {
            const RuntimeArrow& value = arrows[index];
            const bool activeForward =
                isActiveForward(value);
            const bool isAttempting = attempting[index] != 0;
            const std::span<const Cell> proposal = isAttempting
                ? std::span<const Cell>{proposals[index]}
                : activeForward
                    ? std::span<const Cell>{value.targetCells}
                    : std::span<const Cell>{};
            const std::span<const Cell> reservedTarget =
                !activeForward
                ? std::span<const Cell>{value.targetCells}
                : std::span<const Cell>{};
            bodies.push_back({
                value.cells,
                proposal,
                reservedTarget,
                value.present,
                activeForward
                    ? MotionRole::Advancing
                    : MotionRole::Static,
                isAttempting &&
                    (proposals[index].empty() ||
                     collidesWithSelf(proposals[index]))
            });
        }

        const MotionResolution resolution = resolveMotionStep(
            bodies,
            {},
            {board.columns(), board.rows()}
        );
        const bool hardCollision =
            std::ranges::any_of(
                resolution.hardFailures,
                [](std::uint8_t value) { return value != 0; }
            );

        for (std::size_t index = 0;
             index < arrows.size();
             ++index) {
            RuntimeArrow& value = arrows[index];
            if (resolution.hardFailures[index]) {
                value.failed = true;
                value.motion = Motion::CollisionPause;
                value.progress = 0.0;
                value.speed = 0.0;
                value.launchDelay = 0.0;
                value.targetCells.clear();
                clearMovingContactsFor(index);
            } else if (attempting[index]) {
                value.targetCells = std::move(proposals[index]);
                value.progress = 0.0;
            }
        }
        return hardCollision;
    };

    const auto revalidateCompletedTransitions =
        [&](std::span<std::uint8_t> completing) {
            std::vector<MotionBodyView> bodies;
            bodies.reserve(arrows.size());
            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                const RuntimeArrow& value = arrows[index];
                const bool activeForward =
                    isActiveForward(value);
                const bool isCompleting =
                    activeForward &&
                    completing[index] != 0;
                bodies.push_back({
                    value.cells,
                    activeForward
                        ? std::span<const Cell>{
                              value.targetCells
                          }
                        : std::span<const Cell>{},
                    !activeForward
                        ? std::span<const Cell>{
                              value.targetCells
                          }
                        : std::span<const Cell>{},
                    value.present,
                    isCompleting
                        ? MotionRole::Advancing
                        : activeForward
                            ? MotionRole::Moving
                            : MotionRole::Static,
                    false
                });
            }

            const MotionResolution resolution = resolveMotionStep(
                bodies,
                {},
                {board.columns(), board.rows()}
            );
            const bool hardCollision =
                std::ranges::any_of(
                    resolution.hardFailures,
                    [](std::uint8_t value) {
                        return value != 0;
                    }
                );
            if (!hardCollision) {
                return false;
            }

            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                if (!resolution.hardFailures[index]) {
                    continue;
                }
                completing[index] = 0;
                RuntimeArrow& value = arrows[index];
                value.failed = true;
                value.motion = Motion::CollisionPause;
                value.progress = 0.0;
                value.speed = 0.0;
                value.launchDelay = 0.0;
                value.targetCells.clear();
                clearMovingContactsFor(index);
            }
            return true;
        };

    const auto resolveCompletedMovingContacts =
        [&](std::span<const std::uint8_t> completing) {
            std::vector<MotionBodyView> bodies;
            bodies.reserve(arrows.size());
            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                const RuntimeArrow& value = arrows[index];
                const bool activeForward =
                    isActiveForward(value);
                bodies.push_back({
                    value.cells,
                    activeForward
                        ? std::span<const Cell>{
                              value.targetCells
                          }
                        : std::span<const Cell>{},
                    {},
                    activeForward,
                    completing[index]
                        ? MotionRole::Advancing
                        : activeForward
                            ? MotionRole::Moving
                            : MotionRole::Static,
                    false
                });
            }
            MotionResolution resolution = resolveMotionStep(
                bodies,
                movingContacts,
                {board.columns(), board.rows()}
            );
            if (resolution.hasNewMovingContact) {
                const std::size_t count = arrows.size();
                for (std::size_t first = 0;
                     first < count;
                     ++first) {
                    for (std::size_t second = first + 1;
                         second < count;
                         ++second) {
                        const std::size_t pair =
                            first * count + second;
                        if (!resolution.movingContacts[pair] ||
                            movingContacts[pair]) {
                            continue;
                        }
                        arrows[first].movingCollisionTint =
                            true;
                        arrows[second].movingCollisionTint =
                            true;
                    }
                }
            }
            movingContacts = resolution.movingContacts;
            return resolution.hasNewMovingContact;
        };

    const auto commitCompletedTransitions =
        [&](std::span<const std::uint8_t> completing) {
            bool committed = false;
            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                if (!completing[index]) {
                    continue;
                }
                RuntimeArrow& value = arrows[index];
                value.history.push_back({
                    forwardTick,
                    value.cells
                });
                value.cells = std::move(value.targetCells);
                value.targetCells.clear();
                value.progress = 0.0;
                committed = true;

                if (allOutside(value, board)) {
                    value.present = false;
                    value.history.clear();
                    value.motion = Motion::Idle;
                    value.speed = 0.0;
                    value.launchDelay = 0.0;
                    clearMovingContactsFor(index);
                }
            }
            if (committed) {
                ++forwardTick;
            }
        };

    const auto forwardEventBudget = [&]() {
        const std::size_t maximumDimension =
            static_cast<std::size_t>(
                std::max(board.columns(), board.rows())
            );
        std::size_t budget = 1;
        for (const RuntimeArrow& value : arrows) {
            if (value.present &&
                value.motion == Motion::Forward) {
                budget += maximumDimension +
                          value.cells.size() + 2;
            }
        }
        return std::min(
            budget,
            kMaxForwardEventsPerFrame
        );
    };

    while (!WindowShouldClose() && !closeRequested) {
        const float delta = std::max(GetFrameTime(), 0.0F);

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

        if (!automatedScreenshot.empty() &&
            !automatedSecondLaunchDone &&
            automatedSecondLaunch.has_value() &&
            renderedFrames >= automatedSecondLaunchFrame) {
            automatedSecondLaunchDone = true;
            if (*automatedSecondLaunch < arrows.size()) {
                RuntimeArrow& value =
                    arrows[*automatedSecondLaunch];
                if (value.present &&
                    value.motion == Motion::Idle) {
                    value.motion = Motion::Forward;
                    value.progress = 0.0;
                    value.speed = kInitialForwardSpeed;
                    value.launchDelay = forwardTimeDebt;
                    value.movingCollisionTint = false;
                    value.history.clear();
                    value.targetCells.clear();
                }
            }
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
                value.progress = 0.0;
                value.speed = kInitialForwardSpeed;
                value.launchDelay = forwardTimeDebt;
                value.movingCollisionTint = false;
                value.history.clear();
                value.targetCells.clear();
            }
        }

        double simulationDelta = static_cast<double>(delta);
        if (movingCollisionPauseRemaining > 0.0) {
            const double pausedTime = std::min(
                simulationDelta,
                movingCollisionPauseRemaining
            );
            movingCollisionPauseRemaining -= pausedTime;
            simulationDelta -= pausedTime;
            if (movingCollisionPauseRemaining <
                std::numeric_limits<double>::epsilon()) {
                movingCollisionPauseRemaining = 0.0;
            }
        }

        const bool hasForwardArrow = hasForwardMotion();

        // Recovery waits until every still-safe forward arrow has either left
        // the board or stopped as well. All failed arrows then rewind through
        // the exact accepted global states, identified by their logical tick.
        // This preserves safe tail-following without allowing red animations
        // to change occupancy in the middle of a blue step.
        bool enteredRecoveryThisFrame = false;
        double recoveryTimeAfterPause = simulationDelta;
        for (RuntimeArrow& value : arrows) {
            if (!value.present) {
                continue;
            }
            if (value.motion == Motion::CollisionPause) {
                if (hasForwardArrow) {
                    value.progress = 0.0F;
                    continue;
                }
                const double pauseTimeRemaining =
                    std::max(
                        0.0,
                        (1.0 - value.progress) *
                            static_cast<double>(
                                kCollisionPause
                            )
                    );
                value.progress +=
                    simulationDelta /
                    static_cast<double>(kCollisionPause);
                if (pauseTimeRemaining <= simulationDelta) {
                    if (value.history.empty()) {
                        value.motion = Motion::Idle;
                        value.progress = 0.0F;
                    } else {
                        value.motion = Motion::Returning;
                        value.progress = 0.0F;
                        value.targetCells.clear();
                        const double remainder =
                            simulationDelta -
                            pauseTimeRemaining;
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

        double remainingRecoveryProgress =
            ((recoveryClockStartedThisFrame &&
                      enteredRecoveryThisFrame
                  ? recoveryTimeAfterPause
                  : simulationDelta) +
             recoveryTimeDebt) /
            static_cast<double>(kReturnStepDuration);
        recoveryTimeDebt = 0.0;
        std::size_t recoveredSteps = 0;
        constexpr double recoveryEpsilon = 0.0001;
        while (recoveryClockActive &&
               remainingRecoveryProgress > recoveryEpsilon &&
               recoveredSteps <
                   kMaxRecoveryEventsPerFrame) {
            if (!recoveryStepActive) {
                bool scheduled = false;
                std::optional<std::uint64_t> targetTick;
                for (const RuntimeArrow& value : arrows) {
                    if (value.motion != Motion::Returning ||
                        value.history.empty()) {
                        continue;
                    }
                    targetTick = std::max(
                        targetTick.value_or(0),
                        value.history.back().tick
                    );
                }
                if (targetTick.has_value()) {
                    recoveryTick = *targetTick + 1;
                    for (RuntimeArrow& value : arrows) {
                        if (value.motion != Motion::Returning ||
                            value.history.empty() ||
                            value.history.back().tick !=
                                *targetTick) {
                            continue;
                        }
                        value.targetCells =
                            value.history.back().cells;
                        value.progress = 0.0F;
                        scheduled = true;
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

            const double toBoundary =
                1.0 -
                static_cast<double>(recoveryStepProgress);
            const double advance = std::min(
                remainingRecoveryProgress,
                toBoundary
            );
            recoveryStepProgress +=
                static_cast<float>(advance);
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
            recoveredSteps >=
                kMaxRecoveryEventsPerFrame &&
            remainingRecoveryProgress >
                recoveryEpsilon) {
            recoveryTimeDebt =
                remainingRecoveryProgress *
                static_cast<double>(kReturnStepDuration);
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
            recoveryTimeDebt = 0.0;
        }

        bool hasForward = hasForwardMotion();
        if (!hasForward) {
            forwardTimeDebt = 0.0;
            std::ranges::fill(
                movingContacts,
                std::uint8_t{0}
            );
        } else if (movingCollisionPauseRemaining <= 0.0) {
            // Debt is simulated before this frame's time. New arrows carry
            // the same amount as launchDelay, so they cannot age retroactively.
            double priorDebtRemaining = forwardTimeDebt;
            double remainingForwardTime =
                simulationDelta +
                forwardTimeDebt;
            forwardTimeDebt = 0.0;
            std::size_t processedEvents = 0;
            const std::size_t eventBudget =
                forwardEventBudget();

            while (remainingForwardTime > 0.0 &&
                   hasForwardMotion()) {
                const bool preparationCollision =
                    prepareForwardTransitions();
                if (preparationCollision) {
                    lives = std::max(0, lives - 1);
                }
                if (!hasForwardMotion()) {
                    break;
                }
                if (processedEvents >= eventBudget) {
                    forwardTimeDebt = remainingForwardTime;
                    break;
                }

                std::vector<double> boundaryTimes(
                    arrows.size(),
                    std::numeric_limits<double>::infinity()
                );
                double earliestBoundary =
                    std::numeric_limits<double>::infinity();
                double earliestActivation =
                    std::numeric_limits<double>::infinity();
                for (std::size_t index = 0;
                     index < arrows.size();
                     ++index) {
                    RuntimeArrow& value = arrows[index];
                    if (!value.present ||
                        value.motion != Motion::Forward) {
                        continue;
                    }
                    if (value.launchDelay > 0.0) {
                        earliestActivation = std::min(
                            earliestActivation,
                            value.launchDelay
                        );
                        continue;
                    }
                    if (value.targetCells.empty()) {
                        continue;
                    }
                    if (value.speed <= 0.0) {
                        value.speed = kInitialForwardSpeed;
                    }
                    boundaryTimes[index] =
                        timeToCoverDistance(
                            std::max(
                                0.0,
                                1.0 - value.progress
                            ),
                            value.speed,
                            kForwardAcceleration
                        );
                    earliestBoundary = std::min(
                        earliestBoundary,
                        boundaryTimes[index]
                    );
                }
                const double earliestEvent = std::min(
                    earliestBoundary,
                    earliestActivation
                );
                if (!std::isfinite(earliestEvent)) {
                    forwardTimeDebt = remainingForwardTime;
                    break;
                }

                const bool reachesEvent =
                    earliestEvent <= remainingForwardTime;
                const double advanceTime = reachesEvent
                    ? earliestEvent
                    : remainingForwardTime;
                for (RuntimeArrow& value : arrows) {
                    if (!value.present ||
                        value.motion != Motion::Forward ||
                        value.launchDelay > 0.0 ||
                        value.targetCells.empty()) {
                        continue;
                    }
                    value.progress += acceleratedDistance(
                        value.speed,
                        kForwardAcceleration,
                        advanceTime
                    );
                    value.speed +=
                        kForwardAcceleration * advanceTime;
                    value.progress = std::min(
                        value.progress,
                        1.0
                    );
                }
                for (RuntimeArrow& value : arrows) {
                    if (!value.present ||
                        value.motion != Motion::Forward ||
                        value.launchDelay <= 0.0) {
                        continue;
                    }
                    value.launchDelay = std::max(
                        0.0,
                        value.launchDelay - advanceTime
                    );
                }
                remainingForwardTime -= advanceTime;
                priorDebtRemaining = std::max(
                    0.0,
                    priorDebtRemaining - advanceTime
                );
                if (!reachesEvent) {
                    break;
                }

                const bool boundaryEvent =
                    std::isfinite(earliestBoundary) &&
                    eventTimesCoincide(
                        earliestEvent,
                        earliestBoundary
                    );
                if (!boundaryEvent) {
                    ++processedEvents;
                    continue;
                }

                std::vector<std::uint8_t> completing(
                    arrows.size(),
                    0
                );
                for (std::size_t index = 0;
                     index < arrows.size();
                     ++index) {
                    if (eventTimesCoincide(
                            boundaryTimes[index],
                            earliestEvent
                        )) {
                        completing[index] = 1;
                        arrows[index].progress = 1.0;
                    }
                }

                const bool completionHardCollision =
                    revalidateCompletedTransitions(completing);
                const bool movingCollision =
                    resolveCompletedMovingContacts(completing);
                commitCompletedTransitions(completing);
                const bool nextHardCollision =
                    prepareForwardTransitions();
                if (completionHardCollision ||
                    movingCollision ||
                    nextHardCollision) {
                    lives = std::max(0, lives - 1);
                }
                ++processedEvents;

                if (movingCollision) {
                    movingCollisionPauseRemaining =
                        static_cast<double>(
                            kMovingCollisionPause
                        );
                    forwardTimeDebt =
                        priorDebtRemaining;
                    break;
                }
            }

            hasForward = hasForwardMotion();
            if (!hasForward) {
                forwardTimeDebt = 0.0;
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
