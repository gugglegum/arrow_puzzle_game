#pragma once

#include <array>
#include <optional>

namespace arrow {

// Mouse and keyboard share one selection. Leaving a row keeps it selected;
// only pointer movement over a row (or a click) changes it after navigation.
class MenuSelection {
public:
    void reset() { *this = MenuSelection{}; }

    void updatePointer(float x, float y, std::optional<int> hovered, bool active) {
        if (!active) {
            pointerPosition_.reset();
            hovered_.reset();
            return;
        }
        const std::array position{x, y};
        if (pointerPosition_ != position && hovered) selectedItem_ = *hovered;
        pointerPosition_ = position;
        hovered_ = hovered;
    }

    [[nodiscard]] int selected() const { return selectedItem_; }

    void usePointer() { if (hovered_) selectedItem_ = *hovered_; }

    template<class Enabled>
    void validate(int count, const Enabled& enabled) {
        const auto valid = [&](int index) {
            return index >= 0 && index < count && enabled(index);
        };
        if (!valid(selectedItem_)) {
            selectedItem_ = 0;
            for (int index = 0; index < count; ++index) {
                if (enabled(index)) {
                    selectedItem_ = index;
                    break;
                }
            }
        }
        if (hovered_ && !valid(*hovered_)) hovered_.reset();
    }

    template<class Enabled>
    void move(int direction, int count, const Enabled& enabled) {
        validate(count, enabled);
        const int start = selected();
        if (count <= 0 || direction == 0) return;
        const int step = direction < 0 ? -1 : 1;
        for (int distance = 1; distance <= count; ++distance) {
            const int candidate = (start + step * distance + count) % count;
            if (enabled(candidate)) {
                selectedItem_ = candidate;
                break;
            }
        }
    }

private:
    int selectedItem_ = 0;
    std::optional<std::array<float, 2>> pointerPosition_;
    std::optional<int> hovered_;
};

} // namespace arrow
