#pragma once

#include "arrow/Board.hpp"

#include <random>
#include <vector>

namespace arrow::detail {

// Partition a Hamiltonian cycle by removing currently escapable fragments in
// either direction. The returned arrows are in solution order. An empty result
// means this bounded construction got stuck; it is never a partial board.
[[nodiscard]] std::vector<Arrow> partitionFromFrontiers(
    const std::vector<Cell>& cycle,
    int columns,
    int rows,
    int maximumLength,
    bool allowArrowLoops,
    int initialChoiceTarget,
    int maximumInitialChoices,
    std::mt19937& random
);

} // namespace arrow::detail
