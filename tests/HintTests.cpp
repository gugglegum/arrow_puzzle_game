#include "arrow/Hints.hpp"
#include "arrow/Generator.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void checkCandidates(const arrow::Board& board, const std::vector<bool>& present) {
    std::vector<std::size_t> expected;
    for (std::size_t index = 0; index < present.size(); ++index) {
        if (board.canExit(index, present)) expected.push_back(index);
    }
    require(arrow::hintCandidates(board, present) == expected, "Hint disagrees with complete step-by-step exit simulation");
}
}

int main() {
    try {
        arrow::HintState hints;
        require(!hints.ready(), "New board offered a hint immediately");
        hints.advance(19.99);
        require(!hints.ready(), "Hint unlocked before 20 seconds");
        hints.advance(0.01);
        require(hints.ready(), "Hint did not unlock at 20 seconds");
        hints.highlighted = 2;
        hints.advance(3600);
        require(hints.ready() && hints.highlighted == 2, "Hint expired without progress");
        hints.reset();
        hints.advance(19.9);
        require(!hints.ready() && !hints.highlighted, "Progress did not reset the timer and highlight");

        // The head enters the original tail cell on step one: legal because
        // that cell is vacated on the same tick. A longer curl self-collides.
        const arrow::Board loops(5, 5, {
            {{{1, 1}, {0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 1}}, {1, 0}},
            {{{3, 3}, {2, 3}, {2, 4}, {3, 4}, {4, 4}, {4, 3}, {4, 2}}, {1, 0}}
        });
        checkCandidates(loops, {true, true});
        require(arrow::hintCandidates(loops, {true, true}) == std::vector<std::size_t>{0}, "Tail-pass/self-collision mismatch");
        require(arrow::hintCandidates(loops, {}).empty(), "Invalid presence mask accepted");
        std::mt19937 random(20261031);
        for (bool allowLoops : {false, true}) {
            for (unsigned seed = 0; seed < 12; ++seed) {
                const auto board = arrow::generateBoard({20, 20, 2, 30, seed, 8, allowLoops});
                std::vector<bool> present(board.arrows().size(), true);
                while (true) {
                    checkCandidates(board, present);
                    const auto next = arrow::chooseHint(board, present, random);
                    if (!next) break;
                    require(board.canExit(*next, present), "Selected hint cannot exit");
                    present[*next] = false;
                }
                require(std::ranges::none_of(present, [](bool value) { return value; }), "Hints could not finish a generated puzzle");
            }
        }
        const arrow::Board choices(5, 5, {{{{0, 0}}, {-1, 0}}, {{{4, 4}}, {1, 0}}, {{{2, 0}}, {0, -1}}});
        int counts[3]{};
        for (int i = 0; i < 6000; ++i) ++counts[*arrow::chooseHint(choices, {true, true, true}, random)];
        for (int count : counts) require(count > 1700 && count < 2300, "Random hints do not cover all legal choices evenly");
        require(!arrow::chooseHint(choices, {false, false, false}, random), "Empty board offered a hint");
        std::vector<arrow::Arrow> denseArrows;
        for (int y = 0; y < 160; ++y) {
            for (int x = 0; x < 160; x += 2) denseArrows.push_back({{{x + 1, y}, {x, y}}, {1, 0}});
        }
        const arrow::Board dense(160, 160, std::move(denseArrows));
        const auto denseHints = arrow::hintCandidates(dense, std::vector<bool>(12800, true));
        require(denseHints.size() == 160, "Dense large board has wrong hint count");
        for (std::size_t index : denseHints) require(index % 80 == 79, "Dense board hint points into a stationary arrow");
        std::cout << "Hint tests passed: exact timer, reset, unlimited availability, random choice, self-hits, tail passes, 24 complete generated puzzles.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Hint test failure: " << error.what() << '\n';
        return 1;
    }
}
