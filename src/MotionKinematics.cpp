#include "arrow/MotionKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arrow {

double acceleratedDistance(
    double speed,
    double acceleration,
    double time
) noexcept {
    speed = std::max(0.0, speed);
    acceleration = std::max(0.0, acceleration);
    time = std::max(0.0, time);
    if (time == 0.0) {
        return 0.0;
    }
    return speed * time +
           0.5 * acceleration * time * time;
}

double timeToCoverDistance(
    double distance,
    double speed,
    double acceleration
) noexcept {
    distance = std::max(0.0, distance);
    speed = std::max(0.0, speed);
    acceleration = std::max(0.0, acceleration);
    if (distance == 0.0) {
        return 0.0;
    }
    if (acceleration == 0.0) {
        return speed > 0.0
            ? distance / speed
            : std::numeric_limits<double>::infinity();
    }

    const double root = std::hypot(
        speed,
        std::sqrt(2.0 * acceleration * distance)
    );
    return distance / (0.5 * speed + 0.5 * root);
}

bool eventTimesCoincide(
    double first,
    double second
) noexcept {
    if (!std::isfinite(first) || !std::isfinite(second)) {
        return false;
    }
    if (first == second) {
        return true;
    }
    constexpr double ulpTolerance = 64.0;
    const double scale = std::max(
        std::abs(first),
        std::abs(second)
    );
    const double relativeTolerance =
        ulpTolerance *
        std::numeric_limits<double>::epsilon() *
        scale;
    const double subnormalTolerance =
        ulpTolerance *
        std::numeric_limits<double>::denorm_min();
    return std::abs(first - second) <=
           std::max(
               relativeTolerance,
               subnormalTolerance
           );
}

} // namespace arrow
