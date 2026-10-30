#pragma once

#include <cstddef>
#include <optional>

namespace arrow {

// A board press becomes either a click or a drag. Once it crosses the screen-
// space threshold, returning to the press position cannot turn it into a click.
// Disabling panning suppresses displacement while retaining this launch guard.
class BoardPointerGesture {
public:
    struct Movement {
        float x{};
        float y{};
    };

    void reset() { *this = BoardPointerGesture{}; }

    void begin(float x, float y, std::optional<std::size_t> arrow, bool panEnabled = true) {
        reset();
        active_ = true;
        panEnabled_ = panEnabled;
        startX_ = previousX_ = x;
        startY_ = previousY_ = y;
        pressedArrow_ = arrow;
    }

    [[nodiscard]] Movement move(float x, float y) {
        if (!active_) return {};

        const float dx = x - startX_;
        const float dy = y - startY_;
        const bool wasDragging = dragging_;
        if (dx * dx + dy * dy > kDragThreshold * kDragThreshold) {
            dragging_ = true;
            cancelLaunch();
        }
        Movement movement;
        if (dragging_ && panEnabled_) {
            // Include the initial displacement when the drag first starts.
            movement = wasDragging ? Movement{x - previousX_, y - previousY_}
                                   : Movement{dx, dy};
        }
        previousX_ = x;
        previousY_ = y;
        return movement;
    }

    void cancelLaunch() { pressedArrow_.reset(); }

    [[nodiscard]] std::optional<std::size_t> release(
        std::optional<std::size_t> hoveredArrow
    ) {
        const auto launched = active_ && !dragging_ && pressedArrow_ == hoveredArrow
            ? pressedArrow_ : std::nullopt;
        reset();
        return launched;
    }

private:
    static constexpr float kDragThreshold = 6.0F;
    bool active_ = false;
    bool dragging_ = false;
    bool panEnabled_ = true;
    float startX_{};
    float startY_{};
    float previousX_{};
    float previousY_{};
    std::optional<std::size_t> pressedArrow_;
};

} // namespace arrow
