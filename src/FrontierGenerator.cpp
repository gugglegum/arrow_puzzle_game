#include "FrontierGenerator.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace arrow::detail {
namespace {

// Indexed sets avoid accumulating stale heap entries as exit rays change.
class FrontPool {
public:
    explicit FrontPool(int size) : indices_(size, -1) {
        entries_.reserve(static_cast<std::size_t>(size));
    }

    void set(int head, bool enabled) {
        int& index = indices_[head];
        if (enabled && index < 0) {
            index = static_cast<int>(entries_.size());
            entries_.push_back(head);
        } else if (!enabled && index >= 0) {
            const int last = entries_.back();
            entries_[index] = last;
            indices_[last] = index;
            entries_.pop_back();
            index = -1;
        }
    }

    [[nodiscard]] bool empty() const { return entries_.empty(); }

    [[nodiscard]] int sample(std::mt19937& random) const {
        return entries_[random() % entries_.size()];
    }

private:
    std::vector<int> entries_;
    std::vector<int> indices_;
};

class FrontierPartition {
public:
    FrontierPartition(
        const std::vector<Cell>& cycle,
        int columns,
        int rows,
        int maximumLength,
        bool allowArrowLoops
    ) : cycle_(cycle), columns_(columns), rows_(rows),
        area_(static_cast<int>(cycle.size())),
        // Long threads need not span hundreds of cells to make a large board
        // difficult. This also bounds the local work when a fragment is cut.
        lengthLimit_(std::min(maximumLength, 120)),
        positions_(area_), present_(area_, 1U),
        directions_(area_ * 2), safeLengths_(area_ * 2, lengthLimit_),
        availableLengths_(area_ * 2, lengthLimit_ + 2),
        minimumLengths_(area_ * 2, 2), stamps_(area_ * 2, -1),
        initial_(area_ * 2), dependent_(area_ * 2) {
        dirty_.reserve(static_cast<std::size_t>(area_ * 2));
        for (int position = 0; position < area_; ++position) {
            positions_[cellIndex(cycle_[position])] = position;
            directions_[position * 2] =
                cycle_[position] - cycle_[wrap(position + 1)];
            directions_[position * 2 + 1] =
                cycle_[position] - cycle_[wrap(position - 1)];
        }

        for (int head = 0; head < area_ * 2; ++head) {
            Cell probe = cycle_[head / 2] + directions_[head];
            int distance = 1;
            while (inside(probe)) {
                const int offset = bodyOffset(head, positions_[cellIndex(probe)]);
                // At tick d, body cell k is free iff length <= k + d.
                // Without loops, that cell must be outside the body altogether.
                safeLengths_[head] = std::min(
                    safeLengths_[head],
                    allowArrowLoops ? offset + distance : offset
                );
                probe = probe + directions_[head];
                ++distance;
            }
            updateFront(head);
        }
    }

    [[nodiscard]] std::vector<Arrow> build(
        int initialChoiceTarget,
        int maximumInitialChoices,
        std::mt19937& random
    ) {
        std::vector<Arrow> arrows;
        arrows.reserve(static_cast<std::size_t>(area_ / 6));
        int usedInitial = 0;
        int removed = 0;
        while (removed < area_) {
            const bool seedFront = !initial_.empty() &&
                (usedInitial < initialChoiceTarget ||
                 (dependent_.empty() && usedInitial < maximumInitialChoices));
            const FrontPool& pool = seedFront ? initial_ : dependent_;
            if (pool.empty()) {
                return {};
            }

            const int band = static_cast<int>(random() % 100U);
            const int target = seedFront
                ? 32 + static_cast<int>(random() % 25U)
                : band < 30 ? 2 + static_cast<int>(random() % 4U)
                : band < 80 ? 6 + static_cast<int>(random() % 6U)
                : 18 + static_cast<int>(random() % 15U);
            const auto [head, length] = chooseFragment(pool, target, random);
            if (head < 0) {
                return {};
            }

            Arrow chosen;
            chosen.direction = directions_[head];
            chosen.cells.reserve(static_cast<std::size_t>(length));
            for (int offset = 0; offset < length; ++offset) {
                const int position = wrap(head / 2 + offset * step(head));
                chosen.cells.push_back(cycle_[position]);
                present_[position] = 0U;
                for (int orientation = 0; orientation < 2; ++orientation) {
                    initial_.set(position * 2 + orientation, false);
                    dependent_.set(position * 2 + orientation, false);
                }
            }
            usedInitial += seedFront ? 1 : 0;
            removed += length;
            refreshAfterCut(head, length, chosen.cells, static_cast<int>(arrows.size()));
            arrows.push_back(std::move(chosen));
        }
        return arrows;
    }

private:
    [[nodiscard]] int wrap(int position) const {
        return (position + area_) % area_;
    }

