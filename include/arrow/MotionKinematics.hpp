#pragma once

namespace arrow {

[[nodiscard]] double acceleratedDistance(
    double speed,
    double acceleration,
    double time
) noexcept;

[[nodiscard]] double timeToCoverDistance(
    double distance,
    double speed,
    double acceleration
) noexcept;

[[nodiscard]] bool eventTimesCoincide(
    double first,
    double second
) noexcept;

} // namespace arrow
