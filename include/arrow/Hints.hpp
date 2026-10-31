#pragma once

#include "arrow/Board.hpp"

#include <algorithm>
#include <optional>
#include <random>

namespace arrow {

class HintState {
public:
    static constexpr double delay = 20.0;

    void reset() { elapsed_ = 0.0; highlighted.reset(); }
    void advance(double seconds) { elapsed_ = std::clamp(elapsed_ + seconds, 0.0, delay); }
    [[nodiscard]] bool ready() const { return elapsed_ >= delay; }

    std::optional<std::size_t> highlighted;

private:
    double elapsed_{};
};

// Only use on a settled board: runtime arrows have returned to their original
// paths or left completely. Build occupancy once, including tail-vacating times.
[[nodiscard]] std::vector<std::size_t> hintCandidates(const Board& board, const std::vector<bool>& present);
[[nodiscard]] std::optional<std::size_t> chooseHint(const Board& board, const std::vector<bool>& present,
                                                 std::mt19937& random);

} // namespace arrow
