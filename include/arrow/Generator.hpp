#pragma once

#include "arrow/Board.hpp"

#include <cstdint>

namespace arrow {

struct GeneratorOptions {
    int columns{20};
    int rows{20};
    int minimumLength{2};
    int maximumLength{30};
    std::uint32_t seed{0};
};

[[nodiscard]] Board generateBoard(const GeneratorOptions& options);

} // namespace arrow
