#include "arrow/Game.hpp"

#include "arrow/Generator.hpp"
#include "arrow/MotionKinematics.hpp"
#include "arrow/MotionResolver.hpp"
#include "arrow/Persistence.hpp"
#include "StaticBoardGeometry.hpp"
#include "UiFontData.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <future>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace arrow {
namespace {

constexpr int kInitialWidth = 1100;
constexpr int kInitialHeight = 900;
constexpr int kDefaultColumns = 40;
constexpr int kDefaultRows = 40;
constexpr int kDefaultMaximumArrowLength = 60;
constexpr int kDefaultCandidateAttempts = 8;
constexpr float kCellSize = 34.0F;
constexpr float kBoardPadding = 34.0F;
constexpr float kHudHeight = 82.0F;
constexpr float kReturnStepDuration = 0.0175F;
constexpr float kCollisionPause = 0.22F;
constexpr float kMovingCollisionPause = 0.5F;
constexpr double kInitialForwardSpeed = 16.0;
constexpr double kForwardAcceleration = 88.0;
constexpr std::size_t kMaxForwardEventsPerFrame = 1024;
constexpr std::size_t kMaxRecoveryEventsPerFrame = 1024;
constexpr float kArrowThickness = 6.4F;
constexpr float kGridDotRadius = kArrowThickness * 0.5F;

constexpr Color kInk{22, 29, 48, 255};
constexpr Color kBlue{16, 132, 255, 255};
constexpr Color kRed{238, 67, 74, 255};
constexpr Color kSoft{226, 232, 241, 255};
constexpr Color kMuted{104, 115, 132, 255};
constexpr Color kGridDot{195, 202, 214, 255};

struct TextStyle {
    float size;
    float spacing;
};

constexpr int kUiFontRasterSize = 64;
constexpr TextStyle kBrandText{24.0F, 0.8F};
constexpr TextStyle kHudText{19.0F, 0.4F};
constexpr TextStyle kOverlayTitleText{24.0F, 0.6F};
constexpr TextStyle kOverlayDetailText{17.0F, 0.1F};
constexpr TextStyle kErrorTitleText{26.0F, 0.6F};
constexpr TextStyle kErrorActionText{18.0F, 0.1F};
constexpr TextStyle kErrorDetailText{15.0F, 0.0F};
constexpr TextStyle kBannerText{25.0F, 0.1F};
constexpr TextStyle kHelpText{17.0F, 0.1F};
constexpr TextStyle kMenuBrandText{34.0F, 1.1F};
constexpr TextStyle kMenuTitleText{22.0F, 0.6F};
constexpr TextStyle kMenuButtonText{20.0F, 0.2F};
constexpr TextStyle kMenuHintText{15.0F, 0.1F};

enum class Screen {
    MainMenu,
    NewGameMenu,
    SettingsMenu,
    Playing
};

struct DifficultyPreset {
    const char* label;
    int columns;
    int rows;
    int maximumLength;
    int candidateAttempts;
};

constexpr std::array<DifficultyPreset, 4> kDifficultyPresets{{
    {"Simple (20 x 20)", 20, 20, 30, 8},
    {"Middle (40 x 40)", 40, 40, 60, 8},
    {"Hard (80 x 80)", 80, 80, 120, 8},
    {"Very Hard (160 x 160)", 160, 160, 960, 8}
}};

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

// Sleep on the CPU instead of waiting inside the graphics driver's swap call.
// Keep a separate clock because idle frames poll input without EndDrawing().
class FramePacer {
public:
    float beginFrame() {
        constexpr auto interval = std::chrono::duration<double>{1.0 / 60.0};
        const auto deadline = previous_ +
            std::chrono::duration_cast<Clock::duration>(interval);
        std::this_thread::sleep_until(deadline);
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - previous_).count();
        previous_ = now;
        return elapsed;
    }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point previous_{Clock::now()};
};

struct MenuFrameState {
    Screen screen;
    int width;
    int height;
    int focusedButton;
    std::optional<int> pressedButton;
    std::optional<int> hoveredButton;
    Settings settings;
    bool resumeAvailable;
    bool generationInProgress;
    bool windowFocused;

    bool operator==(const MenuFrameState&) const = default;
};

struct PlayingFrameState {
    int width;
    int height;
    int lives;
    int remaining;
    bool generationInProgress;
    bool windowFocused;
    std::string generationError;

    bool operator==(const PlayingFrameState&) const = default;
};

Vector2 measureText(
    Font font,
    const char* text,
    TextStyle style
) {
    return MeasureTextEx(
        font,
        text,
        style.size,
        style.spacing
    );
}

void drawText(
    Font font,
    const char* text,
    Vector2 position,
    TextStyle style,
    Color color
) {
    DrawTextEx(
        font,
        text,
        position,
        style.size,
        style.spacing,
        color
    );
}

void drawCenteredText(
    Font font,
    const char* text,
    float centerX,
    float y,
    TextStyle style,
    Color color
) {
    const Vector2 size = measureText(font, text, style);
    drawText(
        font,
        text,
        {centerX - size.x * 0.5F, y},
        style,
        color
    );
}

Rectangle menuButtonRectangle(int index, int count) {
    constexpr float buttonHeight = 58.0F;
    constexpr float buttonGap = 16.0F;
    const float width = std::min(
        460.0F,
        static_cast<float>(GetScreenWidth()) - 80.0F
    );
    const float totalHeight =
        static_cast<float>(count) * buttonHeight +
        static_cast<float>(std::max(0, count - 1)) * buttonGap;
    const float startY = std::max(
        205.0F,
        (static_cast<float>(GetScreenHeight()) - totalHeight) * 0.5F +
            42.0F
    );
    return {
        (static_cast<float>(GetScreenWidth()) - width) * 0.5F,
        startY + static_cast<float>(index) * (buttonHeight + buttonGap),
        width,
        buttonHeight
    };
}

void drawMenuButton(
    Font font,
    Rectangle bounds,
    const char* label,
    bool enabled,
    bool focused,
    bool armed
) {
    const bool hovered =
        enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const bool pressed = hovered && armed;
    const bool highlighted = enabled && (hovered || focused);
    const Color fill = !enabled
        ? Color{244, 246, 249, 255}
        : pressed
            ? Color{218, 233, 252, 255}
            : highlighted
                ? Color{232, 241, 253, 255}
                : Color{249, 250, 252, 255};
    const Color outline = !enabled
        ? Color{230, 233, 238, 255}
        : highlighted
            ? kBlue
            : kSoft;
    const Color textColor = !enabled
        ? Color{172, 179, 190, 255}
        : highlighted
            ? kBlue
            : kInk;

    DrawRectangleRounded(bounds, 0.18F, 8, fill);
    DrawRectangleRoundedLinesEx(
        bounds,
        0.18F,
        8,
        highlighted ? 2.0F : 1.5F,
        outline
    );
    const Vector2 textSize = measureText(
        font,
        label,
        kMenuButtonText
    );
    drawText(
        font,
        label,
        {
            bounds.x + (bounds.width - textSize.x) * 0.5F,
            bounds.y + (bounds.height - textSize.y) * 0.5F
        },
        kMenuButtonText,
        textColor
    );
}

