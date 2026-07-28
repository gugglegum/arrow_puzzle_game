#include "arrow/Generator.hpp"
#include "arrow/MotionKinematics.hpp"
#include "arrow/MotionResolver.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Quality {
    std::size_t arrowCount{};
    std::size_t initiallyRemovable{};
    std::size_t directionCount{};
    std::size_t bentArrows{};
    std::size_t bentMediumAndLong{};
    std::size_t bentMedium{};
    std::size_t bentLong{};
    std::size_t longWithTwoTurns{};
    std::size_t straightArrows{};
    std::size_t shortArrows{};
    std::size_t mediumArrows{};
    std::size_t longArrows{};
    std::size_t veryLongArrows{};
    std::size_t firstQuartileLength{};
    std::size_t medianLength{};
    std::size_t thirdQuartileLength{};
    std::size_t turns{};
    std::size_t shortestArrow{std::numeric_limits<std::size_t>::max()};
    std::size_t longestArrow{};
    std::size_t maximumChoices{};
    std::size_t choiceExcess{};
    bool intendedOrderValid{true};
    std::size_t interArrowEdges{};
    std::size_t horizontalInterArrowEdges{};
    std::size_t verticalInterArrowEdges{};
    std::size_t scanlineExcessRuns{};
    std::size_t rectangularArrows{};
    std::size_t richMacroBlocks{};
    std::size_t tightSelfPassArrows{};
    std::size_t longComplexTightSelfPassArrows{};
    double medianBoundingFill{};
    double lengthCoefficientOfVariation{};
};

int minimumSelfPassClearance(
    const arrow::Arrow& value,
    int columns,
    int rows
) {
    std::vector<int> bodyPositions(
        static_cast<std::size_t>(columns * rows),
        -1
    );
    for (std::size_t position = 0;
         position < value.cells.size();
         ++position) {
        const arrow::Cell cell = value.cells[position];
        bodyPositions[static_cast<std::size_t>(
            cell.y * columns + cell.x
        )] = static_cast<int>(position);
    }

    int minimumClearance = std::numeric_limits<int>::max();
    arrow::Cell probe = value.cells.front() + value.direction;
    int distance = 1;
    while (
        probe.x >= 0 && probe.x < columns &&
        probe.y >= 0 && probe.y < rows
    ) {
        const int position = bodyPositions[static_cast<std::size_t>(
            probe.y * columns + probe.x
        )];
        if (position >= 0) {
            minimumClearance = std::min(
                minimumClearance,
                distance -
                    (static_cast<int>(value.cells.size()) - position)
            );
        }
        probe = probe + value.direction;
        ++distance;
    }
    return minimumClearance;
}

