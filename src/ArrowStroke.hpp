#pragma once

#include "StaticBoardGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace arrow {

// Stream orthogonal runs without allocating a second polyline. Idle cells and
// interpolated flight/rewind points use exactly the same joins and end caps.
template<class PointAt>
void addArrowStroke(StaticBoardGeometry& geometry, std::size_t count,
                    PointAt pointAt, float thickness, Color color) {
    if (count == 0) return;
    const float half = thickness*0.5F;
    if (count == 1) {
        geometry.addCircle(pointAt(0), half, color);
        return;
    }

    const auto runEnd = [&](std::size_t start) {
        const Vector2 first = pointAt(start);
        const Vector2 second = pointAt(start + 1);
        std::size_t end = start + 1;
        while (end + 1 < count) {
            const Vector2 current = pointAt(end);
            const Vector2 next = pointAt(end + 1);
            const bool straight = first.x == second.x
                ? next.x == first.x && (next.y-current.y)*(second.y-first.y) > 0.0F
                : next.y == first.y && (next.x-current.x)*(second.x-first.x) > 0.0F;
            if (!straight) break;
            ++end;
        }
        return end;
    };
    const auto length = [](Vector2 a, Vector2 b) {
        return std::abs(b.x-a.x) + std::abs(b.y-a.y);
    };

    Vector2 start = pointAt(0);
    float startRadius = 0.0F;
    std::size_t endIndex = runEnd(0);
    bool firstRun = true;
    for (;;) {
        const Vector2 end = pointAt(endIndex);
        const float runLength = length(start, end);
        const Vector2 incoming{(end.x-start.x)/runLength, (end.y-start.y)/runLength};
        const bool bend = endIndex + 1 < count;
        float radius = 0.0F;
        Vector2 outgoing{};
        std::size_t nextIndex = endIndex;
        if (bend) {
            nextIndex = runEnd(endIndex);
            const Vector2 next = pointAt(nextIndex);
            const float nextLength = length(end, next);
            outgoing = {(next.x-end.x)/nextLength, (next.y-end.y)/nextLength};
            // A straight stub of the same length belongs to the bend quad.
            // Partitioning on the straight part also keeps subpixel joins
            // stable at the minimum camera zoom, where a pixel spans a bend.
            radius = std::min({thickness*1.25F, runLength*0.25F, nextLength*0.25F});
        }
        geometry.addCappedLine({start.x + incoming.x*startRadius*2, start.y + incoming.y*startRadius*2},
                         {end.x - incoming.x*radius*2, end.y - incoming.y*radius*2},
                         thickness, color, firstRun, !bend);
        if (!bend) {
            break;
        }
        geometry.addBend(end, incoming, outgoing, radius, thickness, color);
        start = end;
        startRadius = radius;
        endIndex = nextIndex;
        firstRun = false;
    }
}

} // namespace arrow
