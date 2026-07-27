#pragma once

#include "arrow/Types.hpp"

#include <cstddef>
#include <vector>

namespace arrow {

struct Arrow {
    std::vector<Cell> cells; // Head first, tail last.
    Cell direction{};
};

class Board {
public:
    Board(int columns, int rows, std::vector<Arrow> arrows);

    [[nodiscard]] int columns() const noexcept;
    [[nodiscard]] int rows() const noexcept;
    [[nodiscard]] const std::vector<Arrow>& arrows() const noexcept;
    [[nodiscard]] bool isInside(Cell cell) const noexcept;

    [[nodiscard]] bool canExit(
        std::size_t arrowIndex,
        const std::vector<bool>& present
    ) const;

    [[nodiscard]] std::vector<std::size_t> solveGreedily() const;
    [[nodiscard]] bool isCompletelyFilled() const;
    [[nodiscard]] bool hasValidGeometry() const;

private:
    int columns_{};
    int rows_{};
    std::vector<Arrow> arrows_;
};

} // namespace arrow