    [[nodiscard]] static int step(int head) { return head % 2 == 0 ? 1 : -1; }

    [[nodiscard]] int bodyOffset(int head, int position) const {
        return wrap((position - head / 2) * step(head));
    }

    [[nodiscard]] int cellIndex(Cell cell) const { return cell.y * columns_ + cell.x; }

    [[nodiscard]] bool inside(Cell cell) const {
        return cell.x >= 0 && cell.x < columns_ && cell.y >= 0 && cell.y < rows_;
    }

    void updateFront(int head) {
        int minimum = 2;
        int maximum = std::min(safeLengths_[head], availableLengths_[head]);
        if (maximum == availableLengths_[head] - 1) {
            --maximum;
        }
        // Neither side of a cut may leave a one-cell fragment. Unlike fixed
        // domino tiling, this allows odd lengths and both orientations anywhere.
        const bool strandsPredecessor =
            present_[wrap(head / 2 - step(head))] != 0U &&
            present_[wrap(head / 2 - 2 * step(head))] == 0U;
        bool hasEarlierBlocker = false;
        if (present_[head / 2] != 0U && maximum >= 2 && !strandsPredecessor) {
            Cell probe = cycle_[head / 2] + directions_[head];
            while (inside(probe)) {
                const int position = positions_[cellIndex(probe)];
                if (present_[position] != 0U) {
                    minimum = std::max(minimum, bodyOffset(head, position) + 1);
                    if (minimum > maximum) {
                        break;
                    }
                } else {
                    hasEarlierBlocker = true;
                }
                probe = probe + directions_[head];
            }
        } else {
            minimum = maximum + 1;
        }
        minimumLengths_[head] = minimum;
        const bool valid = minimum <= maximum;
        initial_.set(head, valid && !hasEarlierBlocker);
        dependent_.set(head, valid && hasEarlierBlocker);
    }

