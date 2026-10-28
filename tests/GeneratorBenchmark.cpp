#include "arrow/Generator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace {

double peakWorkingSetMiB() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<double>(counters.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return -1.0; // Unavailable; never report zero as a memory measurement.
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 5) {
            throw std::invalid_argument("Usage: arrow_generator_benchmark seed [size=160] [loops=1] [candidates=8]");
        }
        const auto seed = std::stoull(argv[1]);
        if (seed > std::numeric_limits<std::uint32_t>::max()) {
            throw std::invalid_argument("Seed is outside uint32 range");
        }
        const int size = argc > 2 ? std::stoi(argv[2]) : 160;
        if (size < 4 || size > 512 || size % 2 != 0) {
            throw std::invalid_argument("Benchmark size must be even and in 4..512");
        }
        const bool loops = argc <= 3 || std::stoi(argv[3]) != 0;
        const int candidates = argc > 4 ? std::stoi(argv[4]) : 8;
        if (candidates < 1 || candidates > 64) {
            throw std::invalid_argument("Benchmark candidates must be in 1..64");
        }
        const arrow::GeneratorOptions options{
            size, size, 2, size >= 160 ? 960 : size * 3 / 2,
            static_cast<std::uint32_t>(seed), candidates, loops
        };
        const auto started = std::chrono::steady_clock::now();
        const auto board = arrow::generateBoard(options);
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started
        ).count();
        const double peakMiB = peakWorkingSetMiB();

        std::vector<int> owners(static_cast<std::size_t>(size * size), -1);
        std::vector<int> bodyPositions(owners.size(), -1);
        std::uint64_t hash = 14695981039346656037ULL;
        const auto hashInteger = [&](int value) {
            hash ^= static_cast<std::uint32_t>(value);
            hash *= 1099511628211ULL;
        };
        std::size_t shortArrows = 0, mediumArrows = 0, longArrows = 0;
        int shortBorder = 0, interiorLinkedBorder = 0;
        for (std::size_t i = 0; i < board.arrows().size(); ++i) {
            const auto& value = board.arrows()[i];
            hashInteger(static_cast<int>(value.cells.size()));
            hashInteger(value.direction.x);
            hashInteger(value.direction.y);
            int border = 0;
            for (std::size_t k = 0; k < value.cells.size(); ++k) {
                const auto cell = value.cells[k];
                owners[cell.y * size + cell.x] = static_cast<int>(i);
                bodyPositions[cell.y * size + cell.x] = static_cast<int>(k);
                border += cell.x == 0 || cell.y == 0 || cell.x == size - 1 || cell.y == size - 1 ? 1 : 0;
                hashInteger(cell.x);
                hashInteger(cell.y);
            }
            shortArrows += value.cells.size() <= 5 ? 1U : 0U;
            mediumArrows += value.cells.size() > 5 && value.cells.size() <= 12 ? 1U : 0U;
            longArrows += value.cells.size() > 12 ? 1U : 0U;
            shortBorder += value.cells.size() <= 5 ? border : 0;
            interiorLinkedBorder += border < static_cast<int>(value.cells.size()) ? border : 0;
        }

        int initial = 0, selfPasses = 0;
        bool intendedOrder = true, selfCollision = false;
        for (std::size_t i = 0; i < board.arrows().size(); ++i) {
            const auto& value = board.arrows()[i];
            auto probe = value.cells.front() + value.direction;
            int distance = 1;
            bool blocked = false, selfPass = false;
            while (board.isInside(probe)) {
                const int position = probe.y * size + probe.x;
                const int owner = owners[position];
                if (owner == static_cast<int>(i)) {
                    selfPass = true;
                    selfCollision |= distance < static_cast<int>(value.cells.size()) - bodyPositions[position];
                } else {
                    blocked = true;
                    intendedOrder &= owner < static_cast<int>(i);
                }
                probe = probe + value.direction;
                ++distance;
            }
            initial += blocked ? 0 : 1;
            selfPasses += selfPass ? 1 : 0;
        }
        // Timing and peak memory above exclude the deliberately independent,
        // slower Board simulation and these reporting allocations.
        const bool solved = board.isCompletelyFilled() && board.hasValidGeometry() &&
            board.solveGreedily().size() == board.arrows().size() && intendedOrder &&
            !selfCollision && (loops || selfPasses == 0);
        std::cout << "seed,size,loops,candidates,seconds,peak_MiB,arrows,initial,short,medium,long,short_border_cells,interior_linked_border_cells,self_passes,geometry_hash,verified\n"
            << seed << ',' << size << ',' << loops << ',' << candidates << ','
            << std::fixed << std::setprecision(6) << seconds << ',' << peakMiB << ','
            << board.arrows().size() << ',' << initial << ',' << shortArrows << ','
            << mediumArrows << ',' << longArrows << ',' << shortBorder << ','
            << interiorLinkedBorder << ',' << selfPasses << ',' << hash << ',' << solved << '\n';
        return solved ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