void drawMenuHeading(Font font, const char* title) {
    drawCenteredText(
        font,
        "ARROW PUZZLE",
        static_cast<float>(GetScreenWidth()) * 0.5F,
        78.0F,
        kMenuBrandText,
        kInk
    );
    drawCenteredText(
        font,
        title,
        static_cast<float>(GetScreenWidth()) * 0.5F,
        142.0F,
        kMenuTitleText,
        kMuted
    );
}

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

void drawArrow(StaticBoardGeometry& geometry, const RuntimeArrow& value) {
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

    for (std::size_t index = 1; index < points.size(); ++index) {
        geometry.addLine(
            points[index - 1],
            points[index],
            kArrowThickness,
            color
        );
        geometry.addCircle(
            points[index],
            kArrowThickness * 0.5F,
            color
        );
    }
    geometry.addCircle(
        points.front(),
        kArrowThickness * 0.5F,
        color
    );

    const ArrowHeadTriangle head = arrowHeadTriangle(
        points.front(),
        value.direction
    );
    geometry.addTriangle(
        head.tip,
        head.left,
        head.right,
        color
    );
}

void cacheIdleArrow(StaticBoardGeometry& geometry, const RuntimeArrow& value) {
    if (value.cells.empty()) {
        return;
    }
    const Color color = value.failed ? kRed : kInk;
    const Vector2 head = cellCenter(value.cells.front());
    Vector2 segmentStart = head;
    geometry.addCircle(head, kArrowThickness * 0.5F, color);
    for (std::size_t index = 1; index < value.cells.size(); ++index) {
        const bool endOfRun = index + 1 == value.cells.size() ||
            value.cells[index] - value.cells[index - 1] !=
                value.cells[index + 1] - value.cells[index];
        if (endOfRun) {
            const Vector2 point = cellCenter(value.cells[index]);
            geometry.addLine(segmentStart, point, kArrowThickness, color);
            geometry.addCircle(point, kArrowThickness * 0.5F, color);
            segmentStart = point;
        }
    }
    const ArrowHeadTriangle triangle = arrowHeadTriangle(head, value.direction);
    geometry.addTriangle(triangle.tip, triangle.left, triangle.right, color);
}