Quality analyze(const arrow::Board& board) {
    Quality result;
    result.arrowCount = board.arrows().size();
    std::vector<int> owners(
        static_cast<std::size_t>(board.columns() * board.rows()),
        -1
    );
    std::vector<std::size_t> turnsByArrow(result.arrowCount, 0);
    std::vector<int> boundingAreas(result.arrowCount, 0);
    std::vector<double> longArrowBoundingFills;
    std::vector<std::size_t> lengths;
    lengths.reserve(result.arrowCount);
    int directionMask = 0;

    for (std::size_t index = 0; index < result.arrowCount; ++index) {
        const arrow::Arrow& value = board.arrows()[index];
        if (value.direction == arrow::Cell{-1, 0}) {
            directionMask |= 1;
        } else if (value.direction == arrow::Cell{1, 0}) {
            directionMask |= 2;
        } else if (value.direction == arrow::Cell{0, -1}) {
            directionMask |= 4;
        } else if (value.direction == arrow::Cell{0, 1}) {
            directionMask |= 8;
        }

        bool isBent = false;
        std::size_t arrowTurns = 0;
        int minimumX = board.columns();
        int maximumX = -1;
        int minimumY = board.rows();
        int maximumY = -1;
        for (std::size_t cell = 2; cell < value.cells.size(); ++cell) {
            const arrow::Cell before =
                value.cells[cell - 1] - value.cells[cell - 2];
            const arrow::Cell after =
                value.cells[cell] - value.cells[cell - 1];
            if (before != after) {
                isBent = true;
                ++result.turns;
                ++arrowTurns;
            }
        }
        for (arrow::Cell cell : value.cells) {
            owners[static_cast<std::size_t>(
                cell.y * board.columns() + cell.x
            )] = static_cast<int>(index);
            minimumX = std::min(minimumX, cell.x);
            maximumX = std::max(maximumX, cell.x);
            minimumY = std::min(minimumY, cell.y);
            maximumY = std::max(maximumY, cell.y);
        }
        const int boundingArea =
            (maximumX - minimumX + 1) * (maximumY - minimumY + 1);
        boundingAreas[index] = boundingArea;
        turnsByArrow[index] = arrowTurns;
        const double boundingFill =
            static_cast<double>(value.cells.size()) /
            static_cast<double>(boundingArea);
        if (value.cells.size() <= 5) {
            ++result.shortArrows;
        } else if (value.cells.size() <= 12) {
            ++result.mediumArrows;
            result.bentMediumAndLong += isBent ? 1U : 0U;
            result.bentMedium += isBent ? 1U : 0U;
        } else {
            ++result.longArrows;
            result.bentMediumAndLong += isBent ? 1U : 0U;
            result.bentLong += isBent ? 1U : 0U;
            result.longWithTwoTurns += arrowTurns >= 2 ? 1U : 0U;
            result.veryLongArrows += value.cells.size() >= 18 ? 1U : 0U;
            longArrowBoundingFills.push_back(boundingFill);
            result.rectangularArrows += boundingFill > 0.9 ? 1U : 0U;
        }
        result.bentArrows += isBent ? 1U : 0U;
        result.straightArrows += isBent ? 0U : 1U;
        lengths.push_back(value.cells.size());
        result.shortestArrow = std::min(
            result.shortestArrow,
            value.cells.size()
        );
        result.longestArrow = std::max(
            result.longestArrow,
            value.cells.size()
        );
    }
    result.directionCount = static_cast<std::size_t>(
        std::popcount(static_cast<unsigned int>(directionMask))
    );
    if (!longArrowBoundingFills.empty()) {
        std::ranges::sort(longArrowBoundingFills);
        result.medianBoundingFill = longArrowBoundingFills[
            longArrowBoundingFills.size() / 2
        ];
    }
    std::ranges::sort(lengths);
    result.firstQuartileLength = lengths[lengths.size() / 4];
    result.medianLength = lengths[lengths.size() / 2];
    result.thirdQuartileLength = lengths[lengths.size() * 3 / 4];
    const double meanLength = static_cast<double>(
        std::accumulate(lengths.begin(), lengths.end(), std::size_t{0})
    ) / static_cast<double>(lengths.size());
    double squaredDifference = 0.0;
    for (std::size_t length : lengths) {
        const double difference =
            static_cast<double>(length) - meanLength;
        squaredDifference += difference * difference;
    }
    result.lengthCoefficientOfVariation =
        std::sqrt(squaredDifference / static_cast<double>(lengths.size())) /
        meanLength;

    for (std::size_t index = 0; index < result.arrowCount; ++index) {
        const arrow::Arrow& value = board.arrows()[index];
        const int minimumClearance = minimumSelfPassClearance(
            value,
            board.columns(),
            board.rows()
        );
        if (minimumClearance != 0) {
            continue;
        }

        ++result.tightSelfPassArrows;
        const int length = static_cast<int>(value.cells.size());
        const int boundingArea = boundingAreas[index];
        const bool rectangularThread =
            length > 12 && length * 10 > boundingArea * 9;
        const bool complexThread =
            turnsByArrow[index] >= 2 &&
            boundingArea - length >= 3;
        if (!rectangularThread && length >= 18 && complexThread) {
            ++result.longComplexTightSelfPassArrows;
        }
    }

    for (int y = 0; y < board.rows(); ++y) {
        for (int x = 0; x < board.columns(); ++x) {
            const int owner = owners[static_cast<std::size_t>(
                y * board.columns() + x
            )];
            if (x + 1 < board.columns()) {
                const int other = owners[static_cast<std::size_t>(
                    y * board.columns() + x + 1
                )];
                if (owner != other) {
                    ++result.interArrowEdges;
                    ++result.horizontalInterArrowEdges;
                }
            }
            if (y + 1 < board.rows()) {
                const int other = owners[static_cast<std::size_t>(
                    (y + 1) * board.columns() + x
                )];
                if (owner != other) {
                    ++result.interArrowEdges;
                    ++result.verticalInterArrowEdges;
                }
            }
        }
    }

    const auto countRuns = [&](bool rowsFirst) {
        const int outer = rowsFirst ? board.rows() : board.columns();
        const int inner = rowsFirst ? board.columns() : board.rows();
        for (int line = 0; line < outer; ++line) {
            std::vector<int> runs(result.arrowCount, 0);
            int previous = -1;
            for (int position = 0; position < inner; ++position) {
                const int x = rowsFirst ? position : line;
                const int y = rowsFirst ? line : position;
                const int owner = owners[static_cast<std::size_t>(
                    y * board.columns() + x
                )];
                if (owner != previous) {
                    ++runs[static_cast<std::size_t>(owner)];
                    previous = owner;
                }
            }
            for (int count : runs) {
                result.scanlineExcessRuns +=
                    static_cast<std::size_t>(std::max(0, count - 1));
            }
        }
    };
    countRuns(true);
    countRuns(false);

    if (board.columns() == 20 && board.rows() == 20) {
        for (int blockY = 0; blockY < 4; ++blockY) {
            for (int blockX = 0; blockX < 4; ++blockX) {
                std::vector<bool> seen(result.arrowCount, false);
                std::size_t distinct = 0;
                for (int y = blockY * 5; y < blockY * 5 + 5; ++y) {
                    for (int x = blockX * 5; x < blockX * 5 + 5; ++x) {
                        const int owner = owners[static_cast<std::size_t>(
                            y * board.columns() + x
                        )];
                        if (!seen[static_cast<std::size_t>(owner)]) {
                            seen[static_cast<std::size_t>(owner)] = true;
                            ++distinct;
                        }
                    }
                }
                result.richMacroBlocks += distinct >= 4 ? 1U : 0U;
            }
        }
    }

    const std::size_t blockerWordCount =
        (result.arrowCount + 63U) / 64U;
    std::vector<std::vector<std::uint64_t>> blockers(
        result.arrowCount,
        std::vector<std::uint64_t>(blockerWordCount, 0U)
    );
    for (std::size_t index = 0; index < result.arrowCount; ++index) {
        arrow::Cell probe =
            board.arrows()[index].cells.front() +
            board.arrows()[index].direction;
        while (board.isInside(probe)) {
            const std::size_t owner = static_cast<std::size_t>(
                owners[static_cast<std::size_t>(
                    probe.y * board.columns() + probe.x
                )]
            );
            if (owner != index) {
                blockers[index][owner / 64U] |=
                    std::uint64_t{1} << (owner % 64U);
            }
            probe = probe + board.arrows()[index].direction;
        }
    }
    std::vector<std::uint64_t> presentMask(
        blockerWordCount,
        ~std::uint64_t{0}
    );
    const auto isBlocked = [&](
        std::size_t index
    ) {
        for (std::size_t word = 0;
             word < blockerWordCount;
             ++word) {
            if ((blockers[index][word] & presentMask[word]) != 0U) {
                return true;
            }
        }
        return false;
    };

    for (std::size_t step = 0; step < result.arrowCount; ++step) {
        std::size_t choices = 0;
        for (std::size_t index = 0; index < result.arrowCount; ++index) {
            const bool isPresent =
                (presentMask[index / 64U] &
                 (std::uint64_t{1} << (index % 64U))) != 0U;
            choices += isPresent && !isBlocked(index) ? 1U : 0U;
        }
        result.intendedOrderValid =
            result.intendedOrderValid && !isBlocked(step);
        result.maximumChoices = std::max(result.maximumChoices, choices);
        result.choiceExcess += choices > 0 ? choices - 1 : 0;
        if (step == 0) {
            result.initiallyRemovable = choices;
        }
        presentMask[step / 64U] &=
            ~(std::uint64_t{1} << (step % 64U));
    }
    return result;
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct OwnedMotionBody {
    std::vector<arrow::Cell> cells;
    std::vector<arrow::Cell> proposal;
    std::vector<arrow::Cell> reservedTarget;
    bool present{true};
    arrow::MotionRole role{arrow::MotionRole::Advancing};
    bool selfBlocked{};
};

arrow::MotionResolution resolveOwnedMotion(
    const std::vector<OwnedMotionBody>& bodies,
    const std::vector<std::uint8_t>& previousContacts = {},
    arrow::MotionBounds bounds = {}
) {
    std::vector<arrow::MotionBodyView> views;
    views.reserve(bodies.size());
    for (const OwnedMotionBody& body : bodies) {
        views.push_back({
            body.cells,
            body.proposal,
            body.reservedTarget,
            body.present,
            body.role,
            body.selfBlocked
        });
    }
    return arrow::resolveMotionStep(
        views,
        previousContacts,
        bounds
    );
}

void verifyMotionResolution() {
    const OwnedMotionBody first{
        {{0, 0}, {-1, 0}},
        {{1, 0}, {0, 0}}
    };
    const OwnedMotionBody second{
        {{2, 0}, {3, 0}},
        {{1, 0}, {2, 0}}
    };
    const std::vector softBodies{first, second};
    const arrow::MotionResolution firstContact =
        resolveOwnedMotion(softBodies);
    require(
        firstContact.hardFailures == std::vector<std::uint8_t>({0, 0}),
        "moving arrows incorrectly became hard failures"
    );
    require(
        firstContact.hasNewMovingContact,
        "first moving-arrow contact was not reported"
    );
    require(
        firstContact.movingContacts[1] != 0 &&
            firstContact.movingContacts[2] != 0,
        "moving-arrow contact matrix is not symmetric"
    );

    const arrow::MotionResolution continuedContact =
        resolveOwnedMotion(
            softBodies,
            firstContact.movingContacts
        );
    require(
        !continuedContact.hasNewMovingContact,
        "continuous moving overlap charged a second event"
    );

    OwnedMotionBody rearArrow{
        {{0, 0}, {-1, 0}},
        {{1, 0}, {0, 0}}
    };
    OwnedMotionBody leadingArrow{
        {{2, 0}, {1, 0}},
        {{3, 0}, {2, 0}}
    };
    leadingArrow.role = arrow::MotionRole::Moving;
    const arrow::MotionResolution rearEndContact =
        resolveOwnedMotion({rearArrow, leadingArrow});
    require(
        rearEndContact.hasNewMovingContact,
        "rear-end moving-arrow contact was not reported"
    );

    rearArrow.cells = {{1, 0}, {0, 0}};
    rearArrow.proposal = {{2, 0}, {1, 0}};
    rearArrow.role = arrow::MotionRole::Moving;
    leadingArrow.role = arrow::MotionRole::Advancing;
    const arrow::MotionResolution alternatingGap =
        resolveOwnedMotion(
            {rearArrow, leadingArrow},
            rearEndContact.movingContacts
        );
    require(
        !alternatingGap.hasNewMovingContact &&
            alternatingGap.movingContacts ==
                rearEndContact.movingContacts,
        "rear-end contact latch was lost between cell events"
    );

    rearArrow.role = arrow::MotionRole::Advancing;
    leadingArrow.cells = {{3, 0}, {2, 0}};
    leadingArrow.proposal = {{4, 0}, {3, 0}};
    leadingArrow.role = arrow::MotionRole::Moving;
    const arrow::MotionResolution repeatedRearOverlap =
        resolveOwnedMotion(
            {rearArrow, leadingArrow},
            alternatingGap.movingContacts
        );
    require(
        !repeatedRearOverlap.hasNewMovingContact &&
            repeatedRearOverlap.movingContacts ==
                rearEndContact.movingContacts,
        "one rear-end collision was charged more than once"
    );

    std::vector<OwnedMotionBody> fractionalContactBodies =
        softBodies;
    for (OwnedMotionBody& body : fractionalContactBodies) {
        body.role = arrow::MotionRole::Moving;
    }
    const arrow::MotionResolution fractionalContact =
        resolveOwnedMotion(
            fractionalContactBodies,
            firstContact.movingContacts
        );
    require(
        !fractionalContact.hasNewMovingContact &&
            fractionalContact.movingContacts ==
                firstContact.movingContacts,
        "fractional phase lost or repeated an active contact"
    );

    const std::vector clearBodies{
        OwnedMotionBody{
            {{0, 0}, {-1, 0}},
            {{-1, 0}, {0, 0}}
        },
        OwnedMotionBody{
            {{4, 0}, {5, 0}},
            {{5, 0}, {4, 0}}
        }
    };
    const arrow::MotionResolution separatedContact =
        resolveOwnedMotion(
            clearBodies,
            continuedContact.movingContacts
        );
    require(
        std::ranges::none_of(
            separatedContact.movingContacts,
            [](std::uint8_t value) { return value != 0; }
        ),
        "separated moving arrows kept a stale contact"
    );
    require(
        resolveOwnedMotion(
            softBodies,
            separatedContact.movingContacts
        ).hasNewMovingContact,
        "moving-arrow recontact was not reported"
    );

    const arrow::MotionResolution headSwap = resolveOwnedMotion({
        OwnedMotionBody{{{0, 0}}, {{1, 0}}},
        OwnedMotionBody{{{1, 0}}, {{0, 0}}}
    });
    require(
        headSwap.hardFailures ==
            std::vector<std::uint8_t>({0, 0}) &&
            headSwap.hasNewMovingContact,
        "opposite moving heads could not pass through each other"
    );

    const OwnedMotionBody stationary{
        {{1, 0}, {1, 1}},
        {},
        {},
        true,
        arrow::MotionRole::Static,
        false
    };
    const arrow::MotionResolution hardHit =
        resolveOwnedMotion({first, stationary});
    require(
        hardHit.hardFailures == std::vector<std::uint8_t>({1, 0}),
        "stationary obstacle was not a hard failure"
    );
    require(
        !hardHit.hasNewMovingContact,
        "stationary obstacle was reported as a moving contact"
    );

    const OwnedMotionBody reservedObstacle{
        {{3, 0}},
        {},
        {{2, 0}},
        true,
        arrow::MotionRole::Static,
        false
    };
    require(
        resolveOwnedMotion({
            OwnedMotionBody{{{1, 0}}, {{2, 0}}},
            reservedObstacle
        }).hardFailures[0] != 0,
        "stationary reserved target did not block a transition"
    );

    const OwnedMotionBody cascadeFirst{
        {{2, 0}, {1, 0}},
        {{3, 0}, {2, 0}}
    };
    const OwnedMotionBody cascadeFollower{
        {{0, 0}, {-1, 0}},
        {{1, 0}, {0, 0}}
    };
    const OwnedMotionBody cascadeObstacle{
        {{3, 0}, {3, 1}},
        {},
        {},
        true,
        arrow::MotionRole::Static,
        false
    };
    const arrow::MotionResolution cascade = resolveOwnedMotion({
        cascadeFirst,
        cascadeFollower,
        cascadeObstacle
    });
    require(
        cascade.hardFailures ==
            std::vector<std::uint8_t>({1, 1, 0}),
        "unreleased moving tail did not propagate a hard failure"
    );

    const OwnedMotionBody cascadeThird{
        {{-2, 0}, {-3, 0}},
        {{-1, 0}, {-2, 0}}
    };
    const arrow::MotionResolution transitiveCascade =
        resolveOwnedMotion({
            cascadeFirst,
            cascadeFollower,
            cascadeThird,
            cascadeObstacle
        });
    require(
        transitiveCascade.hardFailures ==
            std::vector<std::uint8_t>({1, 1, 1, 0}),
        "hard failure did not propagate through three moving arrows"
    );

    OwnedMotionBody selfBlocked = first;
    selfBlocked.selfBlocked = true;
    require(
        resolveOwnedMotion({selfBlocked}).hardFailures[0] != 0,
        "self-collision was not treated as a hard failure"
    );

    OwnedMotionBody fractionalMover = selfBlocked;
    fractionalMover.role = arrow::MotionRole::Moving;
    require(
        resolveOwnedMotion({fractionalMover}).hardFailures[0] == 0,
        "fractional mover was incorrectly resolved a second time"
    );

    OwnedMotionBody movingObstacle{
        {{1, 0}},
        {{2, 0}}
    };
    movingObstacle.role = arrow::MotionRole::Moving;
    const arrow::MotionResolution asynchronousContact =
        resolveOwnedMotion({
            OwnedMotionBody{{{0, 0}}, {{1, 0}}},
            movingObstacle
        });
    require(
        asynchronousContact.hardFailures ==
            std::vector<std::uint8_t>({0, 0}) &&
            asynchronousContact.hasNewMovingContact,
        "fractional moving arrow was treated as a hard obstacle"
    );

    const OwnedMotionBody staleTargetAttempt{
        {{2, 1}},
        {{2, 0}}
    };
    const OwnedMotionBody crossingMover{
        {{1, 0}},
        {{2, 0}},
        {},
        true,
        arrow::MotionRole::Moving,
        false
    };
    require(
        resolveOwnedMotion({
            staleTargetAttempt,
            crossingMover
        }).hardFailures[0] == 0,
        "moving arrow prematurely blocked an accepted transition"
    );
    const OwnedMotionBody newlyStaticObstacle{
        {{2, 0}},
        {},
        {},
        true,
        arrow::MotionRole::Static,
        false
    };
    require(
        resolveOwnedMotion({
            staleTargetAttempt,
            newlyStaticObstacle
        }).hardFailures[0] != 0,
        "commit-time revalidation missed a newly static obstacle"
    );

    const OwnedMotionBody outsideObstacle{
        {{5, 0}},
        {},
        {},
        true,
        arrow::MotionRole::Static,
        false
    };
    const OwnedMotionBody leavingBoard{
        {{4, 0}},
        {{5, 0}}
    };
    require(
        resolveOwnedMotion(
            {leavingBoard, outsideObstacle},
            {},
            {5, 5}
        ).hardFailures[0] == 0,
        "collision entirely outside the board blocked an exit"
    );
    const OwnedMotionBody outsideReservedObstacle{
        {{4, 1}},
        {},
        {{5, 0}},
        true,
        arrow::MotionRole::Static,
        false
    };
    require(
        resolveOwnedMotion(
            {leavingBoard, outsideReservedObstacle},
            {},
            {5, 5}
        ).hardFailures[0] == 0,
        "reserved target outside the board blocked an exit"
    );

    const arrow::MotionResolution outsideHeadSwap =
        resolveOwnedMotion(
            {
                OwnedMotionBody{{{5, 0}}, {{6, 0}}},
                OwnedMotionBody{{{6, 0}}, {{5, 0}}}
            },
            {},
            {5, 5}
        );
    require(
        !outsideHeadSwap.hasNewMovingContact,
        "head swap outside the board cost a life"
    );

    const std::vector boundarySwapBodies{
        OwnedMotionBody{{{4, 0}}, {{5, 0}}},
        OwnedMotionBody{{{5, 0}}, {{4, 0}}}
    };
    const arrow::MotionResolution boundaryHeadSwap =
        resolveOwnedMotion(
            boundarySwapBodies,
            {},
            {5, 5}
        );
    const arrow::MotionResolution reversedBoundaryHeadSwap =
        resolveOwnedMotion(
            {
                boundarySwapBodies[1],
                boundarySwapBodies[0]
            },
            {},
            {5, 5}
        );
    require(
        boundaryHeadSwap.hasNewMovingContact &&
            reversedBoundaryHeadSwap.hasNewMovingContact,
        "boundary head swap depends on body ordering"
    );
}

void verifyMotionKinematics() {
    constexpr double acceleration = 22.0;
    constexpr double initialSpeed = 8.0;
    constexpr double firstPart = 0.017;
    constexpr double secondPart = 0.033;
    const double wholeDistance = arrow::acceleratedDistance(
        initialSpeed,
        acceleration,
        firstPart + secondPart
    );
    const double splitDistance =
        arrow::acceleratedDistance(
            initialSpeed,
            acceleration,
            firstPart
        ) +
        arrow::acceleratedDistance(
            initialSpeed + acceleration * firstPart,
            acceleration,
            secondPart
        );
    require(
        std::abs(wholeDistance - splitDistance) < 1.0e-12,
        "accelerated motion depends on frame partitioning"
    );

    const double targetDistance = 37.25;
    const double travelTime = arrow::timeToCoverDistance(
        targetDistance,
        140.0,
        acceleration
    );
    require(
        std::abs(
            arrow::acceleratedDistance(
                140.0,
                acceleration,
                travelTime
            ) -
            targetDistance
        ) < 1.0e-10,
        "time-to-boundary inversion is inaccurate at high speed"
    );

    double progress = 0.2;
    double speed = 10.0;
    double remainingTime = 0.35;
    int crossings = 0;
    while (remainingTime > 1.0e-12) {
        const double boundaryTime =
            arrow::timeToCoverDistance(
                1.0 - progress,
                speed,
                0.0
            );
        const double step = std::min(
            boundaryTime,
            remainingTime
        );
        progress += arrow::acceleratedDistance(
            speed,
            0.0,
            step
        );
        remainingTime -= step;
        if (boundaryTime <= step + 1.0e-12) {
            progress = 0.0;
            ++crossings;
        }
    }
    require(
        crossings == 3 &&
            std::abs(progress - 0.7) < 1.0e-12,
        "multiple cell boundaries were skipped in one frame"
    );

    struct TestMover {
        double progress{};
        double speed{};
        int crossings{};
    };
    const auto advanceMover =
        [acceleration](TestMover& mover, double time) {
            constexpr double epsilon = 1.0e-12;
            while (time > epsilon) {
                const double boundaryTime =
                    arrow::timeToCoverDistance(
                        1.0 - mover.progress,
                        mover.speed,
                        acceleration
                    );
                const double step = std::min(
                    boundaryTime,
                    time
                );
                mover.progress +=
                    arrow::acceleratedDistance(
                        mover.speed,
                        acceleration,
                        step
                    );
                mover.speed += acceleration * step;
                time -= step;
                if (boundaryTime <= step + epsilon) {
                    mover.progress = 0.0;
                    ++mover.crossings;
                }
            }
        };

    TestMover earlierArrow{0.0, initialSpeed, 0};
    advanceMover(earlierArrow, 0.2);
    TestMover laterArrow{0.0, initialSpeed, 0};
    advanceMover(earlierArrow, 0.08);
    advanceMover(laterArrow, 0.08);
    require(
        earlierArrow.crossings == 3 &&
            std::abs(earlierArrow.progress - 0.1024) <
                1.0e-10 &&
            std::abs(earlierArrow.speed - 14.16) <
                1.0e-10 &&
            laterArrow.crossings == 0 &&
            std::abs(laterArrow.progress - 0.7104) <
                1.0e-10 &&
            std::abs(laterArrow.speed - 9.76) <
                1.0e-10,
        "later launch inherited another arrow's speed or progress"
    );

    const double maximumFiniteSpeed =
        std::numeric_limits<double>::max();
    const double maximumSpeedBoundaryTime =
        arrow::timeToCoverDistance(
            1.0,
            maximumFiniteSpeed,
            acceleration
        );
    require(
        maximumSpeedBoundaryTime > 0.0 &&
            std::isfinite(maximumSpeedBoundaryTime) &&
            std::abs(
                maximumFiniteSpeed *
                    maximumSpeedBoundaryTime -
                1.0
            ) < 1.0e-12 &&
        arrow::acceleratedDistance(
            std::numeric_limits<double>::infinity(),
            acceleration,
            0.0
        ) == 0.0 &&
            arrow::timeToCoverDistance(
                1.0,
                std::numeric_limits<double>::infinity(),
                acceleration
            ) == 0.0,
        "uncapped speed produced an invalid zero-time boundary event"
    );
    require(
        arrow::eventTimesCoincide(
            maximumSpeedBoundaryTime,
            std::nextafter(
                maximumSpeedBoundaryTime,
                std::numeric_limits<double>::infinity()
            )
        ),
        "adjacent subnormal boundary times were not simultaneous"
    );

    const double extremelyFast =
        arrow::timeToCoverDistance(
            1.0,
            1.0e12,
            acceleration
        );
    const double merelyFast =
        arrow::timeToCoverDistance(
            1.0,
            1.0e8,
            acceleration
        );
    require(
        !arrow::eventTimesCoincide(
            extremelyFast,
            merelyFast
        ),
        "different high-speed boundaries were grouped as simultaneous"
    );
    require(
        arrow::eventTimesCoincide(
            extremelyFast,
            std::nextafter(
                extremelyFast,
                std::numeric_limits<double>::infinity()
            )
        ),
        "one-ULP boundary difference was not grouped as simultaneous"
    );

    const auto nextUlps =
        [](double value, int count) {
            for (int step = 0; step < count; ++step) {
                value = std::nextafter(
                    value,
                    std::numeric_limits<double>::infinity()
                );
            }
            return value;
        };
    require(
        arrow::eventTimesCoincide(
            1.0,
            nextUlps(1.0, 64)
        ) &&
            !arrow::eventTimesCoincide(
                1.0,
                nextUlps(1.0, 65)
            ),
        "normal event-time ULP threshold is inconsistent"
    );
    const double smallestTime =
        std::numeric_limits<double>::denorm_min();
    require(
        arrow::eventTimesCoincide(
            smallestTime,
            nextUlps(smallestTime, 64)
        ) &&
            !arrow::eventTimesCoincide(
                smallestTime,
                nextUlps(smallestTime, 65)
            ),
        "subnormal event-time ULP threshold is inconsistent"
    );
    require(
        !arrow::eventTimesCoincide(
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()
        ) &&
            !arrow::eventTimesCoincide(
                std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()
            ),
        "non-finite event times were grouped as simultaneous"
    );

    constexpr double priorDebt = 0.4;
    constexpr double currentFrame = 0.1;
    double lateLaunchDelay = priorDebt;
    TestMover debtProtectedArrow{0.0, initialSpeed, 0};
    const double preLaunchTime = std::min(
        priorDebt + currentFrame,
        lateLaunchDelay
    );
    lateLaunchDelay -= preLaunchTime;
    advanceMover(
        debtProtectedArrow,
        priorDebt + currentFrame - preLaunchTime
    );
    require(
        lateLaunchDelay == 0.0 &&
            debtProtectedArrow.crossings == 0 &&
            std::abs(
                debtProtectedArrow.progress - 0.91
            ) < 1.0e-12 &&
            std::abs(
                debtProtectedArrow.speed - 10.2
            ) < 1.0e-12,
        "late launch consumed simulation debt from before its launch"
    );
}

void verifyHighSpeedCollisionSweep() {
    std::vector<arrow::Cell> cells{{0, 0}};
    const std::vector<arrow::Cell> obstacle{{7, 0}};
    constexpr arrow::Cell direction{1, 0};
    double speed = 200.0;
    double progress = 0.0;
    double remainingTime = 0.05;
    int committedCells = 0;
    bool hardCollision = false;

    while (remainingTime > 1.0e-12) {
        std::vector<arrow::Cell> proposal = cells;
        proposal.front() = proposal.front() + direction;
        const arrow::MotionResolution resolution =
            resolveOwnedMotion(
                {
                    OwnedMotionBody{cells, proposal},
                    OwnedMotionBody{
                        obstacle,
                        {},
                        {},
                        true,
                        arrow::MotionRole::Static,
                        false
                    }
                },
                {},
                {20, 1}
            );
        if (resolution.hardFailures[0]) {
            hardCollision = true;
            break;
        }

        const double boundaryTime =
            arrow::timeToCoverDistance(
                1.0 - progress,
                speed,
                0.0
            );
        if (boundaryTime > remainingTime) {
            progress += arrow::acceleratedDistance(
                speed,
                0.0,
                remainingTime
            );
            remainingTime = 0.0;
            break;
        }

        remainingTime -= boundaryTime;
        cells = std::move(proposal);
        progress = 0.0;
        ++committedCells;
    }

    require(
        hardCollision &&
            committedCells == 6 &&
            cells.front() == arrow::Cell{6, 0},
        "high-speed arrow tunnelled through a cell obstacle"
    );
}

void verifyDynamicSelfCollision() {
    const arrow::Arrow clearsTailInTime{
        {
            {2, 1},
            {1, 1},
            {1, 2},
            {2, 2},
            {3, 2},
            {3, 1}
        },
        {1, 0}
    };
    const arrow::Board safeBoard(5, 4, {clearsTailInTime});
    require(
        safeBoard.canExit(0, {true}),
        "head cannot enter a cell vacated by its tail on the same tick"
    );
    require(
        minimumSelfPassClearance(clearsTailInTime, 5, 4) == 0,
        "exact tail pass did not report zero-cell clearance"
    );

    const arrow::Arrow clearsSeveralCellsInTime{
        {
            {1, 1},
            {0, 1},
            {0, 2},
            {1, 2},
            {2, 2},
            {3, 2},
            {3, 1},
            {4, 1}
        },
        {1, 0}
    };
    const arrow::Board multipleSafeCrossings(
        5,
        3,
        {clearsSeveralCellsInTime}
    );
    require(
        multipleSafeCrossings.canExit(0, {true}),
        "later body cells were not released at their exact safe ticks"
    );
    require(
        minimumSelfPassClearance(clearsSeveralCellsInTime, 5, 3) == 0,
        "multi-contact exact tail pass did not report zero clearance"
    );

    const arrow::Arrow hitsOwnBody{
        {
            {2, 1},
            {1, 1},
            {1, 2},
            {2, 2},
            {3, 2},
            {3, 1},
            {4, 1},
            {4, 2}
        },
        {1, 0}
    };
    const arrow::Board collidingBoard(5, 4, {hitsOwnBody});
    require(
        !collidingBoard.canExit(0, {true}),
        "head incorrectly passes through its own body"
    );
    require(
        minimumSelfPassClearance(hitsOwnBody, 5, 4) < 0,
        "unsafe self-hit did not report negative clearance"
    );

    const arrow::Arrow noSelfPass{
        {{1, 1}, {0, 1}},
        {1, 0}
    };
    require(
        minimumSelfPassClearance(noSelfPass, 5, 4) ==
            std::numeric_limits<int>::max(),
        "arrow without a body crossing reported a finite clearance"
    );

    const arrow::Arrow delayedSelfHit{
        {
            {1, 1},
            {0, 1},
            {0, 2},
            {1, 2},
            {2, 2},
            {3, 2},
            {3, 1},
            {4, 1},
            {4, 2}
        },
        {1, 0}
    };
    const arrow::Board delayedCollisionBoard(6, 4, {delayedSelfHit});
    require(
        !delayedCollisionBoard.canExit(0, {true}),
        "self-collision after a free first step was not detected"
    );

    const arrow::Arrow shortCandidate{{{1, 1}, {0, 1}}, {1, 0}};
    const arrow::Arrow stationaryObstacle{{{2, 2}, {2, 1}}, {0, 1}};
    const arrow::Board otherTailBoard(
        4,
        4,
        {shortCandidate, stationaryObstacle}
    );
    require(
        !otherTailBoard.canExit(0, {true, true}),
        "tail of a stationary other arrow was incorrectly vacated"
    );
    require(
        otherTailBoard.canExit(0, {true, false}),
        "removed obstacle still blocks the candidate arrow"
    );
}

Quality verifyBoard(
    const arrow::Board& board,
    const arrow::GeneratorOptions& options
) {
    require(board.isCompletelyFilled(), "board is not completely filled");
    require(board.hasValidGeometry(), "arrow geometry is invalid");
    require(!board.arrows().empty(), "board has no arrows");

    const std::vector<std::size_t> solution = board.solveGreedily();
    require(
        solution.size() == board.arrows().size(),
        "greedy solver did not clear the board"
    );

    const std::size_t occupied = std::accumulate(
        board.arrows().begin(),
        board.arrows().end(),
        std::size_t{0},
        [](std::size_t total, const arrow::Arrow& value) {
            return total + value.cells.size();
        }
    );
    require(
        occupied == static_cast<std::size_t>(options.columns * options.rows),
        "occupied cell count does not match board area"
    );
    require(
        std::ranges::none_of(board.arrows(), [](const arrow::Arrow& value) {
            return value.cells.size() < 2;
        }),
        "generator created a one-cell arrow"
    );

    Quality quality = analyze(board);
    const std::size_t effectiveMinimumLength =
        static_cast<std::size_t>(std::clamp(
            options.minimumLength,
            2,
            options.columns * options.rows
        ));
    const std::size_t effectiveMaximumLength =
        static_cast<std::size_t>(std::clamp(
            std::max(
                static_cast<int>(effectiveMinimumLength),
                options.maximumLength
            ),
            static_cast<int>(effectiveMinimumLength),
            options.columns * options.rows
        ));
    require(
        quality.shortestArrow >= effectiveMinimumLength &&
            quality.longestArrow <= effectiveMaximumLength,
        "generator ignored the requested arrow length range"
    );
    if (options.columns == 20 && options.rows == 20) {
        require(
            quality.initiallyRemovable == 1,
            "default board does not have exactly one initial move"
        );
        require(
            quality.intendedOrderValid,
            "constructed removal order is invalid"
        );
        require(
            quality.directionCount >= 3,
            "default board uses fewer than three directions"
        );
        require(
            quality.arrowCount >= 48 && quality.arrowCount <= 54,
            "default board does not contain 48..54 arrows"
        );
        require(
            quality.shortArrows * 100 >= quality.arrowCount * 32,
            "default board has too few short arrows"
        );
        require(
            quality.mediumArrows * 100 >= quality.arrowCount * 34,
            "default board has too few medium arrows: " +
                std::to_string(quality.mediumArrows) + "/" +
                std::to_string(quality.arrowCount)
        );
        require(
            quality.longArrows * 100 >= quality.arrowCount * 10 &&
                quality.longArrows * 100 <= quality.arrowCount * 24,
            "default board long-arrow share is outside 10..24%: " +
                std::to_string(quality.longArrows) + "/" +
                std::to_string(quality.arrowCount)
        );
        require(
            quality.veryLongArrows >= 2,
            "default board has too few arrows of at least 18 cells"
        );
        require(
            quality.bentMedium * 5 >= quality.mediumArrows * 3,
            "too many medium arrows have no bend"
        );
        require(
            quality.bentLong * 5 >= quality.longArrows * 4,
            "too many long arrows have no bend"
        );
        require(
            quality.longWithTwoTurns * 5 >= quality.longArrows * 3,
            "too many long arrows have fewer than two turns"
        );
        require(
            quality.firstQuartileLength <= 5 &&
                quality.medianLength >= 6 &&
                quality.medianLength <= 9 &&
                quality.thirdQuartileLength >= 8 &&
                quality.thirdQuartileLength <= 12,
            "default board length quartiles are not sufficiently mixed: " +
                std::to_string(quality.firstQuartileLength) + "/" +
                std::to_string(quality.medianLength) + "/" +
                std::to_string(quality.thirdQuartileLength)
        );
        require(
            quality.thirdQuartileLength -
                    quality.firstQuartileLength >=
                3,
            "default board length interquartile range is too small"
        );
        require(
            quality.lengthCoefficientOfVariation >= 0.40,
            "default board arrow lengths are too uniform"
        );
        require(
            quality.turns >= 55 && quality.turns <= 160,
            "default board has too few turns"
        );
        require(
            quality.interArrowEdges >= 300 &&
                quality.horizontalInterArrowEdges * 100 >=
                    quality.interArrowEdges * 35 &&
                quality.verticalInterArrowEdges * 100 >=
                    quality.interArrowEdges * 35,
            "default board paths are not sufficiently interwoven"
        );
        require(
            quality.medianBoundingFill <= 0.70,
            "default board paths are too close to filled rectangles"
        );
        require(
            quality.rectangularArrows == 0,
            "default board contains a rectangular S-shaped snake"
        );
        require(
            quality.richMacroBlocks >= 12,
            "too few 5x5 regions contain four or more arrow threads"
        );
        require(
            quality.maximumChoices <= 10,
            "default board exposes too many simultaneous choices"
        );
        require(
            quality.choiceExcess <= board.arrows().size() * 6,
            "default board dependency graph branches too widely"
        );
        require(
            quality.shortestArrow >=
                static_cast<std::size_t>(options.minimumLength),
            "default board has an arrow below the requested minimum"
        );
        require(
            quality.shortestArrow <= 3,
            "default board does not contain a genuinely short arrow"
        );
        require(
            quality.longestArrow >= 18,
            "default board does not contain a genuinely long arrow"
        );
        require(
            quality.longestArrow <=
                static_cast<std::size_t>(options.maximumLength),
            "default board has an arrow above the requested maximum"
        );
    }
    return quality;
}

} // namespace