    [[nodiscard]] std::pair<int, int> chooseFragment(
        const FrontPool& pool,
        int target,
        std::mt19937& random
    ) const {
        int bestHead = -1;
        int bestLength = 0;
        int bestScore = std::numeric_limits<int>::min();
        // A fixed sampling budget bounds each decision; a failed construction
        // can be retried with another cycle, never with relaxed quality rules.
        constexpr int samples = 48;
        for (int sample = 0; sample < samples; ++sample) {
            const int head = pool.sample(random);
            const int longest = std::min(
                std::max(target + 6, minimumLengths_[head] + 6),
                std::min(safeLengths_[head], availableLengths_[head])
            );
            int minimumX = columns_;
            int maximumX = -1;
            int minimumY = rows_;
            int maximumY = -1;
            int borderCells = 0;
            int turns = 0;
            for (int offset = 0; offset < longest; ++offset) {
                const Cell cell = cycle_[wrap(head / 2 + offset * step(head))];
                minimumX = std::min(minimumX, cell.x);
                maximumX = std::max(maximumX, cell.x);
                minimumY = std::min(minimumY, cell.y);
                maximumY = std::max(maximumY, cell.y);
                borderCells += cell.x == 0 || cell.y == 0 ||
                    cell.x == columns_ - 1 || cell.y == rows_ - 1 ? 1 : 0;
                if (offset >= 2) {
                    const Cell before = cycle_[wrap(head / 2 + (offset - 2) * step(head))];
                    const Cell previous = cycle_[wrap(head / 2 + (offset - 1) * step(head))];
                    turns += previous - before != cell - previous ? 1 : 0;
                }
                const int length = offset + 1;
                if (length < minimumLengths_[head] ||
                    length == availableLengths_[head] - 1) {
                    continue;
                }
                const int boundingArea =
                    (maximumX - minimumX + 1) * (maximumY - minimumY + 1);
                if (length > 12 && length * 10 > boundingArea * 9) {
                    continue;
                }
                const int score = -std::abs(length - target) * 200 + turns * 10 +
                    (boundingArea - length) * 5 -
                    (length <= 5 ? borderCells * 1'000 : 0) +
                    (borderCells > 0 && borderCells < length ? 150 : 0) +
                    static_cast<int>(random() % 41U);
                if (score > bestScore) {
                    bestHead = head;
                    bestLength = length;
                    bestScore = score;
                }
            }
        }
        return {bestHead, bestLength};
    }

    void markDirty(int head, int stamp) {
        if (present_[head / 2] != 0U && stamps_[head] != stamp) {
            stamps_[head] = stamp;
            dirty_.push_back(head);
        }
    }

    void refreshAfterCut(int head, int length, const std::vector<Cell>& cells, int stamp) {
        dirty_.clear();
        // Discover affected rays spatially instead of storing an area-by-ray
        // reverse graph. Each cell is removed only once in a construction.
        constexpr std::array<Cell, 4> directions{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        for (Cell cell : cells) {
            for (Cell direction : directions) {
                Cell probe = cell + direction;
                while (inside(probe)) {
                    const int position = positions_[cellIndex(probe)];
                    for (int orientation = 0; orientation < 2; ++orientation) {
                        const int other = position * 2 + orientation;
                        if (directions_[other] == Cell{-direction.x, -direction.y}) {
                            markDirty(other, stamp);
                        }
                    }
                    probe = probe + direction;
                }
            }
        }

        const int first = step(head) == 1 ? head / 2 : wrap(head / 2 - length + 1);
        const int last = wrap(first + length - 1);
        // lengthLimit + 2 is a sentinel: at least two cells would remain after
        // even the longest cut. Smaller cached distances are exact.
        for (int distance = 1; distance <= lengthLimit_ + 1; ++distance) {
            const int position = wrap(first - distance);
            if (present_[position] == 0U) {
                break;
            }
            const int other = position * 2;
            availableLengths_[other] = std::min(availableLengths_[other], distance);
            markDirty(other, stamp);
            if (distance <= 2) {
                markDirty(other + 1, stamp);
            }
        }
        for (int distance = 1; distance <= lengthLimit_ + 1; ++distance) {
            const int position = wrap(last + distance);
            if (present_[position] == 0U) {
                break;
            }
            const int other = position * 2 + 1;
            availableLengths_[other] = std::min(availableLengths_[other], distance);
            markDirty(other, stamp);
            if (distance <= 2) {
                markDirty(other - 1, stamp);
            }
        }
        for (int other : dirty_) {
            updateFront(other);
        }
    }

    const std::vector<Cell>& cycle_;
    int columns_;
    int rows_;
    int area_;
    int lengthLimit_;
    std::vector<int> positions_;
    std::vector<std::uint8_t> present_;
    std::vector<Cell> directions_;
    std::vector<int> safeLengths_;
    std::vector<int> availableLengths_;
    std::vector<int> minimumLengths_;
    std::vector<int> stamps_;
    std::vector<int> dirty_;
    FrontPool initial_;
    FrontPool dependent_;
};

} // namespace

std::vector<Arrow> partitionFromFrontiers(
    const std::vector<Cell>& cycle,
    int columns,
    int rows,
    int maximumLength,
    bool allowArrowLoops,
    int initialChoiceTarget,
    int maximumInitialChoices,
    std::mt19937& random
) {
    return FrontierPartition(cycle, columns, rows, maximumLength, allowArrowLoops)
        .build(initialChoiceTarget, maximumInitialChoices, random);
}

} // namespace arrow::detail
