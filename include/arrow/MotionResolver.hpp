#pragma once

#include "arrow/Types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace arrow {

struct MotionBodyView {
    std::span<const Cell> cells;
    std::span<const Cell> proposal;
    std::span<const Cell> reservedTarget;
    bool present{};
    bool moving{};
    bool selfBlocked{};
};

struct MotionResolution {
    std::vector<std::uint8_t> hardFailures;
    std::vector<std::uint8_t> movingContacts;
    bool hasNewMovingContact{};
};

[[nodiscard]] MotionResolution resolveMotionStep(
    std::span<const MotionBodyView> bodies,
    std::span<const std::uint8_t> previousMovingContacts
);

} // namespace arrow
