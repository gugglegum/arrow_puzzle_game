#pragma once

#include <array>
#include <optional>

namespace arrow {

// Hover temporarily overrides the keyboard bookmark. Only actual pointer
// movement (or a click) takes ownership back after keyboard navigation.
class MenuSelection {
public:
    void reset() { *this = MenuSelection{}; }

    void updatePointer(float x, float y, std::optional<int> hovered, bool active) {
        if (!active) {
            pointerPosition_.reset();
            hovered_.reset();
            pointerOwnsSelection_ = false;
            return;
        }
        const std::array position{x, y};
        if (pointerPosition_ != position) pointerOwnsSelection_ = true;
        pointerPosition_ = position;
        hovered_ = hovered;
    }

    [[nodiscard]] int selected() const {
        return pointerOwnsSelection_ && hovered_ ? *hovered_ : keyboardItem_;
    }

    void usePointer() { pointerOwnsSelection_ = true; }

    void useKeyboard() {
        keyboardItem_ = selected();
        pointerOwnsSelection_ = false;
    }

    template<class Enabled>
    void validate(int count, const Enabled& enabled) {
        const auto valid = [&](int index) {
            return index >= 0 && index < count && enabled(index);
        };
        if (!valid(keyboardItem_)) {
            keyboardItem_ = 0;
            for (int index = 0; index < count; ++index) {
                if (enabled(index)) {
                    keyboardItem_ = index;
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
        useKeyboard();
        if (count <= 0 || direction == 0) return;
        const int step = direction < 0 ? -1 : 1;
        for (int distance = 1; distance <= count; ++distance) {
            const int candidate = (start + step * distance + count) % count;
            if (enabled(candidate)) {
                keyboardItem_ = candidate;
                break;
            }
        }
    }

private:
    int keyboardItem_ = 0;
    std::optional<std::array<float, 2>> pointerPosition_;
    std::optional<int> hovered_;
    bool pointerOwnsSelection_ = false;
};

} // namespace arrow
