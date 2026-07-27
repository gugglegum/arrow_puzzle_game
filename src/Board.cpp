#include "arrow/Board.hpp"

#include <algorithm>
#include <stdexcept>

namespace arrow {

Board::Board(int columns, int rows, std::vector<Arrow> arrows)
    : columns_(columns), rows_(rows), arrows_(std::move(arrows)) {
    if (columns <= 0 || rows <= 0) {
        throw std::invalid_argument("Board dimensions must be positive");
    }
}

int Board::columns() const noexcept {
    return columns_;
}

int Board::rows() const noexcept {
    return rows_;
}

const std::vector<Arrow>& Board::arrows() const noexcept {
    return arrows_;
}

bool Board::isInside(Cell cell) const noexcept {
    return cell.x >= 0 && cell.x < columns_ &&
           cell.y >= 0 && cell.y < rows_;
}

bool Board::canExit(
    std::size_t arrowIndex,
    const std::vector<bool>& present
) const {
    if (arrowIndex >= arrows_.size() || present.size() != arrows_.size() ||
        !present[arrowIndex]) {
        return false;
    }

    const Arrow& candidate = arrows_[arrowIndex];
    if (candidate.cells.empty() || !isCardinal(candidate.direction)) {
        return false;
    }

    std::vector<bool> blocked(
        static_cast<std::size_t>(columns_ * rows_),
        false
    );
    for (std::size_t index = 0; index < arrows_.size(); ++index) {
        if (index == arrowIndex || !present[index]) {
            continue;
        }
        for (Cell cell : arrows_[index].cells) {
            if (isInside(cell)) {
                blocked[static_cast<std::size_t>(
                    cell.y * columns_ + cell.x
                )] = true;
            }
        }
    }

    std::vector<Cell> moving = candidate.cells;
    const int maximumSteps =
        static_cast<int>(moving.size()) +
        std::max(columns_, rows_) +
        1;

    for (int step = 0; step < maximumSteps; ++step) {
        if (std::ranges::none_of(moving, [this](Cell cell) {
                return isInside(cell);
            })) {
            return true;
        }

        const Cell nextHead = moving.front() + candidate.direction;
        if (isInside(nextHead)) {
            if (blocked[static_cast<std::size_t>(
                    nextHead.y * columns_ + nextHead.x
                )]) {
                return false;
            }

            // The tail cell is excluded because it is vacated on this same
            // simulation tick. This permits the deliberate "head follows a
            // disappearing tail" puzzle, but rejects every true self-hit.
            if (moving.size() > 1 &&
                std::ranges::find(
                    moving.begin(),
                    moving.end() - 1,
                    nextHead
                ) != moving.end() - 1) {
                return false;
            }
        }

        for (std::size_t index = moving.size() - 1; index > 0; --index) {
            moving[index] = moving[index - 1];
        }
        moving.front() = nextHead;
    }
    return false;
}

std::vector<std::size_t> Board::solveGreedily() const {
    std::vector<bool> present(arrows_.size(), true);
    std::vector<std::size_t> solution;
    solution.reserve(arrows_.size());

    bool progressed = true;
    while (progressed && solution.size() != arrows_.size()) {
        progressed = false;
        for (std::size_t index = 0; index < arrows_.size(); ++index) {
            if (canExit(index, present)) {
                present[index] = false;
                solution.push_back(index);
                progressed = true;
            }
        }
    }
    return solution;
}

bool Board::isCompletelyFilled() const {
    std::vector<int> occupancy(
        static_cast<std::size_t>(columns_ * rows_),
        0
    );

    for (const Arrow& value : arrows_) {
        for (Cell cell : value.cells) {
            if (!isInside(cell)) {
                return false;
            }
            const std::size_t index = static_cast<std::size_t>(
                cell.y * columns_ + cell.x
            );
            ++occupancy[index];
        }
    }

    return std::ranges::all_of(occupancy, [](int count) {
        return count == 1;
    });
}

bool Board::hasValidGeometry() const {
    for (const Arrow& value : arrows_) {
        if (value.cells.empty() || !isCardinal(value.direction)) {
            return false;
        }
        if (value.cells.size() > 1 &&
            value.cells[0] - value.cells[1] != value.direction) {
            return false;
        }

        for (std::size_t index = 1; index < value.cells.size(); ++index) {
            if (!isCardinal(value.cells[index] - value.cells[index - 1])) {
                return false;
            }
            if (std::ranges::find(
                    value.cells.begin(),
                    value.cells.begin() + static_cast<std::ptrdiff_t>(index),
                    value.cells[index]
                ) != value.cells.begin() + static_cast<std::ptrdiff_t>(index)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace arrow
