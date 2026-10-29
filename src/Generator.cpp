#include "arrow/Generator.hpp"
#include "FrontierGenerator.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arrow {
namespace {

class DisjointSet {
public:
    explicit DisjointSet(int size)
        : parent_(static_cast<std::size_t>(size)),
          rank_(static_cast<std::size_t>(size), 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int value) {
        int& parent = parent_[static_cast<std::size_t>(value)];
        if (parent != value) {
            parent = find(parent);
        }
        return parent;
    }

    bool unite(int lhs, int rhs) {
        lhs = find(lhs);
        rhs = find(rhs);
        if (lhs == rhs) {
            return false;
        }

        if (rank_[static_cast<std::size_t>(lhs)] <
            rank_[static_cast<std::size_t>(rhs)]) {
            std::swap(lhs, rhs);
        }
        parent_[static_cast<std::size_t>(rhs)] = lhs;
        if (rank_[static_cast<std::size_t>(lhs)] ==
            rank_[static_cast<std::size_t>(rhs)]) {
            ++rank_[static_cast<std::size_t>(lhs)];
        }
        return true;
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

struct Opening {
    int cycleIndex{};
    bool forward{};
};

struct RayInfo {
    int requiredEnd{};
    bool hasEarlierBlocker{};
    std::vector<std::pair<int, int>> cells; // (path position, distance)
};

struct Interval {
    int start{};
    int end{};
};

struct LengthProfile {
    std::vector<int> targets;
    std::vector<int> minimums;
    std::vector<int> maximums;
};

enum IntervalFlag : std::uint8_t {
    IntervalValid = 1U << 0U,
    IntervalTightSelfPass = 1U << 1U
};

struct IntervalFacts {
    int lengthStride{};
    std::vector<std::uint8_t> flags;

    [[nodiscard]] std::uint8_t at(int start, int length) const {
        return flags[static_cast<std::size_t>(
            start * lengthStride + length
        )];
    }
};

struct TangleMetrics {
    bool intendedOrderValid{};
    int interArrowEdges{};
    int horizontalInterArrowEdges{};
    int verticalInterArrowEdges{};
    int scanlineExcessRuns{};
    int boundingBoxEmptyCells{};
    int rectangularArrows{};
    int headAtBoundingCorner{};
    int turns{};
    int directionCount{};
    int initiallyRemovable{};
    int maximumChoices{};
    int choiceExcess{};
    int richMacroBlocks{};
    int shortArrows{};
    int mediumArrows{};
    int longArrows{};
    int veryLongArrows{};
    int tightSelfPassArrows{};
    int rewardedTightSelfPassArrows{};
    int longComplexTightSelfPassArrows{};
    int tightSelfPassComplexity{};
    int shortBorderCells{};
    int interiorLinkedBorderCells{};
    double medianBoundingFill{};
};

struct BoardCandidate {
    std::vector<Arrow> arrows;
    TangleMetrics metrics;
    bool meetsRequiredQuality{};
    int score{std::numeric_limits<int>::min()};
};

int cellIndex(Cell cell, int columns) {
    return cell.y * columns + cell.x;
}

Cell cellFromIndex(int index, int columns) {
    return {index % columns, index / columns};
}

bool isInside(Cell cell, int columns, int rows) {
    return cell.x >= 0 && cell.x < columns &&
           cell.y >= 0 && cell.y < rows;
}

bool pointsOutside(Cell head, Cell neck, int columns, int rows) {
    const Cell direction = head - neck;
    return isCardinal(direction) &&
           !isInside(head + direction, columns, rows);
}

std::pair<int, int> initialChoiceRange(int columns, int rows) {
    const int maximumDimension = std::max(columns, rows);
    if (maximumDimension <= 20) {
        return {2, 4};
    }
    if (maximumDimension <= 40) {
        return {2, 5};
    }
    if (maximumDimension <= 80) {
        return {3, 7};
    }
    return {
        std::clamp(maximumDimension / 40 + 2, 4, 8),
        std::clamp(maximumDimension / 16, 6, 12)
    };
}

void addEdge(std::vector<std::vector<int>>& adjacency, int lhs, int rhs) {
    adjacency[static_cast<std::size_t>(lhs)].push_back(rhs);
    adjacency[static_cast<std::size_t>(rhs)].push_back(lhs);
}

void removeEdge(
    std::vector<std::vector<int>>& adjacency,
    int lhs,
    int rhs
) {
    auto& lhsEdges = adjacency[static_cast<std::size_t>(lhs)];
    auto& rhsEdges = adjacency[static_cast<std::size_t>(rhs)];
    lhsEdges.erase(std::ranges::find(lhsEdges, rhs));
    rhsEdges.erase(std::ranges::find(rhsEdges, lhs));
}

std::vector<Cell> makeLabyrinthCycle(
    int columns,
    int rows,
    std::mt19937& random
) {
    const int macroColumns = columns / 2;
    const int macroRows = rows / 2;
    const int macroArea = macroColumns * macroRows;
    const int area = columns * rows;

    std::vector<std::pair<int, int>> macroEdges;
    for (int y = 0; y < macroRows; ++y) {
        for (int x = 0; x < macroColumns; ++x) {
            const int current = y * macroColumns + x;
            if (x + 1 < macroColumns) {
                macroEdges.emplace_back(current, current + 1);
            }
            if (y + 1 < macroRows) {
                macroEdges.emplace_back(current, current + macroColumns);
            }
        }
    }
    std::ranges::shuffle(macroEdges, random);

    DisjointSet sets(macroArea);
    std::vector<std::pair<int, int>> treeEdges;
    treeEdges.reserve(static_cast<std::size_t>(macroArea - 1));
    for (const auto [lhs, rhs] : macroEdges) {
        if (sets.unite(lhs, rhs)) {
            treeEdges.emplace_back(lhs, rhs);
        }
    }

    std::vector<std::vector<int>> adjacency(
        static_cast<std::size_t>(area)
    );
    const auto indexOf = [columns](int x, int y) {
        return y * columns + x;
    };

    // Every macro cell starts as a separate 2 x 2 cycle.
    for (int macroY = 0; macroY < macroRows; ++macroY) {
        for (int macroX = 0; macroX < macroColumns; ++macroX) {
            const int left = macroX * 2;
            const int top = macroY * 2;
            const int topLeft = indexOf(left, top);
            const int topRight = indexOf(left + 1, top);
            const int bottomRight = indexOf(left + 1, top + 1);
            const int bottomLeft = indexOf(left, top + 1);
            addEdge(adjacency, topLeft, topRight);
            addEdge(adjacency, topRight, bottomRight);
            addEdge(adjacency, bottomRight, bottomLeft);
            addEdge(adjacency, bottomLeft, topLeft);
        }
    }

    // An edge of the random macro-tree splices two independent cycles into
    // one. Since the macro graph is a tree, the final result is one cycle
    // visiting every board cell exactly once.
    for (const auto [lhs, rhs] : treeEdges) {
        const int lhsX = lhs % macroColumns;
        const int lhsY = lhs / macroColumns;
        const int rhsX = rhs % macroColumns;
        const int rhsY = rhs / macroColumns;

        if (lhsY == rhsY) {
            const int leftX = std::min(lhsX, rhsX);
            const int y = lhsY;
            const int leftTopRight = indexOf(leftX * 2 + 1, y * 2);
            const int leftBottomRight =
                indexOf(leftX * 2 + 1, y * 2 + 1);
            const int rightTopLeft = indexOf(leftX * 2 + 2, y * 2);
            const int rightBottomLeft =
                indexOf(leftX * 2 + 2, y * 2 + 1);

            removeEdge(adjacency, leftTopRight, leftBottomRight);
            removeEdge(adjacency, rightTopLeft, rightBottomLeft);
            addEdge(adjacency, leftTopRight, rightTopLeft);
            addEdge(adjacency, leftBottomRight, rightBottomLeft);
        } else {
            const int x = lhsX;
            const int topY = std::min(lhsY, rhsY);
            const int topBottomLeft = indexOf(x * 2, topY * 2 + 1);
            const int topBottomRight =
                indexOf(x * 2 + 1, topY * 2 + 1);
            const int bottomTopLeft = indexOf(x * 2, topY * 2 + 2);
            const int bottomTopRight =
                indexOf(x * 2 + 1, topY * 2 + 2);

            removeEdge(adjacency, topBottomLeft, topBottomRight);
            removeEdge(adjacency, bottomTopLeft, bottomTopRight);
            addEdge(adjacency, topBottomLeft, bottomTopLeft);
            addEdge(adjacency, topBottomRight, bottomTopRight);
        }
    }

    for (const auto& edges : adjacency) {
        if (edges.size() != 2) {
            throw std::logic_error("Hamiltonian cycle has invalid degree");
        }
    }

    std::vector<Cell> cycle;
    cycle.reserve(static_cast<std::size_t>(area));
    int previous = -1;
    int current = 0;
    for (int step = 0; step < area; ++step) {
        cycle.push_back(cellFromIndex(current, columns));
        const auto& edges = adjacency[static_cast<std::size_t>(current)];
        const int next = edges[0] == previous ? edges[1] : edges[0];
        previous = current;
        current = next;
    }
    if (current != 0) {
        throw std::logic_error("Hamiltonian traversal did not close");
    }
    return cycle;
}

std::vector<Opening> findOpenings(
    const std::vector<Cell>& cycle,
    int columns,
    int rows
) {
    std::vector<Opening> result;
    const int size = static_cast<int>(cycle.size());
    for (int index = 0; index < size; ++index) {
        const int next = (index + 1) % size;
        const int previous = (index + size - 1) % size;
        if (pointsOutside(cycle[index], cycle[next], columns, rows)) {
            result.push_back({index, true});
        }
        if (pointsOutside(cycle[index], cycle[previous], columns, rows)) {
            result.push_back({index, false});
        }
    }
    return result;
}

std::vector<Cell> openCycle(
    const std::vector<Cell>& cycle,
    Opening opening
) {
    std::vector<Cell> result;
    result.reserve(cycle.size());
    const int size = static_cast<int>(cycle.size());
    for (int offset = 0; offset < size; ++offset) {
        const int index = opening.forward
            ? (opening.cycleIndex + offset) % size
            : (opening.cycleIndex - offset + size) % size;
        result.push_back(cycle[static_cast<std::size_t>(index)]);
    }
    return result;
}

std::vector<RayInfo> analyzeRays(
    const std::vector<Cell>& path,
    int columns,
    int rows
) {
    const int area = static_cast<int>(path.size());
    std::vector<int> positions(static_cast<std::size_t>(area), -1);
    for (int index = 0; index < area; ++index) {
        positions[static_cast<std::size_t>(
            cellIndex(path[static_cast<std::size_t>(index)], columns)
        )] = index;
    }

    std::vector<RayInfo> result(
        static_cast<std::size_t>(area),
        {-1, false, {}}
    );
    for (int start = 0; start + 1 < area; ++start) {
        const Cell direction =
            path[static_cast<std::size_t>(start)] -
            path[static_cast<std::size_t>(start + 1)];
        Cell probe = path[static_cast<std::size_t>(start)] + direction;
        RayInfo info{start - 1, false, {}};
        int distance = 1;
        while (isInside(probe, columns, rows)) {
            const int position = positions[static_cast<std::size_t>(
                cellIndex(probe, columns)
            )];
            info.cells.emplace_back(position, distance);
            if (position < start) {
                info.hasEarlierBlocker = true;
            } else {
                info.requiredEnd = std::max(info.requiredEnd, position);
            }
            probe = probe + direction;
            ++distance;
        }
        result[static_cast<std::size_t>(start)] = info;
    }
    return result;
}

IntervalFacts buildIntervalFacts(
    const std::vector<RayInfo>& rays,
    int minimumLength,
    int maximumLength,
    bool allowArrowLoops
) {
    const int area = static_cast<int>(rays.size());
    IntervalFacts result{
        maximumLength + 1,
        std::vector<std::uint8_t>(
            static_cast<std::size_t>(
                area * (maximumLength + 1)
            ),
            0U
        )
    };
    std::vector<std::pair<int, int>> bodyHits;

    for (int start = 0; start < area; ++start) {
        const RayInfo& ray = rays[static_cast<std::size_t>(start)];
        bodyHits.clear();
        for (const auto [position, distance] : ray.cells) {
            if (position >= start) {
                bodyHits.emplace_back(position, distance);
            }
        }
        std::ranges::sort(bodyHits);

        std::size_t nextHit = 0;
        int earliestDeadline = std::numeric_limits<int>::max();
        const int finalEnd = std::min(
            area - 1,
            start + maximumLength - 1
        );
        for (int end = start; end <= finalEnd; ++end) {
            while (nextHit < bodyHits.size() &&
                   bodyHits[nextHit].first <= end) {
                const auto [position, distance] =
                    bodyHits[nextHit++];
                earliestDeadline = std::min(
                    earliestDeadline,
                    position + distance - 1
                );
            }

            const int length = end - start + 1;
            if (length < minimumLength) {
                continue;
            }
            const bool crossesOwnBody = nextHit != 0;
            const bool clearsOwnBody =
                earliestDeadline >= end;
            const bool valid =
                ray.requiredEnd <= end &&
                clearsOwnBody &&
                (allowArrowLoops || !crossesOwnBody);
            std::uint8_t flags = valid ? IntervalValid : 0U;
            if (valid && crossesOwnBody &&
                earliestDeadline == end) {
                flags |= IntervalTightSelfPass;
            }
            result.flags[static_cast<std::size_t>(
                start * result.lengthStride + length
            )] = flags;
        }
    }
    return result;
}

std::vector<std::uint64_t> reachableArrowCounts(
    const IntervalFacts& intervalFacts,
    int minimumLength,
    int maximumLength,
    int maximumCount
) {
    const int area = static_cast<int>(
        intervalFacts.flags.size() /
        static_cast<std::size_t>(intervalFacts.lengthStride)
    );
    const std::size_t wordCount =
        (static_cast<std::size_t>(maximumCount) + 64U) / 64U;
    std::vector<std::uint64_t> reachable(
        static_cast<std::size_t>(area + 1) * wordCount,
        0U
    );
    reachable[0] = 1U;

    for (int start = 0; start < area; ++start) {
        const std::size_t sourceOffset =
            static_cast<std::size_t>(start) * wordCount;
        bool hasReachableCount = false;
        for (std::size_t word = 0; word < wordCount; ++word) {
            hasReachableCount =
                hasReachableCount ||
                reachable[sourceOffset + word] != 0U;
        }
        if (!hasReachableCount) {
            continue;
        }

        const int minimumEnd = start + minimumLength - 1;
        const int maximumEnd = std::min(
            area - 1,
            start + maximumLength - 1
        );
        for (int end = minimumEnd; end <= maximumEnd; ++end) {
            const int length = end - start + 1;
            if ((intervalFacts.at(start, length) &
                 IntervalValid) == 0U) {
                continue;
            }

            const std::size_t destinationOffset =
                static_cast<std::size_t>(end + 1) * wordCount;
            std::uint64_t carry = 0U;
            for (std::size_t word = 0; word < wordCount; ++word) {
                const std::uint64_t source =
                    reachable[sourceOffset + word];
                reachable[destinationOffset + word] |=
                    (source << 1U) | carry;
                carry = source >> 63U;
            }
        }
    }

    const auto finalBegin = reachable.begin() +
        static_cast<std::ptrdiff_t>(
            static_cast<std::size_t>(area) * wordCount
        );
    return {finalBegin, finalBegin + static_cast<std::ptrdiff_t>(wordCount)};
}

std::vector<std::uint64_t> reachableArrowCountsByRanges(
    const IntervalFacts& intervalFacts,
    int minimumLength,
    int maximumLength,
    int maximumCount
) {
    const int area = static_cast<int>(
        intervalFacts.flags.size() /
        static_cast<std::size_t>(intervalFacts.lengthStride)
    );
    std::vector<int> firstEnds(static_cast<std::size_t>(area), -1);
    std::vector<int> lastEnds(static_cast<std::size_t>(area), -1);
    for (int start = 0; start < area; ++start) {
        const int finalEnd = std::min(
            area - 1,
            start + maximumLength - 1
        );
        for (int end = start + minimumLength - 1;
             end <= finalEnd;
             ++end) {
            const int length = end - start + 1;
            if ((intervalFacts.at(start, length) &
                 IntervalValid) == 0U) {
                continue;
            }
            if (firstEnds[static_cast<std::size_t>(start)] < 0) {
                firstEnds[static_cast<std::size_t>(start)] = end;
            }
            lastEnds[static_cast<std::size_t>(start)] = end;
        }
    }

    const std::size_t countWordCount =
        (static_cast<std::size_t>(maximumCount) + 64U) / 64U;
    std::vector<std::uint64_t> result(countWordCount, 0U);
    std::vector<std::uint8_t> current(
        static_cast<std::size_t>(area + 1),
        0U
    );
    std::vector<std::uint8_t> next(
        static_cast<std::size_t>(area + 1),
        0U
    );
    std::vector<int> differences(
        static_cast<std::size_t>(area + 2),
        0
    );
    current[0] = 1U;

    for (int count = 1; count <= maximumCount; ++count) {
        for (int start = 0; start < area; ++start) {
            if (current[static_cast<std::size_t>(start)] == 0U) {
                continue;
            }
            const int first = firstEnds[static_cast<std::size_t>(start)];
            if (first < 0) {
                continue;
            }
            ++differences[static_cast<std::size_t>(first + 1)];
            --differences[static_cast<std::size_t>(
                lastEnds[static_cast<std::size_t>(start)] + 2
            )];
        }

        int active = 0;
        bool hasNext = false;
        for (int position = 0; position <= area; ++position) {
            const std::size_t index =
                static_cast<std::size_t>(position);
            active += differences[index];
            differences[index] = 0;
            next[index] = active > 0 ? 1U : 0U;
            hasNext = hasNext || next[index] != 0U;
        }
        differences[static_cast<std::size_t>(area + 1)] = 0;
        if (next[static_cast<std::size_t>(area)] != 0U) {
            const std::size_t index = static_cast<std::size_t>(count);
            result[index / 64U] |=
                std::uint64_t{1} << (index % 64U);
        }
        if (!hasNext) {
            break;
        }
        current.swap(next);
    }
    return result;
}

bool containsArrowCount(
    const std::vector<std::uint64_t>& reachableCounts,
    int count
) {
    const std::size_t index = static_cast<std::size_t>(count);
    const std::size_t word = index / 64U;
    return word < reachableCounts.size() &&
           (reachableCounts[word] &
           (std::uint64_t{1} << (index % 64U))) != 0U;
}

int highestReachableArrowCount(
    const std::vector<std::uint64_t>& reachableCounts,
    int minimumCount,
    int maximumCount
) {
    for (int count = maximumCount;
         count >= minimumCount;
         --count) {
        if (containsArrowCount(reachableCounts, count)) {
            return count;
        }
    }
    return 0;
}

bool usesMixedLengthProfile(
    int minimumLength,
    int maximumLength,
    int arrowCount
) {
    return minimumLength <= 2 &&
           maximumLength >= 20 &&
           arrowCount >= 20;
}

LengthProfile makeLengthProfile(
    int area,
    int arrowCount,
    int minimumLength,
    int maximumLength,
    std::mt19937& random
) {
    const int average = std::clamp(
        static_cast<int>(std::lround(
            static_cast<double>(area) /
            static_cast<double>(arrowCount)
        )),
        minimumLength,
        maximumLength
    );
    if (!usesMixedLengthProfile(
            minimumLength,
            maximumLength,
            arrowCount
        )) {
        return {
            std::vector<int>(
                static_cast<std::size_t>(arrowCount),
                average
            ),
            std::vector<int>(
                static_cast<std::size_t>(arrowCount),
                minimumLength
            ),
            std::vector<int>(
                static_cast<std::size_t>(arrowCount),
                maximumLength
            )
        };
    }

    const int shortCount = std::max(1, arrowCount * 40 / 100);
    const int longCount = std::max(1, arrowCount * 14 / 100);
    const int mediumCount = arrowCount - shortCount - longCount;

    std::vector<int> targets;
    std::vector<int> lowerBounds;
    std::vector<int> upperBounds;
    targets.reserve(static_cast<std::size_t>(arrowCount));
    lowerBounds.reserve(static_cast<std::size_t>(arrowCount));
    upperBounds.reserve(static_cast<std::size_t>(arrowCount));

    const auto appendBand = [&](
        int count,
        int lower,
        int upper
    ) {
        lower = std::clamp(lower, minimumLength, maximumLength);
        upper = std::clamp(upper, lower, maximumLength);
        std::uniform_int_distribution<int> distribution(lower, upper);
        for (int index = 0; index < count; ++index) {
            targets.push_back(distribution(random));
            lowerBounds.push_back(lower);
            upperBounds.push_back(upper);
        }
    };
    appendBand(shortCount, 2, 5);
    appendBand(mediumCount, 6, 10);
    appendBand(longCount, 18, std::min(28, maximumLength));

    std::vector<std::size_t> order(targets.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    int difference = area - std::accumulate(
        targets.begin(),
        targets.end(),
        0
    );
    while (difference != 0) {
        std::ranges::shuffle(order, random);
        bool changed = false;
        for (std::size_t index : order) {
            if (difference > 0 && targets[index] < upperBounds[index]) {
                ++targets[index];
                --difference;
                changed = true;
            } else if (
                difference < 0 &&
                targets[index] > lowerBounds[index]
            ) {
                --targets[index];
                ++difference;
                changed = true;
            }
            if (difference == 0) {
                break;
            }
        }
        if (!changed) {
            // The preferred bands normally have ample capacity around the
            // default average of eight cells. Keep the generic generator
            // total-safe if a custom board requests an extreme arrow count.
            for (std::size_t index : order) {
                if (difference > 0 && targets[index] < maximumLength) {
                    ++targets[index];
                    --difference;
                    changed = true;
                } else if (
                    difference < 0 &&
                    targets[index] > minimumLength
                ) {
                    --targets[index];
                    ++difference;
                    changed = true;
                }
                if (difference == 0) {
                    break;
                }
            }
        }
        if (!changed) {
            break;
        }
    }
    for (std::size_t index = 0; index < targets.size(); ++index) {
        lowerBounds[index] = std::min(
            lowerBounds[index],
            targets[index]
        );
        upperBounds[index] = std::max(
            upperBounds[index],
            targets[index]
        );
    }

    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::shuffle(order, random);
    LengthProfile result;
    result.targets.reserve(targets.size());
    result.minimums.reserve(targets.size());
    result.maximums.reserve(targets.size());
    for (std::size_t index : order) {
        result.targets.push_back(targets[index]);
        result.minimums.push_back(lowerBounds[index]);
        result.maximums.push_back(upperBounds[index]);
    }
    return result;
}

bool findIntervalsForCount(
    const std::vector<Cell>& path,
    const std::vector<RayInfo>& rays,
    const IntervalFacts& intervalFacts,
    int columns,
    int rows,
    int minimumLength,
    int maximumLength,
    int arrowCount,
    int initialChoiceTarget,
    std::mt19937& random,
    std::vector<Interval>& result
) {
    const int area = static_cast<int>(path.size());
    const LengthProfile lengthProfile = makeLengthProfile(
        area,
        arrowCount,
        minimumLength,
        maximumLength,
        random
    );
    const bool mixedLengths = usesMixedLengthProfile(
        minimumLength,
        maximumLength,
        arrowCount
    );
    const std::vector<int>& targetLengths =
        lengthProfile.targets;
    std::vector<int> targetMinimums =
        lengthProfile.minimums;
    std::vector<int> targetMaximums =
        lengthProfile.maximums;
    std::vector<int> positions(static_cast<std::size_t>(area), -1);
    for (int index = 0; index < area; ++index) {
        positions[static_cast<std::size_t>(
            cellIndex(path[static_cast<std::size_t>(index)], columns)
        )] = index;
    }
    std::vector<std::uint8_t> preferredInitialFront(
        static_cast<std::size_t>(area),
        0U
    );
    preferredInitialFront[0] = 1U;
    const int lengthStride = maximumLength + 1;
    std::vector<int> intervalScores(
        static_cast<std::size_t>(area * lengthStride),
        0
    );
    constexpr int tightSelfPassBonus = 2'500;
    constexpr int tightSelfPassLengthBonus = 10;
    constexpr int tightSelfPassTurnBonus = 50;
    constexpr int longComplexTightSelfPassBonus = 4'000;
    constexpr int preferredInitialFrontBonus = 8'000;
    constexpr int largePreferredInitialFrontBonus = 1'000'000;
    constexpr int unpreferredInitialFrontPenalty = 1'000'000;
    std::uniform_int_distribution<int> tieBreaker(0, 7);
    for (int start = 0; start < area; ++start) {
        int minimumX = path[static_cast<std::size_t>(start)].x;
        int maximumX = minimumX;
        int minimumY = path[static_cast<std::size_t>(start)].y;
        int maximumY = minimumY;
        int turns = 0;
        int adjacentEdges = 0;

        const int finalEnd = std::min(
            area - 1,
            start + maximumLength - 1
        );
        for (int end = start; end <= finalEnd; ++end) {
            const Cell cell = path[static_cast<std::size_t>(end)];
            minimumX = std::min(minimumX, cell.x);
            maximumX = std::max(maximumX, cell.x);
            minimumY = std::min(minimumY, cell.y);
            maximumY = std::max(maximumY, cell.y);

            if (end >= start + 2) {
                const Cell previous =
                    path[static_cast<std::size_t>(end - 1)] -
                    path[static_cast<std::size_t>(end - 2)];
                const Cell current =
                    path[static_cast<std::size_t>(end)] -
                    path[static_cast<std::size_t>(end - 1)];
                turns += previous != current ? 1 : 0;
            }

            const std::array<Cell, 4> neighbors{{
                {cell.x - 1, cell.y},
                {cell.x + 1, cell.y},
                {cell.x, cell.y - 1},
                {cell.x, cell.y + 1}
            }};
            for (Cell neighbor : neighbors) {
                if (!isInside(neighbor, columns, rows)) {
                    continue;
                }
                const int neighborPosition =
                    positions[static_cast<std::size_t>(
                        cellIndex(neighbor, columns)
                    )];
                adjacentEdges +=
                    neighborPosition >= start && neighborPosition < end
                    ? 1
                    : 0;
            }

            const int length = end - start + 1;
            if (length < minimumLength) {
                continue;
            }
            const int boundingArea =
                (maximumX - minimumX + 1) *
                (maximumY - minimumY + 1);
            const int extraSelfContacts = std::max(
                0,
                adjacentEdges - (length - 1)
            );
            const std::uint8_t facts =
                intervalFacts.at(start, length);
            const bool tightSelfPass =
                (facts & IntervalTightSelfPass) != 0U;
            const bool rectangularThread =
                length > 12 && length * 10 > boundingArea * 9;
            const bool complexThread =
                turns >= 2 && boundingArea - length >= 3;
            int dynamicSelfPassBonus = 0;
            if (tightSelfPass && !rectangularThread) {
                dynamicSelfPassBonus =
                    tightSelfPassBonus +
                    std::min(length, 40) *
                        tightSelfPassLengthBonus +
                    std::min(turns, 12) *
                        tightSelfPassTurnBonus;
                if (length >= 18 && complexThread) {
                    dynamicSelfPassBonus +=
                        longComplexTightSelfPassBonus;
                }
            }
            intervalScores[static_cast<std::size_t>(
                start * lengthStride + length
            )] =
                (mixedLengths
                    ? (boundingArea - length) * 20
                    : boundingArea * 12) +
                turns * 5 -
                extraSelfContacts * 800 -
                tieBreaker(random) +
                dynamicSelfPassBonus;
        }
    }

    for (int choice = 1;
         choice < initialChoiceTarget;
         ++choice) {
        const int target = choice * area / initialChoiceTarget;
        int bestStart = -1;
        int bestDistance = area;
        for (int start = 1; start + 1 < area; ++start) {
            if (rays[static_cast<std::size_t>(start)]
                    .hasEarlierBlocker ||
                preferredInitialFront[
                    static_cast<std::size_t>(start)
                ] != 0U) {
                continue;
            }
            bool hasValidInterval = false;
            for (int length = minimumLength;
                 length <= maximumLength;
                 ++length) {
                if ((intervalFacts.at(start, length) &
                     IntervalValid) != 0U) {
                    hasValidInterval = true;
                    break;
                }
            }
            if (!hasValidInterval) {
                continue;
            }
            const int distance = std::abs(start - target);
            if (distance < bestDistance) {
                bestStart = start;
                bestDistance = distance;
            }
        }
        if (bestStart >= 0) {
            preferredInitialFront[
                static_cast<std::size_t>(bestStart)
            ] = 1U;
        }
    }
    for (int start = 0; start < area; ++start) {
        if (!rays[static_cast<std::size_t>(start)]
                 .hasEarlierBlocker) {
            const bool preferred =
                preferredInitialFront[
                    static_cast<std::size_t>(start)
                ] != 0U;
            const int base = start * lengthStride;
            for (int length = minimumLength;
                 length <= maximumLength;
                 ++length) {
                if ((intervalFacts.at(start, length) &
                     IntervalValid) != 0U) {
                    intervalScores[static_cast<std::size_t>(
                        base + length
                    )] += preferred
                        ? area >= 4096
                            ? largePreferredInitialFrontBonus
                            : preferredInitialFrontBonus
                        : area >= 4096
                            ? -unpreferredInitialFrontPenalty
                            : 0;
                }
            }
        }
    }

    const auto tryBounds = [&]() {
        constexpr int kImpossible =
            std::numeric_limits<int>::min() / 2;
        struct MemoState {
            int score{};
            std::uint32_t selectedLength{};
        };
        std::vector<int> suffixMinimums(
            static_cast<std::size_t>(arrowCount + 1),
            0
        );
        std::vector<int> suffixMaximums(
            static_cast<std::size_t>(arrowCount + 1),
            0
        );
        for (int ordinal = arrowCount - 1; ordinal >= 0; --ordinal) {
            suffixMinimums[static_cast<std::size_t>(ordinal)] =
                suffixMinimums[static_cast<std::size_t>(ordinal + 1)] +
                targetMinimums[static_cast<std::size_t>(ordinal)];
            suffixMaximums[static_cast<std::size_t>(ordinal)] =
                suffixMaximums[static_cast<std::size_t>(ordinal + 1)] +
                targetMaximums[static_cast<std::size_t>(ordinal)];
        }

        if (area >= 4096) {
            std::vector<int> firstEnds(
                static_cast<std::size_t>(area),
                -1
            );
            std::vector<int> lastEnds(
                static_cast<std::size_t>(area),
                -1
            );
            for (int start = 0; start < area; ++start) {
                if (area >= 16384 &&
                    !rays[static_cast<std::size_t>(start)]
                         .hasEarlierBlocker &&
                    preferredInitialFront[
                        static_cast<std::size_t>(start)
                    ] == 0U) {
                    continue;
                }
                const int finalEnd = std::min(
                    area - 1,
                    start + maximumLength - 1
                );
                for (int end = start + minimumLength - 1;
                     end <= finalEnd;
                     ++end) {
                    const int length = end - start + 1;
                    if ((intervalFacts.at(start, length) &
                         IntervalValid) == 0U) {
                        continue;
                    }
                    if (firstEnds[static_cast<std::size_t>(start)] < 0) {
                        firstEnds[static_cast<std::size_t>(start)] =
                            end;
                    }
                    lastEnds[static_cast<std::size_t>(start)] = end;
                }
            }

            const std::size_t positionWordCount =
                (static_cast<std::size_t>(area) + 64U) / 64U;
            std::vector<std::uint64_t> reachable(
                static_cast<std::size_t>(arrowCount + 1) *
                    positionWordCount,
                0U
            );
            reachable[0] = 1U;
            std::vector<int> differences(
                static_cast<std::size_t>(area + 2),
                0
            );
            for (int ordinal = 0;
                 ordinal < arrowCount;
                 ++ordinal) {
                const std::size_t currentOffset =
                    static_cast<std::size_t>(ordinal) *
                    positionWordCount;
                for (int start = 0; start < area; ++start) {
                    const std::size_t position =
                        static_cast<std::size_t>(start);
                    if ((reachable[
                            currentOffset + position / 64U
                        ] &
                         (std::uint64_t{1} <<
                          (position % 64U))) == 0U) {
                        continue;
                    }
                    const int first =
                        firstEnds[position];
                    if (first < 0) {
                        continue;
                    }
                    ++differences[static_cast<std::size_t>(first + 1)];
                    --differences[static_cast<std::size_t>(
                        lastEnds[position] + 2
                    )];
                }

                const std::size_t nextOffset =
                    static_cast<std::size_t>(ordinal + 1) *
                    positionWordCount;
                int active = 0;
                bool hasNext = false;
                for (int position = 0; position <= area; ++position) {
                    active += differences[
                        static_cast<std::size_t>(position)
                    ];
                    differences[
                        static_cast<std::size_t>(position)
                    ] = 0;
                    if (active <= 0) {
                        continue;
                    }
                    const std::size_t index =
                        static_cast<std::size_t>(position);
                    reachable[nextOffset + index / 64U] |=
                        std::uint64_t{1} << (index % 64U);
                    hasNext = true;
                }
                differences[static_cast<std::size_t>(area + 1)] = 0;
                if (!hasNext) {
                    return false;
                }
            }

            const std::size_t finalOffset =
                static_cast<std::size_t>(arrowCount) *
                positionWordCount;
            const std::size_t finalPosition =
                static_cast<std::size_t>(area);
            if ((reachable[
                    finalOffset + finalPosition / 64U
                ] &
                 (std::uint64_t{1} <<
                  (finalPosition % 64U))) == 0U) {
                return false;
            }

            result.resize(static_cast<std::size_t>(arrowCount));
            int position = area;
            for (int ordinal = arrowCount - 1;
                 ordinal >= 0;
                 --ordinal) {
                const std::size_t previousOffset =
                    static_cast<std::size_t>(ordinal) *
                    positionWordCount;
                const int earliestStart = std::max(
                    0,
                    position - maximumLength
                );
                const int latestStart =
                    position - minimumLength;
                int bestStart = -1;
                int bestScore = kImpossible;
                for (int start = earliestStart;
                     start <= latestStart;
                     ++start) {
                    const std::size_t startIndex =
                        static_cast<std::size_t>(start);
                    if ((reachable[
                            previousOffset + startIndex / 64U
                        ] &
                         (std::uint64_t{1} <<
                          (startIndex % 64U))) == 0U ||
                        firstEnds[startIndex] < 0 ||
                        position - 1 < firstEnds[startIndex] ||
                        position - 1 > lastEnds[startIndex]) {
                        continue;
                    }
                    const int length = position - start;
                    if ((intervalFacts.at(start, length) &
                         IntervalValid) == 0U) {
                        continue;
                    }
                    const int lengthPenalty =
                        std::abs(
                            length -
                            targetLengths[
                                static_cast<std::size_t>(ordinal)
                            ]
                        ) * (mixedLengths ? 70 : 12);
                    const int score =
                        intervalScores[static_cast<std::size_t>(
                            start * lengthStride + length
                        )] -
                        lengthPenalty;
                    if (score > bestScore) {
                        bestScore = score;
                        bestStart = start;
                    }
                }
                if (bestStart < 0) {
                    return false;
                }
                result[static_cast<std::size_t>(ordinal)] = {
                    bestStart,
                    position - 1
                };
                position = bestStart;
            }
            return position == 0;
        }

        std::unordered_map<std::uint64_t, MemoState> memo;
        memo.reserve(std::min<std::size_t>(
            static_cast<std::size_t>(area) * 8U,
            1'000'000U
        ));
        const auto stateKey = [area](int start, int ordinal) {
            return
                static_cast<std::uint64_t>(ordinal) *
                    static_cast<std::uint64_t>(area + 1) +
                static_cast<std::uint64_t>(start);
        };

        const auto search =
            [&](auto&& self, int start, int ordinal) -> int {
            if (ordinal == arrowCount) {
                return start == area ? 0 : kImpossible;
            }
            const int cellsLeft = area - start;
            if (cellsLeft <
                    suffixMinimums[
                        static_cast<std::size_t>(ordinal)
                    ] ||
                cellsLeft >
                    suffixMaximums[
                        static_cast<std::size_t>(ordinal)
                    ] ||
                start + 1 >= area) {
                return kImpossible;
            }
            const std::uint64_t key = stateKey(start, ordinal);
            if (const auto found = memo.find(key);
                found != memo.end()) {
                return found->second.score;
            }

            int bestScore = kImpossible;
            int bestEnd = -1;
            const int minimumEnd =
                start +
                targetMinimums[static_cast<std::size_t>(ordinal)] -
                1;
            const int maximumEnd = std::min(
                area - 1,
                start +
                    targetMaximums[
                        static_cast<std::size_t>(ordinal)
                    ] -
                    1
            );
            for (int end = minimumEnd; end <= maximumEnd; ++end) {
                const int length = end - start + 1;
                if ((intervalFacts.at(start, length) &
                     IntervalValid) == 0U ||
                    (!rays[static_cast<std::size_t>(start)]
                          .hasEarlierBlocker &&
                     preferredInitialFront[
                         static_cast<std::size_t>(start)
                     ] == 0U)) {
                    continue;
                }
                const int after = area - end - 1;
                if (after <
                        suffixMinimums[
                            static_cast<std::size_t>(ordinal + 1)
                        ] ||
                    after >
                        suffixMaximums[
                            static_cast<std::size_t>(ordinal + 1)
                        ]) {
                    continue;
                }
                const int suffix =
                    self(self, end + 1, ordinal + 1);
                if (suffix == kImpossible) {
                    continue;
                }
                const int lengthPenalty =
                    std::abs(
                        length -
                        targetLengths[
                            static_cast<std::size_t>(ordinal)
                        ]
                    ) * (mixedLengths ? 70 : 12);
                const int score =
                    intervalScores[static_cast<std::size_t>(
                        start * lengthStride + length
                    )] +
                    suffix -
                    lengthPenalty;
                if (score > bestScore) {
                    bestScore = score;
                    bestEnd = end;
                }
            }

            memo.emplace(
                key,
                MemoState{
                    bestScore,
                    bestEnd >= start
                        ? static_cast<std::uint32_t>(
                              bestEnd - start + 1
                          )
                        : 0U
                }
            );
            return bestScore;
        };

        if (search(search, 0, 0) == kImpossible) {
            return false;
        }

        result.clear();
        int start = 0;
        for (int ordinal = 0; ordinal < arrowCount; ++ordinal) {
            const auto state = memo.find(stateKey(start, ordinal));
            if (state == memo.end()) {
                return false;
            }
            const int length = static_cast<int>(
                state->second.selectedLength
            );
            if (length <= 0) {
                return false;
            }
            const int end = start + length - 1;
            result.push_back({start, end});
            start = end + 1;
        }
        return start == area;
    };

    if (!mixedLengths || area >= 4096) {
        return tryBounds();
    }

    constexpr std::array<int, 8> expansions{{
        0,
        2,
        6,
        16,
        32,
        64,
        128,
        256
    }};
    for (int expansion : expansions) {
        for (int ordinal = 0; ordinal < arrowCount; ++ordinal) {
            targetMinimums[static_cast<std::size_t>(ordinal)] =
                std::max(
                    minimumLength,
                    lengthProfile.minimums[
                        static_cast<std::size_t>(ordinal)
                    ] - expansion
                );
            targetMaximums[static_cast<std::size_t>(ordinal)] =
                std::min(
                    maximumLength,
                    lengthProfile.maximums[
                        static_cast<std::size_t>(ordinal)
                    ] + expansion
                );
        }
        if (tryBounds()) {
            return true;
        }
    }

    std::ranges::fill(targetMinimums, minimumLength);
    std::ranges::fill(targetMaximums, maximumLength);
    return tryBounds();
}

std::vector<int> preferredArrowCounts(
    int area,
    int minimumLength,
    int maximumLength
) {
    const int minimumCount =
        (area + maximumLength - 1) / maximumLength;
    const int maximumCount = area / minimumLength;
    const double requestedPreferredLength =
        minimumLength <= 2 && maximumLength >= 20
        ? (area >= 1024 ? 9.0 : 8.0)
        : 26.0;
    const double preferredLength = std::clamp(
        requestedPreferredLength,
        static_cast<double>(minimumLength),
        static_cast<double>(maximumLength)
    );
    const int preferred = std::clamp(
        static_cast<int>(std::lround(area / preferredLength)),
        minimumCount,
        maximumCount
    );

    std::vector<int> result;
    const int count = maximumCount - minimumCount + 1;
    result.reserve(static_cast<std::size_t>(count));
    result.push_back(preferred);
    for (int distance = 1;
         static_cast<int>(result.size()) < count;
         ++distance) {
        const int shorterArrows = preferred + distance;
        const int longerArrows = preferred - distance;
        if (longerArrows >= minimumCount) {
            result.push_back(longerArrows);
        }
        if (shorterArrows <= maximumCount) {
            result.push_back(shorterArrows);
        }
    }
    return result;
}

std::vector<Arrow> arrowsFromIntervals(
    const std::vector<Cell>& path,
    const std::vector<Interval>& intervals
) {
    std::vector<Arrow> arrows;
    arrows.reserve(intervals.size());
    for (const Interval interval : intervals) {
        Arrow value;
        value.cells.assign(
            path.begin() + interval.start,
            path.begin() + interval.end + 1
        );
        value.direction = value.cells[0] - value.cells[1];
        arrows.push_back(std::move(value));
    }
    return arrows;
}

int directionMask(const std::vector<Arrow>& arrows) {
    int mask = 0;
    for (const Arrow& value : arrows) {
        if (value.direction == Cell{-1, 0}) {
            mask |= 1;
        } else if (value.direction == Cell{1, 0}) {
            mask |= 2;
        } else if (value.direction == Cell{0, -1}) {
            mask |= 4;
        } else if (value.direction == Cell{0, 1}) {
            mask |= 8;
        }
    }
    return mask;
}

TangleMetrics measureTangle(
    int columns,
    int rows,
    const std::vector<Arrow>& arrows
) {
    TangleMetrics result;
    const int arrowCount = static_cast<int>(arrows.size());
    std::vector<int> owners(
        static_cast<std::size_t>(columns * rows),
        -1
    );
    std::vector<int> bodyPositions(
        static_cast<std::size_t>(columns * rows),
        -1
    );
    std::vector<int> arrowTurns(arrows.size(), 0);
    std::vector<int> arrowBoundingAreas(arrows.size(), 0);
    std::vector<double> longArrowBoundingFills;
    longArrowBoundingFills.reserve(arrows.size());

    for (std::size_t index = 0; index < arrows.size(); ++index) {
        const Arrow& value = arrows[index];
        int minimumX = columns;
        int maximumX = -1;
        int minimumY = rows;
        int maximumY = -1;
        int borderCells = 0;
        for (std::size_t position = 0;
             position < value.cells.size();
             ++position) {
            const Cell cell = value.cells[position];
            const std::size_t indexInBoard = static_cast<std::size_t>(
                cellIndex(cell, columns)
            );
            owners[indexInBoard] =
                static_cast<int>(index);
            bodyPositions[indexInBoard] = static_cast<int>(position);
            minimumX = std::min(minimumX, cell.x);
            maximumX = std::max(maximumX, cell.x);
            minimumY = std::min(minimumY, cell.y);
            maximumY = std::max(maximumY, cell.y);
            borderCells += cell.x == 0 || cell.y == 0 ||
                cell.x == columns - 1 || cell.y == rows - 1 ? 1 : 0;
        }

        result.shortBorderCells += value.cells.size() <= 5 ? borderCells : 0;
        if (borderCells < static_cast<int>(value.cells.size())) {
            result.interiorLinkedBorderCells += borderCells;
        }

        const int boundingArea =
            (maximumX - minimumX + 1) * (maximumY - minimumY + 1);
        arrowBoundingAreas[index] = boundingArea;
        result.boundingBoxEmptyCells +=
            boundingArea - static_cast<int>(value.cells.size());
        const double fill = static_cast<double>(value.cells.size()) /
                            static_cast<double>(boundingArea);
        if (value.cells.size() <= 5) {
            ++result.shortArrows;
        } else if (value.cells.size() <= 12) {
            ++result.mediumArrows;
        } else {
            ++result.longArrows;
            result.veryLongArrows +=
                value.cells.size() >= 18 ? 1 : 0;
            longArrowBoundingFills.push_back(fill);
            result.rectangularArrows += fill > 0.9 ? 1 : 0;
        }
        const Cell head = value.cells.front();
        const bool atCorner =
            (head.x == minimumX || head.x == maximumX) &&
            (head.y == minimumY || head.y == maximumY);
        result.headAtBoundingCorner +=
            atCorner && value.cells.size() > 5 ? 1 : 0;

        for (std::size_t cell = 2; cell < value.cells.size(); ++cell) {
            const Cell before =
                value.cells[cell - 1] - value.cells[cell - 2];
            const Cell after =
                value.cells[cell] - value.cells[cell - 1];
            if (before != after) {
                ++result.turns;
                ++arrowTurns[index];
            }
        }
    }

    for (std::size_t index = 0; index < arrows.size(); ++index) {
        const Arrow& value = arrows[index];
        int minimumClearance = std::numeric_limits<int>::max();
        Cell probe = value.cells.front() + value.direction;
        int distance = 1;
        while (isInside(probe, columns, rows)) {
            const std::size_t indexInBoard = static_cast<std::size_t>(
                cellIndex(probe, columns)
            );
            if (owners[indexInBoard] == static_cast<int>(index)) {
                const int position = bodyPositions[indexInBoard];
                const int clearance =
                    distance -
                    (static_cast<int>(value.cells.size()) - position);
                minimumClearance = std::min(
                    minimumClearance,
                    clearance
                );
            }
            probe = probe + value.direction;
            ++distance;
        }
        if (minimumClearance != 0) {
            continue;
        }

        ++result.tightSelfPassArrows;
        const int length = static_cast<int>(value.cells.size());
        const int boundingArea = arrowBoundingAreas[index];
        const bool rectangularThread =
            length > 12 && length * 10 > boundingArea * 9;
        if (rectangularThread) {
            continue;
        }
        ++result.rewardedTightSelfPassArrows;
        const bool complexThread =
            arrowTurns[index] >= 2 &&
            boundingArea - length >= 3;
        if (length >= 18 && complexThread) {
            ++result.longComplexTightSelfPassArrows;
        }
        result.tightSelfPassComplexity +=
            std::min(length, 40) +
            std::min(arrowTurns[index], 12) * 4;
    }

    if (!longArrowBoundingFills.empty()) {
        std::ranges::sort(longArrowBoundingFills);
        result.medianBoundingFill = longArrowBoundingFills[
            longArrowBoundingFills.size() / 2
        ];
    }
    result.directionCount = std::popcount(
        static_cast<unsigned int>(directionMask(arrows))
    );

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const int owner = owners[static_cast<std::size_t>(
                y * columns + x
            )];
            if (x + 1 < columns) {
                const int other = owners[static_cast<std::size_t>(
                    y * columns + x + 1
                )];
                if (owner != other) {
                    ++result.interArrowEdges;
                    ++result.horizontalInterArrowEdges;
                }
            }
            if (y + 1 < rows) {
                const int other = owners[static_cast<std::size_t>(
                    (y + 1) * columns + x
                )];
                if (owner != other) {
                    ++result.interArrowEdges;
                    ++result.verticalInterArrowEdges;
                }
            }
        }
    }

    const auto countRuns = [&](bool rowsFirst) {
        const int outer = rowsFirst ? rows : columns;
        const int inner = rowsFirst ? columns : rows;
        for (int line = 0; line < outer; ++line) {
            std::vector<int> runs(static_cast<std::size_t>(arrowCount), 0);
            int previous = -1;
            for (int position = 0; position < inner; ++position) {
                const int x = rowsFirst ? position : line;
                const int y = rowsFirst ? line : position;
                const int owner = owners[static_cast<std::size_t>(
                    y * columns + x
                )];
                if (owner != previous) {
                    ++runs[static_cast<std::size_t>(owner)];
                    previous = owner;
                }
            }
            for (int count : runs) {
                result.scanlineExcessRuns += std::max(0, count - 1);
            }
        }
    };
    countRuns(true);
    countRuns(false);

    if (columns == 20 && rows == 20) {
        for (int blockY = 0; blockY < 4; ++blockY) {
            for (int blockX = 0; blockX < 4; ++blockX) {
                std::vector<bool> seen(arrows.size(), false);
                int distinct = 0;
                for (int y = blockY * 5; y < blockY * 5 + 5; ++y) {
                    for (int x = blockX * 5; x < blockX * 5 + 5; ++x) {
                        const int owner = owners[static_cast<std::size_t>(
                            y * columns + x
                        )];
                        if (!seen[static_cast<std::size_t>(owner)]) {
                            seen[static_cast<std::size_t>(owner)] = true;
                            ++distinct;
                        }
                    }
                }
                result.richMacroBlocks += distinct >= 4 ? 1 : 0;
            }
        }
    }

    std::vector<std::vector<int>> blockers(arrows.size());
    std::vector<std::vector<int>> dependents(arrows.size());
    std::vector<int> seenBlockers(arrows.size(), -1);
    for (std::size_t index = 0; index < arrows.size(); ++index) {
        Cell probe = arrows[index].cells.front() + arrows[index].direction;
        while (isInside(probe, columns, rows)) {
            const int owner = owners[static_cast<std::size_t>(
                cellIndex(probe, columns)
            )];
            if (owner != static_cast<int>(index) &&
                seenBlockers[static_cast<std::size_t>(owner)] !=
                    static_cast<int>(index)) {
                seenBlockers[static_cast<std::size_t>(owner)] =
                    static_cast<int>(index);
                blockers[index].push_back(owner);
                dependents[static_cast<std::size_t>(owner)].push_back(
                    static_cast<int>(index)
                );
            }
            probe = probe + arrows[index].direction;
        }
    }

    std::vector<int> remainingBlockers(arrows.size(), 0);
    std::vector<std::uint8_t> present(arrows.size(), 1U);
    int choices = 0;
    for (std::size_t index = 0; index < arrows.size(); ++index) {
        remainingBlockers[index] =
            static_cast<int>(blockers[index].size());
        choices += remainingBlockers[index] == 0 ? 1 : 0;
    }

    for (std::size_t step = 0; step < arrows.size(); ++step) {
        if (step == 0) {
            result.initiallyRemovable = choices;
        }
        result.maximumChoices = std::max(result.maximumChoices, choices);
        result.choiceExcess += std::max(0, choices - 1);
        if (remainingBlockers[step] != 0) {
            return {};
        }
        present[step] = 0U;
        --choices;
        for (int dependent : dependents[step]) {
            int& count =
                remainingBlockers[static_cast<std::size_t>(dependent)];
            --count;
            if (count == 0 &&
                present[static_cast<std::size_t>(dependent)] != 0U) {
                ++choices;
            }
        }
    }
    result.intendedOrderValid = true;
    return result;
}

int minimumCountForShare(int arrowCount, int percentage) {
    return (arrowCount * percentage + 99) / 100;
}

bool meetsDefaultQualityFloor(
    const TangleMetrics& metrics,
    const std::vector<Arrow>& arrows
) {
    const int arrowCount = static_cast<int>(arrows.size());
    std::vector<int> lengths;
    lengths.reserve(arrows.size());
    int bentMedium = 0;
    int bentLong = 0;
    int longWithTwoTurns = 0;
    double squaredLengths = 0;
    int totalLength = 0;
    for (const auto& value : arrows) {
        const int length = static_cast<int>(value.cells.size());
        lengths.push_back(length);
        totalLength += length;
        squaredLengths += static_cast<double>(length) * length;
        int turns = 0;
        for (std::size_t cell = 2; cell < value.cells.size(); ++cell) {
            turns += value.cells[cell - 1] - value.cells[cell - 2] !=
                     value.cells[cell] - value.cells[cell - 1] ? 1 : 0;
        }
        bentMedium += length > 5 && length <= 12 && turns > 0 ? 1 : 0;
        bentLong += length > 12 && turns > 0 ? 1 : 0;
        longWithTwoTurns += length > 12 && turns >= 2 ? 1 : 0;
    }
    if (lengths.empty()) return false;
    std::ranges::sort(lengths);
    const int firstQuartile = lengths[lengths.size() / 4];
    const int median = lengths[lengths.size() / 2];
    const int thirdQuartile = lengths[lengths.size() * 3 / 4];
    const double mean = static_cast<double>(totalLength) / arrowCount;
    const double coefficientOfVariation =
        std::sqrt(std::max(0.0, squaredLengths / arrowCount - mean * mean)) / mean;
    const int minimumShort = minimumCountForShare(arrowCount, 32);
    const int minimumMedium = minimumCountForShare(arrowCount, 34);
    const int minimumLong = minimumCountForShare(arrowCount, 10);
    const int maximumLong = arrowCount * 24 / 100;
    return
        arrowCount >= 48 &&
        arrowCount <= 54 &&
        metrics.initiallyRemovable >= 2 &&
        metrics.initiallyRemovable <= 5 &&
        metrics.directionCount >= 3 &&
        metrics.shortArrows >= minimumShort &&
        metrics.mediumArrows >= minimumMedium &&
        metrics.longArrows >= minimumLong &&
        metrics.longArrows <= maximumLong &&
        metrics.veryLongArrows >= 2 &&
        bentMedium * 5 >= metrics.mediumArrows * 3 &&
        bentLong * 5 >= metrics.longArrows * 4 &&
        longWithTwoTurns * 5 >= metrics.longArrows * 3 &&
        firstQuartile <= 5 && median >= 6 && median <= 9 &&
        thirdQuartile >= 8 && thirdQuartile <= 12 &&
        thirdQuartile - firstQuartile >= 3 &&
        coefficientOfVariation >= 0.40 && lengths.front() <= 3 &&
        metrics.turns >= 55 &&
        metrics.turns <= 160 &&
        metrics.interArrowEdges >= 300 &&
        metrics.horizontalInterArrowEdges * 100 >=
            metrics.interArrowEdges * 35 &&
        metrics.verticalInterArrowEdges * 100 >=
            metrics.interArrowEdges * 35 &&
        metrics.medianBoundingFill <= 0.70 &&
        metrics.rectangularArrows == 0 &&
        metrics.richMacroBlocks >= 12 &&
        metrics.maximumChoices <= 10 &&
        metrics.choiceExcess <= arrowCount * 6;
}

int scoreCandidate(
    const TangleMetrics& metrics,
    int arrowCount,
    int desiredInitial
) {
    const int desiredChoiceExcess = std::max(2, arrowCount * 2 / 3);
    const int desiredShort = arrowCount * 40 / 100;
    const int desiredLong = std::max(1, arrowCount * 14 / 100);
    constexpr int tightSelfPassArrowBonus = 18'000;
    constexpr int longComplexTightSelfPassBonus = 35'000;
    constexpr int tightSelfPassComplexityBonus = 100;
    return
        metrics.interArrowEdges * 180 +
        std::min(
            metrics.horizontalInterArrowEdges,
            metrics.verticalInterArrowEdges
        ) * 120 +
        std::min(metrics.scanlineExcessRuns, 6) * 300 +
        metrics.boundingBoxEmptyCells * 8 +
        metrics.turns * 6 +
        metrics.directionCount * 8'000 +
        metrics.richMacroBlocks * 600 +
        metrics.rewardedTightSelfPassArrows *
            tightSelfPassArrowBonus +
        metrics.longComplexTightSelfPassArrows *
            longComplexTightSelfPassBonus +
        metrics.tightSelfPassComplexity *
            tightSelfPassComplexityBonus -
        // A compact rectangular S is visually a territory, not a thread.
        // Make even one such arrow dominate small improvements in the global
        // interface score so a genuinely interwoven candidate wins.
        metrics.rectangularArrows * 500'000 -
        metrics.headAtBoundingCorner * 180 -
        std::abs(metrics.shortArrows - desiredShort) * 25'000 -
        std::abs(metrics.longArrows - desiredLong) * 16'000 -
        std::max(0, 3 - metrics.veryLongArrows) * 20'000 -
        std::abs(metrics.choiceExcess - desiredChoiceExcess) * 220 -
        std::max(0, metrics.maximumChoices - 5) * 2'000 -
        std::abs(
            metrics.initiallyRemovable - desiredInitial
        ) * 20'000;
}

} // namespace

Board generateBoard(const GeneratorOptions& options) {
    if (options.columns < 3 || options.rows < 2) {
        throw std::invalid_argument(
            "Generator requires at least a 3 x 2 board"
        );
    }
    if (options.columns % 2 != 0 || options.rows % 2 != 0) {
        throw std::invalid_argument(
            "Labyrinth generator requires even board dimensions"
        );
    }

    const int area = options.columns * options.rows;
    const int minimumLength = std::clamp(
        options.minimumLength,
        2,
        area
    );
    const int maximumLength = std::clamp(
        std::max(minimumLength, options.maximumLength),
        minimumLength,
        area
    );
    std::seed_seq seed{
        options.seed,
        static_cast<std::uint32_t>(options.columns),
        static_cast<std::uint32_t>(options.rows),
        static_cast<std::uint32_t>(minimumLength),
        static_cast<std::uint32_t>(maximumLength),
        0xC10BBE2U
    };
    std::mt19937 random(seed);
    const auto [minimumInitialChoices, maximumInitialChoices] =
        initialChoiceRange(options.columns, options.rows);
    const int initialChoiceTarget =
        std::uniform_int_distribution<int>(
            minimumInitialChoices,
            maximumInitialChoices
        )(random);
    const int candidateAttempts = std::max(
        1,
        options.candidateAttempts
    );
    const bool enforceDefaultQuality =
        options.columns == 20 &&
        options.rows == 20 &&
        minimumLength == 2 &&
        maximumLength == 30;
    const bool enforceHardQuality =
        options.columns == 80 && options.rows == 80 &&
        minimumLength == 2 && maximumLength == 120;
    // The frontier quality profile assumes a substantial interior. Preserve
    // the generic path cutter for narrow custom boards and length constraints.
    const bool useFrontiers = options.columns >= 128 && options.rows >= 128 &&
        minimumLength == 2 && maximumLength >= 30;

    BoardCandidate best;
    const bool supportsLabyrinth =
        area / minimumLength >= 3;

    if (supportsLabyrinth) {
        const bool useReachabilityPruning = area >= 256;
        const std::vector<int> counts = preferredArrowCounts(
            area,
            minimumLength,
            maximumLength
        );
        const std::size_t preferredCountAttempts =
            area >= 4096 && area < 16384
            ? 33U
            : 17U;
        const std::size_t countAttemptLimit =
            area >= 256
            ? std::min(
                  preferredCountAttempts,
                  counts.size()
              )
            : counts.size();

        const auto generateAttempt = [&](int attempt) {
            std::seed_seq attemptSeed{
                options.seed,
                static_cast<std::uint32_t>(options.columns),
                static_cast<std::uint32_t>(options.rows),
                static_cast<std::uint32_t>(minimumLength),
                static_cast<std::uint32_t>(maximumLength),
                static_cast<std::uint32_t>(attempt),
                0xA77E6D5U
            };
            std::mt19937 attemptRandom(attemptSeed);
            if (useFrontiers) {
                // Fixed work budgets preserve reproducibility and bound retries.
                // No whole-board count/length DP or ray cache is needed here.
                constexpr int cycleAttempts = 32;
                for (int retry = 0; retry < cycleAttempts; ++retry) {
                    const auto cycle = makeLabyrinthCycle(
                        options.columns, options.rows, attemptRandom
                    );
                    auto arrows = detail::partitionFromFrontiers(
                        cycle, options.columns, options.rows, maximumLength,
                        options.allowArrowLoops, initialChoiceTarget,
                        maximumInitialChoices, attemptRandom
                    );
                    if (arrows.empty()) {
                        continue;
                    }
                    const auto metrics = measureTangle(options.columns, options.rows, arrows);
                    const int perimeter = 2 * (options.columns + options.rows) - 4;
                    const int arrowCount = static_cast<int>(arrows.size());
                    const bool quality = metrics.intendedOrderValid &&
                        metrics.initiallyRemovable >= minimumInitialChoices &&
                        metrics.initiallyRemovable <= maximumInitialChoices &&
                        metrics.directionCount == 4 && metrics.rectangularArrows == 0 &&
                        metrics.shortArrows * 100 >= arrowCount * 30 &&
                        metrics.shortArrows * 100 <= arrowCount * 60 &&
                        metrics.mediumArrows * 100 >= arrowCount * 30 &&
                        metrics.longArrows * 100 >= arrowCount * 9 &&
                        metrics.shortBorderCells * 5 <= perimeter &&
                        metrics.interiorLinkedBorderCells * 4 >= perimeter * 3;
                    if (!quality) {
                        continue;
                    }
                    const int score = scoreCandidate(metrics, arrowCount, initialChoiceTarget) -
                        metrics.shortBorderCells * 2'000 + metrics.interiorLinkedBorderCells * 500;
                    return BoardCandidate{std::move(arrows), metrics, true, score};
                }
                return BoardCandidate{};
            }
            const std::vector<Cell> cycle = makeLabyrinthCycle(
                options.columns,
                options.rows,
                attemptRandom
            );
            std::vector<Opening> openings = findOpenings(
                cycle,
                options.columns,
                options.rows
            );
            std::ranges::shuffle(openings, attemptRandom);

            for (Opening opening : openings) {
                const std::vector<Cell> path = openCycle(cycle, opening);
                const std::vector<RayInfo> rays = analyzeRays(
                    path,
                    options.columns,
                    options.rows
                );
                const IntervalFacts intervalFacts =
                    buildIntervalFacts(
                        rays,
                        minimumLength,
                        maximumLength,
                        options.allowArrowLoops
                    );
                std::vector<std::uint64_t> reachableCounts;
                int maximumAttemptedCount = 0;
                if (useReachabilityPruning) {
                    for (std::size_t countIndex = 0;
                         countIndex < countAttemptLimit;
                         ++countIndex) {
                        maximumAttemptedCount = std::max(
                            maximumAttemptedCount,
                            counts[countIndex]
                        );
                    }
                    if (area >= 4096 && area < 16384) {
                        maximumAttemptedCount = std::max(
                            maximumAttemptedCount,
                            area / minimumLength
                        );
                    }
                    reachableCounts =
                        area >= 4096
                        ? reachableArrowCountsByRanges(
                              intervalFacts,
                              minimumLength,
                              maximumLength,
                              maximumAttemptedCount
                          )
                        : reachableArrowCounts(
                              intervalFacts,
                              minimumLength,
                              maximumLength,
                              maximumAttemptedCount
                          );
                }

                std::vector<int> countsForOpening(
                    counts.begin(),
                    counts.begin() +
                        static_cast<std::ptrdiff_t>(
                            countAttemptLimit
                        )
                );
                if (useReachabilityPruning &&
                    std::ranges::none_of(
                        countsForOpening,
                        [&](int count) {
                            return containsArrowCount(
                                reachableCounts,
                                count
                            );
                        }
                    )) {
                    const int fallbackCount =
                        highestReachableArrowCount(
                            reachableCounts,
                            (area + maximumLength - 1) /
                                maximumLength,
                            maximumAttemptedCount
                        );
                    if (fallbackCount > 0) {
                        countsForOpening.push_back(fallbackCount);
                    }
                }

                for (int count : countsForOpening) {
                    if (useReachabilityPruning &&
                        !containsArrowCount(reachableCounts, count)) {
                        continue;
                    }
                    std::vector<Interval> intervals;
                    const bool foundIntervals = findIntervalsForCount(
                        path,
                        rays,
                        intervalFacts,
                        options.columns,
                        options.rows,
                        minimumLength,
                        maximumLength,
                        count,
                        initialChoiceTarget,
                        attemptRandom,
                        intervals
                    );
                    if (!foundIntervals) {
                        continue;
                    }

                    std::vector<Arrow> arrows = arrowsFromIntervals(
                        path,
                        intervals
                    );
                    const TangleMetrics metrics = measureTangle(
                        options.columns,
                        options.rows,
                        arrows
                    );
                    if (!metrics.intendedOrderValid) {
                        continue;
                    }
                    const int score = scoreCandidate(
                        metrics,
                        static_cast<int>(arrows.size()),
                        initialChoiceTarget
                    );
                    const bool meetsRequiredQuality =
                        (!enforceDefaultQuality || meetsDefaultQualityFloor(
                            metrics,
                            arrows
                        )) &&
                        (!enforceHardQuality ||
                         (metrics.initiallyRemovable >= minimumInitialChoices &&
                          metrics.initiallyRemovable <= maximumInitialChoices));
                    return BoardCandidate{
                        std::move(arrows),
                        metrics,
                        meetsRequiredQuality,
                        score
                    };
                }
            }
            return BoardCandidate{};
        };

        std::vector<std::future<BoardCandidate>> futures;
        std::vector<BoardCandidate> frontierCandidates;
        if (useFrontiers && candidateAttempts > 1) {
            frontierCandidates.resize(static_cast<std::size_t>(candidateAttempts));
            const int workers = std::min(candidateAttempts, 4);
            std::vector<std::future<void>> jobs;
            for (int worker = 0; worker < workers; ++worker) {
                jobs.push_back(std::async(std::launch::async, [&, worker] {
                    for (int attempt = worker; attempt < candidateAttempts; attempt += workers) {
                        frontierCandidates[attempt] = generateAttempt(attempt);
                    }
                }));
            }
            for (auto& job : jobs) {
                job.get();
            }
        }
        if (!useFrontiers && candidateAttempts > 1) {
            futures.reserve(static_cast<std::size_t>(candidateAttempts));
            for (int attempt = 0;
                 attempt < candidateAttempts;
                 ++attempt) {
                futures.push_back(std::async(
                    std::launch::async,
                    generateAttempt,
                    attempt
                ));
            }
        }

        for (int attempt = 0;
             attempt < candidateAttempts;
             ++attempt) {
            BoardCandidate candidate =
                candidateAttempts == 1
                ? generateAttempt(attempt)
                : useFrontiers ? std::move(frontierCandidates[attempt])
                : futures[static_cast<std::size_t>(attempt)].get();
            if (candidate.arrows.empty()) {
                continue;
            }
            const bool improvesEligibility =
                candidate.meetsRequiredQuality &&
                !best.meetsRequiredQuality;
            if (improvesEligibility ||
                (candidate.meetsRequiredQuality ==
                     best.meetsRequiredQuality &&
                 candidate.score > best.score)) {
                best = std::move(candidate);
            }
        }
    }

    if (best.arrows.empty() || !best.meetsRequiredQuality) {
        throw std::runtime_error(
            "Unable to generate a valid " +
            std::to_string(options.columns) + "x" +
            std::to_string(options.rows) +
            " labyrinth board for seed " +
            std::to_string(options.seed) + " after " +
            std::to_string(candidateAttempts) +
            " candidate attempt(s)"
        );
    }

    Board board(options.columns, options.rows, std::move(best.arrows));
    if (!board.isCompletelyFilled() || !board.hasValidGeometry()) {
        throw std::logic_error("Generated board failed its invariants");
    }
    return board;
}

} // namespace arrow
