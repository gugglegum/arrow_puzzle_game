#include "arrow/Hints.hpp"

namespace arrow {

std::vector<std::size_t> hintCandidates(const Board& board, const std::vector<bool>& present) {
    const auto& arrows = board.arrows();
    if (present.size() != arrows.size()) return {};
    struct Occupant {
        std::size_t arrow;
        std::size_t vacatesAt;
    };
    std::vector<Occupant> occupied(static_cast<std::size_t>(board.columns()) * board.rows(), {arrows.size(), 0});
    const auto offset = [&](Cell cell) { return static_cast<std::size_t>(cell.y) * board.columns() + cell.x; };
    for (std::size_t index = 0; index < arrows.size(); ++index) {
        if (!present[index]) continue;
        const auto& cells = arrows[index].cells;
        for (std::size_t part = 0; part < cells.size(); ++part) {
            if (board.isInside(cells[part])) occupied[offset(cells[part])] = {index, cells.size() - part};
        }
    }
    std::vector<std::size_t> candidates;
    for (std::size_t index = 0; index < arrows.size(); ++index) {
        const Arrow& arrow = arrows[index];
        if (!present[index] || arrow.cells.empty() || !isCardinal(arrow.direction)) continue;
        Cell head = arrow.cells.front() + arrow.direction;
        bool clear = true;
        for (std::size_t step = 1; board.isInside(head); ++step, head = head + arrow.direction) {
            const Occupant blocker = occupied[offset(head)];
            if (blocker.arrow != arrows.size() &&
                (blocker.arrow != index || step < blocker.vacatesAt)) {
                clear = false;
                break;
            }
        }
        if (clear) candidates.push_back(index);
    }
    return candidates;
}

std::optional<std::size_t> chooseHint(const Board& board, const std::vector<bool>& present, std::mt19937& random) {
    const auto candidates = hintCandidates(board, present);
    if (candidates.empty()) return std::nullopt;
    return candidates[std::uniform_int_distribution<std::size_t>{0, candidates.size() - 1}(random)];
}

} // namespace arrow