void cacheIdleBoardGeometry(
    StaticBoardGeometry& geometry,
    const Board& board,
    std::span<const RuntimeArrow> arrows
) {
    geometry.begin();
    std::vector<std::uint8_t> occupied(
        static_cast<std::size_t>(board.columns()) * board.rows(),
        0
    );
    for (const RuntimeArrow& value : arrows) {
        if (value.present && value.motion == Motion::Idle) {
            for (Cell cell : value.cells) {
                if (board.isInside(cell)) {
                    occupied[static_cast<std::size_t>(cell.y) *
                                 board.columns() + cell.x] = 1;
                }
            }
        }
    }
    for (int y = 0; y < board.rows(); ++y) {
        for (int x = 0; x < board.columns(); ++x) {
            // An idle stroke completely covers its grid dot. Exposed dots are
            // restored when launching an arrow invalidates the geometry.
            if (occupied[static_cast<std::size_t>(y) * board.columns() + x] == 0) {
                geometry.addCircle(cellCenter({x, y}), kGridDotRadius, kGridDot);
            }
        }
    }
    for (const RuntimeArrow& value : arrows) {
        if (value.present && value.motion == Motion::Idle) {
            cacheIdleArrow(geometry, value);
        }
    }
    geometry.finish();
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

std::uint64_t movingContactKey(
    std::size_t first,
    std::size_t second
) {
    const std::uint64_t low = static_cast<std::uint64_t>(
        std::min(first, second)
    );
    const std::uint64_t high = static_cast<std::uint64_t>(
        std::max(first, second)
    );
    return (low << 32U) | high;
}

bool movingContactContains(
    std::uint64_t key,
    std::size_t arrowIndex
) {
    const std::uint64_t index =
        static_cast<std::uint64_t>(arrowIndex);
    return (key >> 32U) == index ||
           (key & 0xFFFFFFFFULL) == index;
}

void drawHeart(Vector2 center, bool filled) {
    const Color color = filled ? kRed : kSoft;
    constexpr std::size_t segmentCount = 6;
    constexpr std::size_t pointsPerSegment = 8;
    constexpr std::size_t pointCount = 48;
    static const std::array<Vector2, pointCount> outline = [] {
        constexpr std::array<Vector2, segmentCount + 1> anchors{{
            {0.0F, -5.0F},
            {15.0F, -8.0F},
            {11.0F, 9.0F},
            {0.0F, 17.0F},
            {-11.0F, 9.0F},
            {-15.0F, -8.0F},
            {0.0F, -5.0F}
        }};
        constexpr std::array<Vector2, segmentCount> firstControls{{
            {4.0F, -11.0F},
            {19.0F, -2.0F},
            {8.0F, 12.0F},
            {-4.0F, 17.0F},
            {-16.0F, 5.0F},
            {-10.0F, -13.0F}
        }};
        constexpr std::array<Vector2, segmentCount> secondControls{{
            {10.0F, -13.0F},
            {16.0F, 5.0F},
            {4.0F, 17.0F},
            {-8.0F, 12.0F},
            {-19.0F, -2.0F},
            {-4.0F, -11.0F}
        }};

        std::array<Vector2, pointCount> points{};
        for (std::size_t segment = 0;
             segment < segmentCount;
             ++segment) {
            for (std::size_t step = 0;
                 step < pointsPerSegment;
                 ++step) {
                const float t =
                    static_cast<float>(step) /
                    static_cast<float>(pointsPerSegment);
                const float oneMinusT = 1.0F - t;
                const float anchorWeight =
                    oneMinusT * oneMinusT * oneMinusT;
                const float firstControlWeight =
                    3.0F * oneMinusT * oneMinusT * t;
                const float secondControlWeight =
                    3.0F * oneMinusT * t * t;
                const float endWeight = t * t * t;
                points[segment * pointsPerSegment + step] = {
                    anchors[segment].x * anchorWeight +
                        firstControls[segment].x *
                            firstControlWeight +
                        secondControls[segment].x *
                            secondControlWeight +
                        anchors[segment + 1].x * endWeight,
                    anchors[segment].y * anchorWeight +
                        firstControls[segment].y *
                            firstControlWeight +
                        secondControls[segment].y *
                            secondControlWeight +
                        anchors[segment + 1].y * endWeight
                };
            }
        }
        return points;
    }();

    const Vector2 fillCenter{center.x, center.y + 2.0F};
    for (std::size_t index = 0; index < pointCount; ++index) {
        const Vector2 first{
            center.x + outline[index].x,
            center.y + outline[index].y
        };
        const Vector2 second{
            center.x + outline[(index + 1) % pointCount].x,
            center.y + outline[(index + 1) % pointCount].y
        };
        DrawTriangle(fillCenter, second, first, color);
    }
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
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(kInitialWidth, kInitialHeight, "Arrow Puzzle");
    SetWindowMinSize(720, 600);
    SetTargetFPS(0);
    SetExitKey(KEY_NULL);

    const Font defaultFont = GetFontDefault();
    const int uiFontDataSize =
        assets::kUiFontDataSize <=
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max()
                )
            ? static_cast<int>(assets::kUiFontDataSize)
            : 0;
    Font uiFont = LoadFontFromMemory(
        ".ttf",
        assets::kUiFontData,
        uiFontDataSize,
        kUiFontRasterSize,
        nullptr,
        0
    );
    const bool ownsUiFont =
        uiFont.texture.id != defaultFont.texture.id;
    if (ownsUiFont) {
        SetTextureFilter(
            uiFont.texture,
            TEXTURE_FILTER_BILINEAR
        );
    } else {
        uiFont = defaultFont;
        TraceLog(
            LOG_WARNING,
            "Could not load the embedded UI font"
        );
    }

    Persistence persistence(Persistence::executableDirectory());
    Settings settings = persistence.loadSettings();
    std::optional<SavedGame> savedGame =
        persistence.loadSavedGame();
    bool resumeAvailable = savedGame.has_value();
    Screen screen = Screen::MainMenu;
    bool activeGame = false;

    std::uint32_t seed = unsignedFromEnvironment(
        "ARROWPUZZLE_SEED"
    ).value_or(
        static_cast<std::uint32_t>(std::time(nullptr))
    );
    GeneratorOptions currentGeneratorOptions{
        kDefaultColumns,
        kDefaultRows,
        2,
        kDefaultMaximumArrowLength,
        seed,
        kDefaultCandidateAttempts,
        settings.arrowLoops
    };
    // Keep raylib entirely on this thread. The worker only creates value-type
    // board data, which is moved into the live game at a frame boundary.
    Board board(20, 20, {});
    std::vector<RuntimeArrow> arrows;
    std::unordered_set<std::uint64_t> movingContacts;
    double movingCollisionPauseRemaining = 0.0;
    double forwardTimeDebt = 0.0;
    double recoveryTimeDebt = 0.0;
    std::uint64_t forwardTick = 0;
    bool recoveryClockActive = false;
    bool recoveryStepActive = false;
    float recoveryStepProgress = 0.0F;
    std::uint64_t recoveryTick = 0;
    std::optional<std::size_t> pressedArrow;
    std::optional<int> pressedMenuButton;
    int focusedMenuButton = 0;
    RenderTexture2D staticBoardLayer{};
    int staticBoardLayerWidth = 0;
    int staticBoardLayerHeight = 0;
    bool staticBoardLayerDirty = true;
    StaticBoardGeometry staticBoardGeometry;
    bool staticBoardGeometryDirty = true;
    const auto invalidateStaticBoard = [&]() {
        staticBoardLayerDirty = true;
        staticBoardGeometryDirty = true;
    };
    auto resetRuntime = [&](const SavedGame* restored = nullptr) {
        arrows.clear();
        arrows.reserve(board.arrows().size());
        for (std::size_t index = 0;
             index < board.arrows().size();
             ++index) {
            const Arrow& source = board.arrows()[index];
            const bool present =
                restored == nullptr ||
                restored->present[index];
            const bool failed =
                restored != nullptr &&
                restored->failed[index];
            arrows.push_back({
                source.cells,
                {},
                {},
                source.direction,
                Motion::Idle,
                0.0,
                0.0,
                0.0,
                present,
                failed,
                false
            });
        }
        movingContacts.clear();
        movingCollisionPauseRemaining = 0.0;
        forwardTimeDebt = 0.0;
        recoveryTimeDebt = 0.0;
        forwardTick = 0;
        recoveryClockActive = false;
        recoveryStepActive = false;
        recoveryStepProgress = 0.0F;
        recoveryTick = 0;
        pressedArrow.reset();
        invalidateStaticBoard();
    };
    resetRuntime();

    int lives = 3;
    std::future<Board> generationFuture;
    bool generationInProgress = false;
    bool generationOverlayRendered = false;
    std::string generationError;
    bool closeRequested = false;
    const std::string automatedScreenshot =
        environmentValue("ARROWPUZZLE_SCREENSHOT");
    const int automatedScreenshotFrame = static_cast<int>(std::max(
        std::uint32_t{1},
        unsignedFromEnvironment(
            "ARROWPUZZLE_SCREENSHOT_FRAME"
        ).value_or(30)
    ));
    const std::uint32_t automatedMenuScreen =
        unsignedFromEnvironment(
            "ARROWPUZZLE_SCREENSHOT_MENU"
        ).value_or(0);
    const std::uint32_t automatedDifficulty =
        unsignedFromEnvironment(
            "ARROWPUZZLE_DIFFICULTY"
        ).value_or(2);
    const bool automatedResume =
        unsignedFromEnvironment(
            "ARROWPUZZLE_RESUME"
        ).value_or(0) != 0;
    const bool automatedSaveOnExit =
        unsignedFromEnvironment(
            "ARROWPUZZLE_SAVE_ON_EXIT"
        ).value_or(0) != 0;
    if (automatedMenuScreen == 1) {
        screen = Screen::MainMenu;
    } else if (automatedMenuScreen == 2) {
        screen = Screen::NewGameMenu;
    } else if (automatedMenuScreen == 3) {
        screen = Screen::SettingsMenu;
    }
    bool automatedInitialSetupPending =
        !automatedScreenshot.empty();
    const auto applyAutomatedInitialSetup = [&]() {
        if (!automatedInitialSetupPending) {
            return;
        }
        automatedInitialSetupPending = false;
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
    };
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

    float boardWidth =
        kBoardPadding * 2.0F + board.columns() * kCellSize;
    float boardHeight =
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
    const auto updateBoardDimensions = [&]() {
        boardWidth =
            kBoardPadding * 2.0F + board.columns() * kCellSize;
        boardHeight =
            kBoardPadding * 2.0F + board.rows() * kCellSize;
    };
    const auto resetCamera = [&]() {
        camera.offset = {
            static_cast<float>(GetScreenWidth()) * 0.5F,
            kHudHeight +
                (static_cast<float>(GetScreenHeight()) -
                 kHudHeight) *
                    0.5F
        };
        camera.target = {
            boardWidth * 0.5F,
            boardHeight * 0.5F
        };
        camera.zoom = std::min(
            (GetScreenWidth() - 60.0F) / boardWidth,
            (GetScreenHeight() - kHudHeight - 50.0F) /
                boardHeight
        );
        staticBoardLayerDirty = true;
    };
    const auto optionsForDimensions =
        [&](int columns, int rows) {
            for (const DifficultyPreset& preset :
                 kDifficultyPresets) {
                if (preset.columns == columns &&
                    preset.rows == rows) {
                    return GeneratorOptions{
                        columns,
                        rows,
                        2,
                        preset.maximumLength,
                        seed,
                        preset.candidateAttempts,
                        settings.arrowLoops
                    };
                }
            }
            return GeneratorOptions{
                columns,
                rows,
                2,
                kDefaultMaximumArrowLength,
                seed,
                kDefaultCandidateAttempts,
                settings.arrowLoops
            };
        };
    const auto startGeneration = [&](GeneratorOptions options) {
        if (generationInProgress) {
            return;
        }
        options.allowArrowLoops = settings.arrowLoops;
        currentGeneratorOptions = options;
        generationError.clear();
        generationOverlayRendered = false;
        pressedArrow.reset();
        screen = Screen::Playing;
        activeGame = false;
        try {
            generationFuture = std::async(
                std::launch::async,
                [options]() mutable {
                    const bool hardPreset =
                        options.columns == 80 &&
                        options.rows == 80;
                    const int seedAttempts =
                        hardPreset ? 3 : 1;
                    for (int attempt = 0;
                         attempt < seedAttempts;
                         ++attempt) {
                        try {
                            return generateBoard(options);
                        } catch (const std::runtime_error&) {
                            if (attempt + 1 >= seedAttempts) {
                                throw;
                            }
                            options.seed += 0x9E3779B9U;
                        }
                    }
                    throw std::runtime_error(
                        "Level generation exhausted its retries"
                    );
                }
            );
            generationInProgress = true;
        } catch (const std::exception& error) {
            generationInProgress = false;
            generationError = error.what();
            TraceLog(
                LOG_ERROR,
                "Could not start level generation: %s",
                error.what()
            );
        } catch (...) {
            generationInProgress = false;
            generationError =
                "Could not start the generation worker";
            TraceLog(
                LOG_ERROR,
                "Could not start the level generation worker"
            );
        }
    };

    const auto restoreSavedGame =
        [&](SavedGame restored) {
            if (!isValidSavedGame(restored)) {
                return false;
            }
            const int restoredLives = restored.lives;
            const int restoredColumns = restored.columns;
            const int restoredRows = restored.rows;
            board = Board(
                restoredColumns,
                restoredRows,
                std::move(restored.arrows)
            );
            updateBoardDimensions();
            resetRuntime(&restored);
            lives = restoredLives;
            currentGeneratorOptions =
                optionsForDimensions(
                    restoredColumns,
                    restoredRows
                );
            activeGame = true;
            generationError.clear();
            screen = Screen::Playing;
            resetCamera();
            renderedFrames = 0;
            return true;
        };

    const auto persistCurrentGame = [&]() {
        if (!activeGame || generationInProgress ||
            board.arrows().empty()) {
            return false;
        }
        const std::size_t remaining =
            static_cast<std::size_t>(std::ranges::count_if(
                arrows,
                [](const RuntimeArrow& value) {
                    return value.present;
                }
            ));
        if (remaining == 0 || lives <= 0) {
            const bool removed = persistence.removeSavedGame();
            if (removed) {
                resumeAvailable = false;
                savedGame.reset();
            }
            return removed;
        }

        SavedGame snapshot;
        snapshot.columns = board.columns();
        snapshot.rows = board.rows();
        snapshot.lives = lives;
        snapshot.arrows = board.arrows();
        snapshot.present.reserve(arrows.size());
        snapshot.failed.reserve(arrows.size());
        for (const RuntimeArrow& value : arrows) {
            snapshot.present.push_back(value.present);
            snapshot.failed.push_back(value.failed);
        }
        const bool saved = persistence.saveSavedGame(snapshot);
        if (saved) {
            resumeAvailable = true;
            savedGame.reset();
        }
        return saved;
    };

    if (!automatedScreenshot.empty() &&
        automatedMenuScreen == 0) {
        if (automatedResume &&
            savedGame.has_value() &&
            restoreSavedGame(std::move(*savedGame))) {
            applyAutomatedInitialSetup();
        } else {
            const std::size_t difficultyIndex =
                static_cast<std::size_t>(std::clamp(
                    automatedDifficulty,
                    std::uint32_t{1},
                    static_cast<std::uint32_t>(
                        kDifficultyPresets.size()
                    )
                ) - 1U);
            const DifficultyPreset& preset =
                kDifficultyPresets[difficultyIndex];
            currentGeneratorOptions = {
                preset.columns,
                preset.rows,
                2,
                preset.maximumLength,
                seed++,
                preset.candidateAttempts,
                settings.arrowLoops
            };
            startGeneration(currentGeneratorOptions);
        }
    }

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
        [&movingContacts](std::size_t arrowIndex) {
            for (auto contact = movingContacts.begin();
                 contact != movingContacts.end();) {
                if (movingContactContains(
                        *contact,
                        arrowIndex
                    )) {
                    contact = movingContacts.erase(contact);
                } else {
                    ++contact;
                }
            }
        };

    const auto launchArrow = [&](
        std::size_t arrowIndex
    ) {
        RuntimeArrow& value = arrows[arrowIndex];
        clearMovingContactsFor(arrowIndex);
        value.motion = Motion::Forward;
        value.progress = 0.0;
        value.speed = kInitialForwardSpeed;
        value.launchDelay = forwardTimeDebt;
        value.movingCollisionTint = false;
        value.history.clear();
        value.targetCells.clear();
        invalidateStaticBoard();
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
            {board.columns(), board.rows()},
            false
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
                {board.columns(), board.rows()},
                false
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
            if (!settings.flyingArrowCollisions) {
                movingContacts.clear();
                return false;
            }
            std::vector<std::size_t> activeIndices;
            std::vector<MotionBodyView> bodies;
            activeIndices.reserve(arrows.size());
            bodies.reserve(arrows.size());
            for (std::size_t index = 0;
                 index < arrows.size();
                 ++index) {
                const RuntimeArrow& value = arrows[index];
                const bool activeForward =
                    isActiveForward(value);
                if (!activeForward) {
                    continue;
                }
                activeIndices.push_back(index);
                bodies.push_back({
                    value.cells,
                    value.targetCells,
                    {},
                    true,
                    completing[index]
                        ? MotionRole::Advancing
                        : MotionRole::Moving,
                    false
                });
            }
            const std::size_t count = bodies.size();
            std::vector<std::uint8_t> previousContacts(
                count * count,
                0
            );
            for (std::size_t first = 0;
                 first < count;
                 ++first) {
                for (std::size_t second = first + 1;
                     second < count;
                     ++second) {
                    if (!movingContacts.contains(
                            movingContactKey(
                                activeIndices[first],
                                activeIndices[second]
                            )
                        )) {
                        continue;
                    }
                    previousContacts[first * count + second] = 1;
                    previousContacts[second * count + first] = 1;
                }
            }
            MotionResolution resolution = resolveMotionStep(
                bodies,
                previousContacts,
                {board.columns(), board.rows()}
            );
            std::unordered_set<std::uint64_t> nextContacts;
            for (std::size_t first = 0;
                 first < count;
                 ++first) {
                for (std::size_t second = first + 1;
                     second < count;
                     ++second) {
                    const std::size_t pair =
                        first * count + second;
                    if (!resolution.movingContacts[pair]) {
                        continue;
                    }
                    const std::uint64_t key = movingContactKey(
                        activeIndices[first],
                        activeIndices[second]
                    );
                    nextContacts.insert(key);
                    if (!previousContacts[pair]) {
                        arrows[activeIndices[first]]
                            .movingCollisionTint = true;
                        arrows[activeIndices[second]]
                            .movingCollisionTint = true;
                    }
                }
            }
            movingContacts = std::move(nextContacts);
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

    const auto rebuildStaticBoardLayer = [&]() {
        const int width = std::max(1, GetScreenWidth());
        const int height = std::max(1, GetScreenHeight());
        if (staticBoardLayer.id == 0 ||
            staticBoardLayerWidth != width ||
            staticBoardLayerHeight != height) {
            if (staticBoardLayer.id != 0) {
                UnloadRenderTexture(staticBoardLayer);
            }
            staticBoardLayer = LoadRenderTexture(width, height);
            staticBoardLayerWidth = width;
            staticBoardLayerHeight = height;
            staticBoardLayerDirty = true;
        }
        if (!staticBoardLayerDirty ||
            staticBoardLayer.id == 0) {
            return;
        }

        if (staticBoardGeometryDirty) {
            cacheIdleBoardGeometry(staticBoardGeometry, board, arrows);
            staticBoardGeometryDirty = false;
        }

        BeginTextureMode(staticBoardLayer);
        ClearBackground(RAYWHITE);
        BeginMode2D(camera);
        DrawRectangleRounded(
            {0.0F, 0.0F, boardWidth, boardHeight},
            0.035F,
            8,
            {248, 250, 253, 255}
        );
        staticBoardGeometry.draw(camera.zoom);
        DrawRectangleRoundedLinesEx(
            {0.0F, 0.0F, boardWidth, boardHeight},
            0.035F,
            8,
            2.0F,
            kSoft
        );
        EndMode2D();
        EndTextureMode();
        staticBoardLayerDirty = false;
    };

    FramePacer framePacer;
    std::optional<Screen> lastDrawnScreen;
    std::optional<MenuFrameState> lastMenuFrame;
    std::optional<PlayingFrameState> lastPlayingFrame;
    while (!WindowShouldClose() && !closeRequested) {
        const float delta = framePacer.beginFrame();

        if (generationInProgress &&
            (generationOverlayRendered ||
             screen != Screen::Playing) &&
            generationFuture.wait_for(
                std::chrono::seconds{0}
            ) == std::future_status::ready) {
            generationInProgress = false;
            try {
                Board generated = generationFuture.get();
                board = std::move(generated);
                updateBoardDimensions();
                resetRuntime();
                lives = 3;
                activeGame = true;
                if (persistence.removeSavedGame()) {
                    resumeAvailable = false;
                    savedGame.reset();
                }
                resetCamera();
                renderedFrames = 0;
                applyAutomatedInitialSetup();
                if (screen != Screen::Playing) {
                    (void)persistCurrentGame();
                }
            } catch (const std::exception& error) {
                generationError = error.what();
                if (!automatedScreenshot.empty()) {
                    automatedInitialSetupPending = false;
                }
                TraceLog(
                    LOG_ERROR,
                    "Level generation failed: %s",
                    error.what()
                );
            } catch (...) {
                generationError = "Unknown generation error";
                if (!automatedScreenshot.empty()) {
                    automatedInitialSetupPending = false;
                }
                TraceLog(
                    LOG_ERROR,
                    "Level generation failed with an unknown error"
                );
            }
        }
        const bool generationBlocksInput =
            generationInProgress || !generationError.empty();

        if (IsKeyPressed(KEY_F11) ||
            (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
            ToggleFullscreen();
            staticBoardLayerDirty = true;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (IsWindowFullscreen()) {
                ToggleFullscreen();
                staticBoardLayerDirty = true;
            } else if (screen == Screen::Playing) {
                (void)persistCurrentGame();
                generationError.clear();
                pressedArrow.reset();
                pressedMenuButton.reset();
                focusedMenuButton = 0;
                screen = Screen::MainMenu;
            } else if (screen != Screen::MainMenu) {
                pressedMenuButton.reset();
                focusedMenuButton = 0;
                screen = Screen::MainMenu;
            } else {
                closeRequested = true;
            }
        }
        if (IsKeyPressed(KEY_F12)) {
            TakeScreenshot("arrow-puzzle-screenshot.png");
        }

        if (screen != Screen::Playing) {
            const Screen menuScreen = screen;
            const int menuItemCount =
                menuScreen == Screen::MainMenu
                ? 4
                : menuScreen == Screen::NewGameMenu
                    ? 5
                    : 3;
            const auto menuItemEnabled = [&](int index) {
                if (index < 0 || index >= menuItemCount) {
                    return false;
                }
                if (menuScreen == Screen::MainMenu) {
                    if (index == 0) {
                        return !generationInProgress;
                    }
                    if (index == 1) {
                        return resumeAvailable &&
                               !generationInProgress;
                    }
                }
                return true;
            };
            const auto firstEnabledMenuItem = [&]() {
                for (int index = 0;
                     index < menuItemCount;
                     ++index) {
                    if (menuItemEnabled(index)) {
                        return index;
                    }
                }
                return 0;
            };
            if (focusedMenuButton < 0 ||
                focusedMenuButton >= menuItemCount ||
                !menuItemEnabled(focusedMenuButton)) {
                focusedMenuButton = firstEnabledMenuItem();
            }
            const auto moveMenuFocus = [&](int direction) {
                for (int distance = 1;
                     distance <= menuItemCount;
                     ++distance) {
                    const int candidate =
                        (focusedMenuButton +
                         direction * distance +
                         menuItemCount * menuItemCount) %
                        menuItemCount;
                    if (menuItemEnabled(candidate)) {
                        focusedMenuButton = candidate;
                        return;
                    }
                }
            };

            const bool keyboardUp = IsKeyPressed(KEY_UP);
            const bool keyboardDown = IsKeyPressed(KEY_DOWN);
            if (keyboardUp) {
                moveMenuFocus(-1);
                pressedMenuButton.reset();
            }
            if (keyboardDown) {
                moveMenuFocus(1);
                pressedMenuButton.reset();
            }

            const bool mouseDown =
                IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            const bool mousePressed =
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            const bool mouseReleased =
                pressedMenuButton.has_value() &&
                !mouseDown &&
                !mousePressed;
            const auto menuItemAtPoint =
                [&](Vector2 point) -> std::optional<int> {
                    for (int index = 0;
                         index < menuItemCount;
                         ++index) {
                        if (menuItemEnabled(index) &&
                            CheckCollisionPointRec(
                                point,
                                menuButtonRectangle(
                                    index,
                                    menuItemCount
                                )
                            )) {
                            return index;
                        }
                    }
                    return std::nullopt;
                };

            std::optional<int> activatedMenuButton;
            if (!IsWindowFocused()) {
                pressedMenuButton.reset();
            } else {
                if (mousePressed) {
                    const std::optional<int> hovered =
                        menuItemAtPoint(GetMousePosition());
                    if (hovered.has_value()) {
                        focusedMenuButton = *hovered;
                        pressedMenuButton =
                            static_cast<int>(menuScreen) * 16 +
                            *hovered;
                    } else {
                        pressedMenuButton.reset();
                    }
                }
                if (mouseReleased) {
                    const std::optional<int> hovered =
                        menuItemAtPoint(GetMousePosition());
                    if (hovered.has_value() &&
                        pressedMenuButton ==
                            static_cast<int>(menuScreen) * 16 +
                                *hovered) {
                        activatedMenuButton = *hovered;
                    }
                    pressedMenuButton.reset();
                }

                const bool altDown =
                    IsKeyDown(KEY_LEFT_ALT) ||
                    IsKeyDown(KEY_RIGHT_ALT);
                const bool enterPressed =
                    !altDown &&
                    (IsKeyPressed(KEY_ENTER) ||
                     IsKeyPressed(KEY_KP_ENTER));
                const bool spacePressed =
                    IsKeyPressed(KEY_SPACE);
                if (enterPressed &&
                    menuItemEnabled(focusedMenuButton)) {
                    activatedMenuButton =
                        focusedMenuButton;
                    pressedMenuButton.reset();
                } else if (
                    spacePressed &&
                    menuScreen == Screen::SettingsMenu &&
                    focusedMenuButton <= 1) {
                    activatedMenuButton =
                        focusedMenuButton;
                    pressedMenuButton.reset();
                }
            }
            const MenuFrameState menuFrame{
                menuScreen,
                GetScreenWidth(),
                GetScreenHeight(),
                focusedMenuButton,
                pressedMenuButton,
                menuItemAtPoint(GetMousePosition()),
                settings,
                resumeAvailable,
                generationInProgress,
                IsWindowFocused()
            };
            if (automatedScreenshot.empty() &&
                delta < 0.1F &&
                !activatedMenuButton.has_value() &&
                lastDrawnScreen == menuScreen &&
                lastMenuFrame == menuFrame) {
                PollInputEvents();
                continue;
            }
            lastMenuFrame = menuFrame;

            const auto menuButton = [&](
                int index,
                Rectangle bounds,
                const char* label,
                bool enabled = true
            ) {
                const int id =
                    static_cast<int>(menuScreen) * 16 + index;
                drawMenuButton(
                    uiFont,
                    bounds,
                    label,
                    enabled,
                    enabled &&
                        focusedMenuButton == index,
                    pressedMenuButton == id
                );
                return enabled &&
                       activatedMenuButton == index;
            };

            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawRectangleGradientV(
                0,
                0,
                GetScreenWidth(),
                GetScreenHeight(),
                RAYWHITE,
                Color{247, 249, 252, 255}
            );

            if (screen == Screen::MainMenu) {
                drawMenuHeading(uiFont, "MAIN MENU");
                constexpr int buttonCount = 4;
                if (menuButton(
                        0,
                        menuButtonRectangle(0, buttonCount),
                        "New game",
                        !generationInProgress
                    )) {
                    screen = Screen::NewGameMenu;
                }
                if (menuButton(
                        1,
                        menuButtonRectangle(1, buttonCount),
                        "Resume game",
                        resumeAvailable &&
                            !generationInProgress
                    )) {
                    std::optional<SavedGame> restored =
                        persistence.loadSavedGame();
                    if (!restored.has_value() ||
                        !restoreSavedGame(
                            std::move(*restored)
                        )) {
                        resumeAvailable = false;
                        savedGame.reset();
                    }
                }
                if (menuButton(
                        2,
                        menuButtonRectangle(2, buttonCount),
                        "Settings"
                    )) {
                    screen = Screen::SettingsMenu;
                }
                if (menuButton(
                        3,
                        menuButtonRectangle(3, buttonCount),
                        "Exit"
                    )) {
                    (void)persistCurrentGame();
                    closeRequested = true;
                }
                if (generationInProgress) {
                    drawCenteredText(
                        uiFont,
                        "Level generation is finishing in the background",
                        static_cast<float>(GetScreenWidth()) * 0.5F,
                        static_cast<float>(GetScreenHeight()) - 54.0F,
                        kMenuHintText,
                        kMuted
                    );
                } else {
                    drawCenteredText(
                        uiFont,
                        "Up / Down navigate    Enter select",
                        static_cast<float>(GetScreenWidth()) * 0.5F,
                        static_cast<float>(GetScreenHeight()) - 54.0F,
                        kMenuHintText,
                        kMuted
                    );
                }
            } else if (screen == Screen::NewGameMenu) {
                drawMenuHeading(uiFont, "NEW GAME");
                constexpr int buttonCount = 5;
                for (int index = 0;
                     index <
                         static_cast<int>(
                             kDifficultyPresets.size()
                         );
                     ++index) {
                    const DifficultyPreset& preset =
                        kDifficultyPresets[
                            static_cast<std::size_t>(index)
                        ];
                    if (!menuButton(
                            index,
                            menuButtonRectangle(
                                index,
                                buttonCount
                            ),
                            preset.label
                        )) {
                        continue;
                    }
                    GeneratorOptions options{
                        preset.columns,
                        preset.rows,
                        2,
                        preset.maximumLength,
                        seed++,
                        preset.candidateAttempts,
                        settings.arrowLoops
                    };
                    startGeneration(options);
                }
                if (menuButton(
                        4,
                        menuButtonRectangle(4, buttonCount),
                        "Back"
                    )) {
                    screen = Screen::MainMenu;
                }
                drawCenteredText(
                    uiFont,
                    "Up / Down navigate    Enter select    Esc back",
                    static_cast<float>(GetScreenWidth()) * 0.5F,
                    static_cast<float>(GetScreenHeight()) - 54.0F,
                    kMenuHintText,
                    kMuted
                );
            } else if (screen == Screen::SettingsMenu) {
                drawMenuHeading(uiFont, "SETTINGS");
                constexpr int buttonCount = 3;
                const std::string loopsLabel =
                    std::string{"Arrow loops  ["} +
                    (settings.arrowLoops ? "ON" : "OFF") +
                    "]";
                if (menuButton(
                        0,
                        menuButtonRectangle(0, buttonCount),
                        loopsLabel.c_str()
                    )) {
                    if (!persistence.setArrowLoops(
                            settings,
                            !settings.arrowLoops
                        )) {
                        TraceLog(
                            LOG_WARNING,
                            "Could not save arrow loop setting"
                        );
                    }
                }
                const std::string collisionLabel =
                    std::string{"Flying arrow collisions  ["} +
                    (settings.flyingArrowCollisions
                         ? "ON"
                         : "OFF") +
                    "]";
                if (menuButton(
                        1,
                        menuButtonRectangle(1, buttonCount),
                        collisionLabel.c_str()
                    )) {
                    if (!persistence.setFlyingArrowCollisions(
                            settings,
                            !settings.flyingArrowCollisions
                        )) {
                        TraceLog(
                            LOG_WARNING,
                            "Could not save flying collision setting"
                        );
                    }
                }
                if (menuButton(
                        2,
                        menuButtonRectangle(2, buttonCount),
                        "Back"
                    )) {
                    screen = Screen::MainMenu;
                }
                drawCenteredText(
                    uiFont,
                    "Up / Down navigate    Enter / Space toggle"
                    "    Saved automatically",
                    static_cast<float>(GetScreenWidth()) * 0.5F,
                    static_cast<float>(GetScreenHeight()) - 54.0F,
                    kMenuHintText,
                    kMuted
                );
            }

            if (screen != menuScreen) {
                pressedMenuButton.reset();
                focusedMenuButton = 0;
            }
            EndDrawing();
            lastDrawnScreen = menuScreen;
            ++renderedFrames;
            if (!automatedScreenshot.empty() &&
                automatedMenuScreen != 0 &&
                renderedFrames >= automatedScreenshotFrame) {
                TakeScreenshot(automatedScreenshot.c_str());
                closeRequested = true;
            }
            continue;
        }

        camera.offset = {
            static_cast<float>(GetScreenWidth()) * 0.5F,
            kHudHeight +
                (static_cast<float>(GetScreenHeight()) - kHudHeight) * 0.5F
        };

        const float wheel = GetMouseWheelMove();
        if (!generationBlocksInput && wheel != 0.0F) {
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
            staticBoardLayerDirty = true;
        }

        if (!generationBlocksInput &&
            IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            const Vector2 movement = GetMouseDelta();
            if (movement.x != 0.0F || movement.y != 0.0F) {
                camera.target.x -= movement.x / camera.zoom;
                camera.target.y -= movement.y / camera.zoom;
                staticBoardLayerDirty = true;
            }
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
        if (IsKeyPressed(KEY_R) &&
            !anyArrowInMotion &&
            !generationInProgress) {
            currentGeneratorOptions.seed = seed++;
            currentGeneratorOptions.allowArrowLoops =
                settings.arrowLoops;
            startGeneration(currentGeneratorOptions);
        }

        if (!automatedScreenshot.empty() &&
            !automatedSecondLaunchDone &&
            automatedSecondLaunch.has_value() &&
            !generationInProgress &&
            generationError.empty() &&
            renderedFrames >= automatedSecondLaunchFrame) {
            automatedSecondLaunchDone = true;
            if (*automatedSecondLaunch < arrows.size()) {
                RuntimeArrow& value =
                    arrows[*automatedSecondLaunch];
                if (value.present &&
                    value.motion == Motion::Idle) {
                    launchArrow(*automatedSecondLaunch);
                }
            }
        }

        const bool launchInputAvailable =
            IsWindowFocused() &&
            lives > 0 &&
            !generationInProgress &&
            generationError.empty() &&
            !anyArrowRecovering &&
            movingCollisionPauseRemaining <= 0.0F;
        const bool leftPressed =
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const bool leftReleased =
            IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        const bool leftDown =
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        if (!launchInputAvailable ||
            (!leftDown && !leftReleased)) {
            pressedArrow.reset();
        }

        if (leftPressed && launchInputAvailable) {
            const Vector2 world = GetScreenToWorld2D(
                GetMousePosition(),
                camera
            );
            pressedArrow = hitTest(world, arrows);
        }

        if (leftReleased) {
            const std::optional<std::size_t> armedArrow =
                std::exchange(pressedArrow, std::nullopt);
            if (armedArrow.has_value() &&
                launchInputAvailable) {
                const Vector2 world = GetScreenToWorld2D(
                    GetMousePosition(),
                    camera
                );
                const std::optional<std::size_t> releasedArrow =
                    hitTest(world, arrows);
                if (releasedArrow == armedArrow) {
                    launchArrow(*armedArrow);
                }
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
                        invalidateStaticBoard();
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
                    invalidateStaticBoard();
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
            movingContacts.clear();
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
                movingContacts.clear();
            }
        }

        const int remaining = static_cast<int>(std::ranges::count_if(
            arrows,
            [](const RuntimeArrow& value) { return value.present; }
        ));
        if (activeGame &&
            (remaining == 0 || lives <= 0) &&
            resumeAvailable &&
            persistence.removeSavedGame()) {
            resumeAvailable = false;
            savedGame.reset();
        }

        const PlayingFrameState playingFrame{
            GetScreenWidth(),
            GetScreenHeight(),
            lives,
            remaining,
            generationInProgress,
            IsWindowFocused(),
            generationError
        };
        if (automatedScreenshot.empty() &&
            delta < 0.1F &&
            !generationInProgress &&
            !anyArrowInMotion &&
            !staticBoardLayerDirty &&
            lastDrawnScreen == Screen::Playing &&
            lastPlayingFrame == playingFrame) {
            PollInputEvents();
            continue;
        }
        lastPlayingFrame = playingFrame;
        rebuildStaticBoardLayer();

        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (staticBoardLayer.id != 0) {
            DrawTextureRec(
                staticBoardLayer.texture,
                {
                    0.0F,
                    0.0F,
                    static_cast<float>(staticBoardLayerWidth),
                    -static_cast<float>(staticBoardLayerHeight)
                },
                {0.0F, 0.0F},
                WHITE
            );
        }

        DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(kHudHeight), RAYWHITE);
        DrawLine(
            0,
            static_cast<int>(kHudHeight),
            GetScreenWidth(),
            static_cast<int>(kHudHeight),
            kSoft
        );
        const Vector2 brandSize = measureText(
            uiFont,
            "ARROW PUZZLE",
            kBrandText
        );
        drawText(
            uiFont,
            "ARROW PUZZLE",
            {
                28.0F,
                (kHudHeight - brandSize.y) * 0.5F
            },
            kBrandText,
            kInk
        );
        const std::string countText =
            generationInProgress
            ? "GENERATING"
            : !generationError.empty()
                ? "GENERATION FAILED"
                : "ARROWS  " + std::to_string(remaining);
        const Vector2 countSize = measureText(
            uiFont,
            countText.c_str(),
            kHudText
        );
        drawText(
            uiFont,
            countText.c_str(),
            {
                static_cast<float>(GetScreenWidth()) * 0.5F -
                    countSize.x * 0.5F,
                (kHudHeight - countSize.y) * 0.5F
            },
            kHudText,
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
        staticBoardGeometry.beginDynamic(camera.zoom);
        for (const RuntimeArrow& value : arrows) {
            if (value.present &&
                value.motion != Motion::Idle) {
                drawArrow(staticBoardGeometry, value);
            }
        }
        staticBoardGeometry.endDynamic();
        EndMode2D();

        if (generationInProgress) {
            DrawRectangle(
                0,
                static_cast<int>(kHudHeight),
                GetScreenWidth(),
                GetScreenHeight() -
                    static_cast<int>(kHudHeight),
                {248, 250, 253, 224}
            );
            const Vector2 spinnerCenter{
                static_cast<float>(GetScreenWidth()) * 0.5F,
                kHudHeight +
                    (static_cast<float>(GetScreenHeight()) -
                     kHudHeight) *
                        0.5F -
                    34.0F
            };
            const float spinnerAngle = static_cast<float>(
                std::fmod(GetTime() * 210.0, 360.0)
            );
            DrawRing(
                spinnerCenter,
                17.0F,
                23.0F,
                spinnerAngle,
                spinnerAngle + 275.0F,
                48,
                kBlue
            );
            const char* title = "GENERATING LEVEL";
            drawCenteredText(
                uiFont,
                title,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                spinnerCenter.y + 43.0F,
                kOverlayTitleText,
                kInk
            );
            const char* detail =
                "Searching for an interesting puzzle...";
            drawCenteredText(
                uiFont,
                detail,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                spinnerCenter.y + 79.0F,
                kOverlayDetailText,
                kMuted
            );
            generationOverlayRendered = true;
        } else if (!generationError.empty()) {
            DrawRectangle(
                0,
                static_cast<int>(kHudHeight),
                GetScreenWidth(),
                GetScreenHeight() -
                    static_cast<int>(kHudHeight),
                {248, 250, 253, 230}
            );
            const int centerY =
                (GetScreenHeight() +
                 static_cast<int>(kHudHeight)) /
                2;
            const char* title = "GENERATION FAILED";
            drawCenteredText(
                uiFont,
                title,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(centerY) - 58.0F,
                kErrorTitleText,
                kRed
            );
            const char* retry =
                "Press R to try another level  /  Esc for menu";
            drawCenteredText(
                uiFont,
                retry,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(centerY) - 10.0F,
                kErrorActionText,
                kInk
            );
            std::string errorDetail = generationError;
            constexpr std::size_t maximumErrorLength = 90;
            if (errorDetail.size() > maximumErrorLength) {
                errorDetail.resize(maximumErrorLength - 3);
                errorDetail += "...";
            }
            drawCenteredText(
                uiFont,
                errorDetail.c_str(),
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(centerY) + 31.0F,
                kErrorDetailText,
                kMuted
            );
        } else if (remaining == 0) {
            const char* text =
                "FIELD CLEARED!  Press R for a new puzzle or Esc for menu";
            const Vector2 textSize = measureText(
                uiFont,
                text,
                kBannerText
            );
            DrawRectangleRounded(
                {
                    GetScreenWidth() * 0.5F -
                        textSize.x * 0.5F -
                        24.0F,
                    GetScreenHeight() - 84.0F,
                    textSize.x + 48.0F,
                    52.0F
                },
                0.25F,
                8,
                Fade(RAYWHITE, 0.94F)
            );
            drawCenteredText(
                uiFont,
                text,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(GetScreenHeight()) - 70.0F,
                kBannerText,
                kBlue
            );
        } else if (lives == 0) {
            const char* text = "NO LIVES LEFT  -  Press R to restart";
            drawCenteredText(
                uiFont,
                text,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(GetScreenHeight()) - 48.0F,
                kBannerText,
                kRed
            );
        } else {
            const char* help =
                "LMB launch   Wheel zoom   MMB pan   F11 fullscreen   Esc menu";
            drawCenteredText(
                uiFont,
                help,
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(GetScreenHeight()) - 30.0F,
                kHelpText,
                kMuted
            );
        }

        EndDrawing();

        lastDrawnScreen = Screen::Playing;

        ++renderedFrames;
        if (!automatedScreenshot.empty() &&
            !automatedInitialSetupPending &&
            !generationInProgress &&
            renderedFrames >= automatedScreenshotFrame) {
            TakeScreenshot(automatedScreenshot.c_str());
            closeRequested = true;
        }
    }

    if (automatedScreenshot.empty() ||
        automatedSaveOnExit) {
        (void)persistCurrentGame();
    }
    if (generationFuture.valid()) {
        while (generationFuture.wait_for(
                   std::chrono::milliseconds{16}
               ) != std::future_status::ready) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawRectangleGradientV(
                0,
                0,
                GetScreenWidth(),
                GetScreenHeight(),
                RAYWHITE,
                Color{247, 249, 252, 255}
            );
            const Vector2 spinnerCenter{
                static_cast<float>(GetScreenWidth()) * 0.5F,
                static_cast<float>(GetScreenHeight()) * 0.5F -
                    42.0F
            };
            const float spinnerAngle = static_cast<float>(
                std::fmod(GetTime() * 210.0, 360.0)
            );
            DrawRing(
                spinnerCenter,
                17.0F,
                23.0F,
                spinnerAngle,
                spinnerAngle + 275.0F,
                48,
                kBlue
            );
            drawCenteredText(
                uiFont,
                "FINISHING LEVEL GENERATION",
                static_cast<float>(GetScreenWidth()) * 0.5F,
                spinnerCenter.y + 43.0F,
                kOverlayTitleText,
                kInk
            );
            drawCenteredText(
                uiFont,
                "The game will close automatically...",
                static_cast<float>(GetScreenWidth()) * 0.5F,
                spinnerCenter.y + 79.0F,
                kOverlayDetailText,
                kMuted
            );
            EndDrawing();
        }
        try {
            static_cast<void>(generationFuture.get());
        } catch (...) {
            // Generation errors no longer matter during shutdown, but
            // consuming the future keeps the worker lifetime deterministic.
        }
    }
    if (ownsUiFont) {
        UnloadFont(uiFont);
    }
    if (staticBoardLayer.id != 0) {
        UnloadRenderTexture(staticBoardLayer);
    }
    staticBoardGeometry.release();
    CloseWindow();
    return 0;
}

} // namespace arrow
