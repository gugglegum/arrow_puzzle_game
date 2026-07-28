#include "arrow/MotionResolver.hpp"

#include <algorithm>
#include <cstddef>

namespace arrow {
namespace {

bool cellIsRelevant(
    Cell cell,
    MotionBounds bounds
) {
    if (bounds.columns <= 0 || bounds.rows <= 0) {
        return true;
    }
    return cell.x >= 0 && cell.x < bounds.columns &&
           cell.y >= 0 && cell.y < bounds.rows;
}

bool cellsOverlap(
    std::span<const Cell> lhs,
    std::span<const Cell> rhs,
    MotionBounds bounds
) {
    return std::ranges::any_of(lhs, [=](Cell cell) {
        if (!cellIsRelevant(cell, bounds)) {
            return false;
        }
        return std::ranges::find(rhs, cell) != rhs.end();
    });
}

bool hasUsableGrid(MotionBounds bounds) {
    return bounds.columns > 0 && bounds.rows > 0;
}

std::size_t cellIndex(Cell cell, MotionBounds bounds) {
    return static_cast<std::size_t>(cell.y) *
               static_cast<std::size_t>(bounds.columns) +
           static_cast<std::size_t>(cell.x);
}

void markCells(
    std::vector<std::uint8_t>& occupancy,
    std::span<const Cell> cells,
    MotionBounds bounds
) {
    for (Cell cell : cells) {
        if (cellIsRelevant(cell, bounds)) {
            occupancy[cellIndex(cell, bounds)] = 1;
        }
    }
}

bool overlapsOccupancy(
    std::span<const Cell> cells,
    std::span<const std::uint8_t> occupancy,
    MotionBounds bounds
) {
    return std::ranges::any_of(cells, [&](Cell cell) {
        return cellIsRelevant(cell, bounds) &&
               occupancy[cellIndex(cell, bounds)] != 0;
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
    std::span<const std::uint8_t> previousMovingContacts,
    MotionBounds bounds,
    bool trackMovingContacts
) {
    const std::size_t count = bodies.size();
    const bool hasPreviousMatrix =
        trackMovingContacts &&
        previousMovingContacts.size() == count * count;
    MotionResolution result{
        std::vector<std::uint8_t>(count, 0),
        !trackMovingContacts
            ? std::vector<std::uint8_t>{}
            : hasPreviousMatrix
            ? std::vector<std::uint8_t>(
                  previousMovingContacts.begin(),
                  previousMovingContacts.end()
              )
            : std::vector<std::uint8_t>(count * count, 0),
        false
    };

    for (std::size_t index = 0; index < count; ++index) {
        const MotionBodyView& body = bodies[index];
        if (!body.present ||
            body.role != MotionRole::Advancing) {
            continue;
        }
        result.hardFailures[index] =
            body.selfBlocked || body.proposal.empty();
    }

    if (hasUsableGrid(bounds)) {
        std::vector<std::uint8_t> staticOccupancy(
            static_cast<std::size_t>(bounds.columns) *
                static_cast<std::size_t>(bounds.rows),
            0
        );
        for (const MotionBodyView& body : bodies) {
            if (!body.present ||
                body.role != MotionRole::Static) {
                continue;
            }
            markCells(staticOccupancy, body.cells, bounds);
            markCells(
                staticOccupancy,
                body.reservedTarget,
                bounds
            );
        }
        for (std::size_t moving = 0;
             moving < count;
             ++moving) {
            const MotionBodyView& body = bodies[moving];
            if (!body.present ||
                body.role != MotionRole::Advancing ||
                result.hardFailures[moving]) {
                continue;
            }
            if (overlapsOccupancy(
                    body.proposal,
                    staticOccupancy,
                    bounds
                )) {
                result.hardFailures[moving] = 1;
            }
        }
    } else {
        for (std::size_t moving = 0; moving < count; ++moving) {
            const MotionBodyView& body = bodies[moving];
            if (!body.present ||
                body.role != MotionRole::Advancing ||
                result.hardFailures[moving]) {
                continue;
            }
            for (std::size_t obstacle = 0;
                 obstacle < count;
                 ++obstacle) {
                const MotionBodyView& other = bodies[obstacle];
                if (obstacle == moving || !other.present ||
                    other.role != MotionRole::Static) {
                    continue;
                }
                if (cellsOverlap(
                        body.proposal,
                        other.cells,
                        bounds
                    ) ||
                    cellsOverlap(
                        body.proposal,
                        other.reservedTarget,
                        bounds
                    )) {
                    result.hardFailures[moving] = 1;
                    break;
                }
            }
        }
    }

    bool expanded = true;
    while (expanded) {
        expanded = false;
        std::vector<std::uint8_t> failedOccupancy;
        if (hasUsableGrid(bounds)) {
            failedOccupancy.assign(
                static_cast<std::size_t>(bounds.columns) *
                    static_cast<std::size_t>(bounds.rows),
                0
            );
            for (std::size_t failed = 0;
                 failed < count;
                 ++failed) {
                const MotionBodyView& body = bodies[failed];
                if (body.present &&
                    body.role == MotionRole::Advancing &&
                    result.hardFailures[failed]) {
                    markCells(
                        failedOccupancy,
                        body.cells,
                        bounds
                    );
                }
            }
        }
        for (std::size_t moving = 0; moving < count; ++moving) {
            const MotionBodyView& body = bodies[moving];
            if (!body.present ||
                body.role != MotionRole::Advancing ||
                result.hardFailures[moving]) {
                continue;
            }
            if (!failedOccupancy.empty()) {
                if (overlapsOccupancy(
                        body.proposal,
                        failedOccupancy,
                        bounds
                    )) {
                    result.hardFailures[moving] = 1;
                    expanded = true;
                }
                continue;
            }
            for (std::size_t failed = 0; failed < count; ++failed) {
                const MotionBodyView& other = bodies[failed];
                if (failed == moving || !other.present ||
                    other.role != MotionRole::Advancing ||
                    !result.hardFailures[failed]) {
                    continue;
                }
                if (cellsOverlap(
                        body.proposal,
                        other.cells,
                        bounds
                    )) {
                    result.hardFailures[moving] = 1;
                    expanded = true;
                    break;
                }
            }
        }
    }

    if (!trackMovingContacts) {
        return result;
    }

    for (std::size_t first = 0; first < count; ++first) {
        const MotionBodyView& lhs = bodies[first];
        for (std::size_t second = first + 1;
             second < count;
             ++second) {
            const MotionBodyView& rhs = bodies[second];
            const std::size_t forward =
                pairIndex(first, second, count);
            const std::size_t reverse =
                pairIndex(second, first, count);
            const bool lhsMoving =
                lhs.role != MotionRole::Static;
            const bool rhsMoving =
                rhs.role != MotionRole::Static;
            if (!lhs.present || !rhs.present ||
                !lhsMoving || !rhsMoving ||
                result.hardFailures[first] ||
                result.hardFailures[second]) {
                result.movingContacts[forward] = 0;
                result.movingContacts[reverse] = 0;
                continue;
            }
            const bool evaluate =
                lhs.role == MotionRole::Advancing ||
                rhs.role == MotionRole::Advancing;
            if (!evaluate) {
                continue;
            }

            const bool headSwap =
                !lhs.proposal.empty() &&
                !rhs.proposal.empty() &&
                !lhs.cells.empty() &&
                !rhs.cells.empty() &&
                (cellIsRelevant(lhs.cells.front(), bounds) ||
                 cellIsRelevant(rhs.cells.front(), bounds) ||
                 cellIsRelevant(lhs.proposal.front(), bounds) ||
                 cellIsRelevant(rhs.proposal.front(), bounds)) &&
                lhs.proposal.front() == rhs.cells.front() &&
                rhs.proposal.front() == lhs.cells.front();
            const std::span<const Cell> lhsAfter =
                lhs.role == MotionRole::Advancing
                ? lhs.proposal
                : lhs.cells;
            const std::span<const Cell> rhsAfter =
                rhs.role == MotionRole::Advancing
                ? rhs.proposal
                : rhs.cells;
            const bool contact =
                headSwap ||
                cellsOverlap(lhsAfter, rhsAfter, bounds);
            if (!contact) {
                // Rear-end contacts alternate between two boundary events:
                // the leader advances, then the follower enters the cell it
                // vacated. Preserve an existing episode while both accepted
                // next states still overlap, but do not use this look-ahead
                // to report a collision before the bodies actually meet.
                const bool remainsFollowing =
                    hasPreviousMatrix &&
                    previousMovingContacts[forward] != 0 &&
                    !lhs.proposal.empty() &&
                    !rhs.proposal.empty() &&
                    cellsOverlap(
                        lhs.proposal,
                        rhs.proposal,
                        bounds
                    );
                if (!remainsFollowing) {
                    result.movingContacts[forward] = 0;
                    result.movingContacts[reverse] = 0;
                }
                continue;
            }

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
