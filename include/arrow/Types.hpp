#pragma once

#include <compare>

namespace arrow {

struct Cell {
    int x{};
    int y{};

    auto operator<=>(const Cell&) const = default;
};

constexpr Cell operator+(Cell lhs, Cell rhs) noexcept {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

constexpr Cell operator-(Cell lhs, Cell rhs) noexcept {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

constexpr bool isCardinal(Cell value) noexcept {
    return (value.x == 0 && (value.y == -1 || value.y == 1)) ||
           (value.y == 0 && (value.x == -1 || value.x == 1));
}

} // namespace arrow
