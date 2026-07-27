#include "arrow/MotionResolver.hpp"

#include <algorithm>
#include <cstddef>

namespace arrow {
namespace {

bool cellsOverlap(
    std::span<const Cell> lhs,
    std::span<const Cell> rhs
) {
    return std::ranges::any_of(lhs, [rhs](Cell cell) {
        return std::ranges::find(rhs, cell) != rhs.end();
    });
}

std::size_t pairIndex(
    std::size_t first,
    std::size_t second,
    std::size_t count
) {
    return first * count + second;
}

} // namespace

MotionResolution resolveMotionStep(
    std::span<const MotionBodyView> bodies,
    std::span<const std::uint8_t> previousMovingContacts
) {
    const std::size_t count = bodies.size();
    MotionResolution result{
        std::vector<std::uint8_t>(count, 0),
        std::vector<std::uint8_t>(count * count, 0),
        false
    };

    for (std::size_t index = 0; index < count; ++index) {
        const MotionBodyView& body = bodies[index];
        if (!body.present || !body.moving) {
            continue;
        }
        result.hardFailures[index] =
            body.selfBlocked || body.proposal.empty();
    }

    for (std::size_t moving = 0; moving < count; ++moving) {
        const MotionBodyView& body = bodies[moving];
        if (!body.present || !body.moving ||
            result.hardFailures[moving]) {
            continue;
        }
        for (std::size_t obstacle = 0; obstacle < count; ++obstacle) {
            const MotionBodyView& other = bodies[obstacle];
            if (obstacle == moving || !other.present || other.moving) {
                continue;
            }
            if (cellsOverlap(body.proposal, other.cells) ||
                cellsOverlap(body.proposal, other.reservedTarget)) {
                result.hardFailures[moving] = 1;
                break;
            }
        }
    }

    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (std::size_t moving = 0; moving < count; ++moving) {
            const MotionBodyView& body = bodies[moving];
            if (!body.present || !body.moving ||
                result.hardFailures[moving]) {
                continue;
            }
            for (std::size_t failed = 0; failed < count; ++failed) {
                const MotionBodyView& other = bodies[failed];
                if (failed == moving || !other.present || !other.moving ||
                    !result.hardFailures[failed]) {
                    continue;
                }
                if (cellsOverlap(body.proposal, other.cells)) {
                    result.hardFailures[moving] = 1;
                    expanded = true;
                    break;
                }
            }
        }
    }

    const bool hasPreviousMatrix =
        previousMovingContacts.size() == count * count;
    for (std::size_t first = 0; first < count; ++first) {
        const MotionBodyView& lhs = bodies[first];
        if (!lhs.present || !lhs.moving ||
            result.hardFailures[first]) {
            continue;
        }
        for (std::size_t second = first + 1;
             second < count;
             ++second) {
            const MotionBodyView& rhs = bodies[second];
            if (!rhs.present || !rhs.moving ||
                result.hardFailures[second]) {
                continue;
            }

            const bool headSwap =
                !lhs.proposal.empty() &&
                !rhs.proposal.empty() &&
                !lhs.cells.empty() &&
                !rhs.cells.empty() &&
                lhs.proposal.front() == rhs.cells.front() &&
                rhs.proposal.front() == lhs.cells.front();
            const bool contact =
                headSwap || cellsOverlap(lhs.proposal, rhs.proposal);
            if (!contact) {
                continue;
            }

            const std::size_t forward =
                pairIndex(first, second, count);
            const std::size_t reverse =
                pairIndex(second, first, count);
            result.movingContacts[forward] = 1;
            result.movingContacts[reverse] = 1;
            if (!hasPreviousMatrix ||
                previousMovingContacts[forward] == 0) {
                result.hasNewMovingContact = true;
            }
        }
    }

    return result;
}

} // namespace arrow