int main(int argc, char** argv) {
    try {
        verifyMotionKinematics();
        verifyMotionResolution();
        verifyHighSpeedCollisionSweep();
        verifyDynamicSelfCollision();
        require(
            arrow::GeneratorOptions{}.candidateAttempts == 8,
            "default generator quality no longer evaluates eight candidates"
        );

        bool oddDimensionsRejected = false;
        try {
            static_cast<void>(arrow::generateBoard(
                arrow::GeneratorOptions{5, 4, 2, 12, 7U, 1}
            ));
        } catch (const std::invalid_argument& error) {
            oddDimensionsRejected =
                std::string(error.what()).find("even board dimensions") !=
                std::string::npos;
        }
        require(
            oddDimensionsRejected,
            "odd board dimensions were not rejected with a clear error"
        );

        bool impossibleBoardRejected = false;
        try {
            static_cast<void>(arrow::generateBoard(
                arrow::GeneratorOptions{4, 4, 16, 16, 7U, 0}
            ));
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            impossibleBoardRejected =
                message.find("Unable to generate a valid 4x4 labyrinth") !=
                    std::string::npos &&
                message.find("seed 7") != std::string::npos &&
                message.find("1 candidate attempt") != std::string::npos;
        }
        require(
            impossibleBoardRejected,
            "failed labyrinth generation used a fallback or unclear error"
        );

        constexpr std::array<std::uint32_t, 8> largeBoardSeeds{
            0U,
            1U,
            2U,
            7U,
            64U,
            747U,
            23'000U,
            4'000'000'000U
        };
        std::size_t minimumLargeTightSelfPasses =
            std::numeric_limits<std::size_t>::max();
        std::size_t minimumLargeComplexTightSelfPasses =
            std::numeric_limits<std::size_t>::max();
        Quality seed64HighQuality;
        for (std::uint32_t largeSeed : largeBoardSeeds) {
            const arrow::GeneratorOptions largeBoardOptions{
                40,
                40,
                2,
                60,
                largeSeed
            };
            const Quality quality = verifyBoard(
                arrow::generateBoard(largeBoardOptions),
                largeBoardOptions
            );
            require(
                quality.arrowCount >= 160 &&
                    quality.arrowCount <= 220,
                "40x40 board does not contain a dense arrow mix"
            );
            require(
                quality.shortArrows * 4 >=
                        quality.arrowCount &&
                    quality.mediumArrows * 4 >=
                        quality.arrowCount &&
                    quality.longArrows >= 5,
                "40x40 board lost its short/medium/long mix"
            );
            require(
                quality.initiallyRemovable == 1 &&
                    quality.directionCount >= 3,
                "40x40 board has a weak dependency structure"
            );
            minimumLargeTightSelfPasses = std::min(
                minimumLargeTightSelfPasses,
                quality.tightSelfPassArrows
            );
            minimumLargeComplexTightSelfPasses = std::min(
                minimumLargeComplexTightSelfPasses,
                quality.longComplexTightSelfPassArrows
            );
            if (largeSeed == 64U) {
                seed64HighQuality = quality;
            }
        }
        require(
            minimumLargeTightSelfPasses >= 6,
            "40x40 fixed seeds contain too few exact tail-pass arrows"
        );
        require(
            minimumLargeComplexTightSelfPasses >= 2,
            "40x40 fixed seeds contain too few long complex exact tail passes"
        );
        const arrow::GeneratorOptions seed64FastOptions{
            40,
            40,
            2,
            60,
            64U,
            1
        };
        const Quality seed64FastQuality = verifyBoard(
            arrow::generateBoard(seed64FastOptions),
            seed64FastOptions
        );
        require(
            seed64HighQuality.tightSelfPassArrows >=
                    seed64FastQuality.tightSelfPassArrows &&
                seed64HighQuality.longComplexTightSelfPassArrows >=
                    seed64FastQuality.longComplexTightSelfPassArrows,
            "eight-candidate selection reduced fixed-seed tail-pass quality"
        );

        int defaultSeedCount = 1000;
        if (argc >= 2) {
            defaultSeedCount = std::max(1, std::stoi(argv[1]));
        }
        const std::uint32_t firstDefaultSeed =
            argc >= 3
                ? static_cast<std::uint32_t>(std::stoul(argv[2]))
                : 0U;
        const int alternateSeedCount = std::max(
            1,
            defaultSeedCount / 10
        );

        std::size_t minimumArrows = std::numeric_limits<std::size_t>::max();
        std::size_t maximumArrows = 0;
        std::size_t maximumInitial = 0;
        std::size_t minimumDirections = 4;
        std::size_t minimumBent = std::numeric_limits<std::size_t>::max();
        std::size_t minimumTurns = std::numeric_limits<std::size_t>::max();
        std::size_t minimumLength = std::numeric_limits<std::size_t>::max();
        std::size_t maximumLength = 0;
        std::size_t maximumChoices = 0;
        std::size_t maximumChoiceExcess = 0;
        std::size_t minimumInterEdges = std::numeric_limits<std::size_t>::max();
        std::size_t minimumHorizontalInter =
            std::numeric_limits<std::size_t>::max();
        std::size_t minimumVerticalInter =
            std::numeric_limits<std::size_t>::max();
        std::size_t minimumScanlineExcess =
            std::numeric_limits<std::size_t>::max();
        std::size_t maximumRectangular = 0;
        std::size_t minimumRichMacro = 16;
        double maximumMedianFill = 0.0;

        std::size_t minimumShort = std::numeric_limits<std::size_t>::max();
        std::size_t minimumMedium = std::numeric_limits<std::size_t>::max();
        std::size_t minimumLong = std::numeric_limits<std::size_t>::max();
        std::size_t minimumVeryLong =
            std::numeric_limits<std::size_t>::max();
        std::size_t minimumLengthIqr =
            std::numeric_limits<std::size_t>::max();
        double minimumLengthCv = std::numeric_limits<double>::max();

        for (std::uint32_t offset = 0;
             offset < static_cast<std::uint32_t>(defaultSeedCount);
             ++offset) {
            const std::uint32_t seed = firstDefaultSeed + offset;
            const arrow::GeneratorOptions options{20, 20, 2, 30, seed};
            Quality quality;
            try {
                const arrow::Board board = arrow::generateBoard(options);
                quality = verifyBoard(board, options);
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    "default seed " + std::to_string(seed) + ": " +
                    error.what()
                );
            }
            minimumArrows = std::min(minimumArrows, quality.arrowCount);
            maximumArrows = std::max(maximumArrows, quality.arrowCount);
            maximumInitial = std::max(
                maximumInitial,
                quality.initiallyRemovable
            );
            minimumDirections = std::min(
                minimumDirections,
                quality.directionCount
            );
            minimumBent = std::min(minimumBent, quality.bentArrows);
            minimumShort = std::min(minimumShort, quality.shortArrows);
            minimumMedium = std::min(minimumMedium, quality.mediumArrows);
            minimumLong = std::min(minimumLong, quality.longArrows);
            minimumVeryLong = std::min(
                minimumVeryLong,
                quality.veryLongArrows
            );
            minimumLengthIqr = std::min(
                minimumLengthIqr,
                quality.thirdQuartileLength -
                    quality.firstQuartileLength
            );
            minimumLengthCv = std::min(
                minimumLengthCv,
                quality.lengthCoefficientOfVariation
            );
            minimumTurns = std::min(minimumTurns, quality.turns);
            minimumLength = std::min(minimumLength, quality.shortestArrow);
            maximumLength = std::max(maximumLength, quality.longestArrow);
            maximumChoices = std::max(
                maximumChoices,
                quality.maximumChoices
            );
            maximumChoiceExcess = std::max(
                maximumChoiceExcess,
                quality.choiceExcess
            );
            minimumInterEdges = std::min(
                minimumInterEdges,
                quality.interArrowEdges
            );
            minimumHorizontalInter = std::min(
                minimumHorizontalInter,
                quality.horizontalInterArrowEdges
            );
            minimumVerticalInter = std::min(
                minimumVerticalInter,
                quality.verticalInterArrowEdges
            );
            minimumScanlineExcess = std::min(
                minimumScanlineExcess,
                quality.scanlineExcessRuns
            );
            maximumRectangular = std::max(
                maximumRectangular,
                quality.rectangularArrows
            );
            minimumRichMacro = std::min(
                minimumRichMacro,
                quality.richMacroBlocks
            );
            maximumMedianFill = std::max(
                maximumMedianFill,
                quality.medianBoundingFill
            );
        }

        for (std::uint32_t seed = 0;
             seed < static_cast<std::uint32_t>(alternateSeedCount);
             ++seed) {
            const std::array options{
                arrow::GeneratorOptions{8, 8, 5, 15, seed},
                arrow::GeneratorOptions{30, 16, 20, 58, seed},
                arrow::GeneratorOptions{12, 26, 10, 32, seed}
            };
            for (const arrow::GeneratorOptions& value : options) {
                try {
                    const arrow::Board board = arrow::generateBoard(value);
                    static_cast<void>(verifyBoard(board, value));
                } catch (const std::exception& error) {
                    throw std::runtime_error(
                        "alternate seed " + std::to_string(seed) + " (" +
                        std::to_string(value.columns) + "x" +
                        std::to_string(value.rows) + ", lengths " +
                        std::to_string(value.minimumLength) + ".." +
                        std::to_string(value.maximumLength) + "): " +
                        error.what()
                    );
                }
            }
        }

        std::cout
            << "Default-board quality across " << defaultSeedCount
            << " seeds: arrows "
            << minimumArrows << ".." << maximumArrows
            << ", initial moves <= " << maximumInitial
            << ", directions >= " << minimumDirections
            << ", bent arrows >= " << minimumBent
            << ", S/M/L >= " << minimumShort
            << "/" << minimumMedium
            << "/" << minimumLong
            << ", very long >= " << minimumVeryLong
            << ", length IQR >= " << minimumLengthIqr
            << ", length CV >= " << minimumLengthCv
            << ", turns >= " << minimumTurns
            << ", lengths " << minimumLength << ".." << maximumLength
            << ", max choices <= " << maximumChoices
            << ", choice excess <= " << maximumChoiceExcess
            << ", inter-edges >= " << minimumInterEdges
            << " (H " << minimumHorizontalInter
            << ", V " << minimumVerticalInter << ")"
            << ", scanline excess >= " << minimumScanlineExcess
            << ", median bbox fill <= " << maximumMedianFill
            << ", rectangular arrows <= " << maximumRectangular
            << ", rich 5x5 blocks >= " << minimumRichMacro
            << ", 40x40 tight/long-complex tail passes >= "
            << minimumLargeTightSelfPasses
            << "/" << minimumLargeComplexTightSelfPasses
            << " (seed 64: one candidate "
            << seed64FastQuality.tightSelfPassArrows
            << "/"
            << seed64FastQuality.longComplexTightSelfPassArrows
            << ", eight candidates "
            << seed64HighQuality.tightSelfPassArrows
            << "/"
            << seed64HighQuality.longComplexTightSelfPassArrows
            << ")"
            << ".\n"
            << "All generator tests passed ("
            << defaultSeedCount + alternateSeedCount * 3 +
                   static_cast<int>(largeBoardSeeds.size()) + 1
            << " boards).\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "TEST FAILURE: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
