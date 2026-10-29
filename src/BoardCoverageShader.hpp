#pragma once

namespace arrow::render {

// Local primitive coordinates and their half extents are stored in the existing
// UV/position buffers. A half-pixel fringe covers boundary pixels at any zoom.
inline constexpr const char* kBoardCoverageVertexShader = R"glsl(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform float pixelSize;
out vec2 localPosition;
out vec4 primitiveColor;
flat out vec2 halfSize;
flat out int primitiveKind;
flat out float bendRatio;

void main() {
    vec2 fringe = sign(vertexTexCoord) * (0.5 * pixelSize);
    // Segment ends are internal boundaries covered by the round join/cap.
    // Filtering them again would leave pale seams where primitives meet.
    if (int(vertexPosition.z + 0.5) == 0) fringe.x = 0.0;
    if (int(vertexPosition.z + 0.5) == 6) fringe.y = 0.0;
    int kind = int(vertexPosition.z + 0.5);
    if (kind >= 7 && kind <= 10) {
        vec2 quadrant = vec2(kind == 7 || kind == 10 ? 1.0 : -1.0,
                             kind == 7 || kind == 8 ? 1.0 : -1.0);
        // Tangent ends meet unfiltered straight runs, just as the old joins did.
        if (vertexTexCoord.x * quadrant.x < 0.0) fringe.x = 0.0;
        if (vertexTexCoord.y * quadrant.y < 0.0) fringe.y = 0.0;
    }
    if (kind >= 17) {
        int flags = kind - 15;
        bool vertical = (flags & 1) != 0;
        float along = vertical ? vertexTexCoord.y : vertexTexCoord.x;
        bool capped = along < 0.0 ? (flags & 2) != 0 : (flags & 4) != 0;
        if (!capped) {
            if (vertical) fringe.y = 0.0;
            else fringe.x = 0.0;
        }
    }
    localPosition = vertexTexCoord + fringe;
    halfSize = abs(vertexTexCoord);
    primitiveKind = int(vertexPosition.z + 0.5);
    bendRatio = fract(vertexPosition.z) * 4.0;
    primitiveColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition.xy + fringe, 0.0, 1.0);
}
)glsl";

inline constexpr const char* kBoardCoverageFragmentShader = R"glsl(#version 330
in vec2 localPosition;
in vec4 primitiveColor;
flat in vec2 halfSize;
flat in int primitiveKind;
flat in float bendRatio;
uniform float pixelSize;
uniform vec4 colDiffuse;
out vec4 finalColor;

float discIntegral(float x, float radius) {
    float square = radius * radius;
    return 0.5 * (x * sqrt(max(square - x*x, 0.0)) +
                  square * asin(clamp(x / radius, 0.0, 1.0)));
}

// Signed area between the origin and (x,y), clipped to the disc.
float discQuadrant(vec2 point, float radius) {
    vec2 p = min(abs(point), vec2(radius));
    float area = p.x * p.y;
    if (dot(p, p) > radius * radius) {
        float crossing = sqrt(max(radius * radius - p.y*p.y, 0.0));
        area = p.y * crossing + discIntegral(p.x, radius) -
               discIntegral(crossing, radius);
    }
    return sign(point.x) * sign(point.y) * area;
}

float discCoverage(vec2 p, float radius) {
    float pixelHalf = 0.5 * pixelSize;
    vec2 nearest = max(abs(p) - pixelHalf, vec2(0.0));
    if (dot(nearest, nearest) >= radius * radius) return 0.0;
    vec2 farthest = abs(p) + pixelHalf;
    if (dot(farthest, farthest) <= radius * radius) return 1.0;
    if (all(greaterThanEqual(vec2(pixelHalf), abs(p) + radius))) {
        return 3.14159265359 * radius * radius / (pixelSize * pixelSize);
    }
    vec2 lo = p - pixelHalf;
    vec2 hi = p + pixelHalf;
    float area = discQuadrant(hi, radius) -
                 discQuadrant(vec2(lo.x, hi.y), radius) -
                 discQuadrant(vec2(hi.x, lo.y), radius) +
                 discQuadrant(lo, radius);
    return clamp(area / (pixelSize * pixelSize), 0.0, 1.0);
}

float discRectangle(vec2 lo, vec2 hi, float radius) {
    if (radius <= 0.0 || any(lessThanEqual(hi,lo))) return 0.0;
    vec2 nearest = max(max(lo,-hi),vec2(0.0));
    if (dot(nearest,nearest) >= radius*radius) return 0.0;
    vec2 farthest = max(abs(lo),abs(hi));
    if (dot(farthest,farthest) <= radius*radius) return (hi.x-lo.x)*(hi.y-lo.y);
    return discQuadrant(hi, radius) - discQuadrant(vec2(lo.x, hi.y), radius) -
           discQuadrant(vec2(hi.x, lo.y), radius) + discQuadrant(lo, radius);
}

float cappedLineCoverage(vec2 p, vec2 extent, int kind) {
    int flags = kind - 15;
    if ((flags & 1) != 0) { p = p.yx; extent = extent.yx; }
    float radius = extent.y;
    bool capLow = (flags & 2) != 0;
    bool capHigh = (flags & 4) != 0;
    float low = -extent.x + (capLow ? radius : 0.0);
    float high = extent.x - (capHigh ? radius : 0.0);
    vec2 lo = p - 0.5*pixelSize;
    vec2 hi = p + 0.5*pixelSize;
    // Uncapped ends adjoin bends: evaluate the neighbouring straight extension
    // rather than antialiasing an internal endpoint a second time.
    vec2 covered = max(min(hi,vec2(capHigh ? high : hi.x,radius)) -
                       max(lo,vec2(capLow ? low : lo.x,-radius)), vec2(0.0));
    float area = covered.x*covered.y;
    if (capLow && lo.x < low) {
        area += discRectangle(vec2(lo.x-low,lo.y), vec2(min(hi.x,low)-low,hi.y),radius);
    }
    if (capHigh && hi.x > high) {
        area += discRectangle(vec2(max(lo.x,high)-high,lo.y), vec2(hi.x-high,hi.y),radius);
    }
    return clamp(area/(pixelSize*pixelSize), 0.0, 1.0);
}

float bendCoverage(vec2 p, vec2 extent, int kind) {
    p *= vec2(kind == 7 || kind == 10 ? 1.0 : -1.0,
              kind == 7 || kind == 8 ? 1.0 : -1.0);
    float side = 2.0 * extent.x;
    float radius = side * bendRatio;
    float halfStroke = side - 2.0*radius;
    float outer = radius + halfStroke;
    p += extent - radius;
    float inner = max(radius - halfStroke, 0.0);
    vec2 lo = p - 0.5 * pixelSize;
    vec2 hi = p + 0.5 * pixelSize;
    if (all(greaterThanEqual(lo, vec2(0.0)))) {
        if (dot(lo, lo) >= outer*outer || dot(hi, hi) <= inner*inner) return 0.0;
        if (dot(hi, hi) <= outer*outer && dot(lo, lo) >= inner*inner) return 1.0;
    }
    // Integrate the quarter annulus and its two tangent straight extensions.
    // Including the extensions keeps tangent pixels solid instead of filtering
    // the arc and its neighbouring straight run as separate transparent ends.
    vec2 quadrantLo = max(lo, vec2(0.0));
    vec2 quadrantHi = max(hi, vec2(0.0));
    float area = discRectangle(quadrantLo, quadrantHi, outer) -
                 discRectangle(quadrantLo, quadrantHi, inner);
    float low = radius - halfStroke;
    area += max(min(hi.x, 0.0) - lo.x, 0.0) *
            max(min(hi.y, outer) - max(lo.y, low), 0.0);
    area += max(min(hi.y, 0.0) - lo.y, 0.0) *
            max(min(hi.x, outer) - max(lo.x, low), 0.0);
    if (low < 0.0) {
        vec2 overlap = max(min(hi, vec2(0.0)) - max(lo, vec2(low)), vec2(0.0));
        area -= overlap.x * overlap.y;
    }
    return clamp(area / (pixelSize * pixelSize), 0.0, 1.0);
}

// Integrate a linear upper boundary over a rectangle in the positive Y half.
// Splitting at full/partial/empty coverage avoids subtracting nearly equal
// squared values at high zoom. No per-fragment polygon arrays are needed.
float halfTriangleArea(float x0, float x1, float y0, float y1, float h, float slope) {
    if (x1 <= x0 || y1 <= y0) return 0.0;
    float fullEnd = clamp(h-y1/slope, x0, x1);
    float emptyStart = clamp(h-y0/slope, x0, x1);
    float leftHeight = clamp(slope*(h-fullEnd)-y0, 0.0, y1-y0);
    float rightHeight = clamp(slope*(h-emptyStart)-y0, 0.0, y1-y0);
    return (fullEnd-x0)*(y1-y0) +
           0.5*(emptyStart-fullEnd)*(leftHeight+rightHeight);
}

float triangleRectangle(vec2 lo, vec2 hi, float h, float w) {
    float x0 = max(lo.x,-h);
    float x1 = min(hi.x,h);
    float slope = w/(2.0*h);
    return halfTriangleArea(x0,x1,max(lo.y,0.0),max(hi.y,0.0),h,slope) +
           halfTriangleArea(x0,x1,max(-hi.y,0.0),max(-lo.y,0.0),h,slope);
}

float triangleCoverage(vec2 p, vec2 extent, int kind, bool rounded) {
    // Rotate each cardinal arrowhead into the same right-facing triangle.
    if (kind == 3 || kind == 5) { p = p.yx; extent = extent.yx; }
    if (kind == 4 || kind == 5) p.x = -p.x;
    float h = extent.x;
    float w = extent.y;
    vec2 lo = p - 0.5*pixelSize;
    vec2 hi = p + 0.5*pixelSize;
    float area;
    if (!rounded) {
        area = triangleRectangle(lo,hi,h,w);
    } else {
        float radius = min(h,w)*(1.6/9.5);
        float side = length(vec2(2.0*h,w));
        float tangent = radius*(side+w)/(2.0*h);
        float baseEnd = -h + 2.0*h*tangent/side;
        float tipStart = h - radius*4.0*h*h/(w*side);
        float tipCenter = h - radius*side/w;
        // The rounded silhouette splits into three disjoint vertical strips:
        // two base arcs with a rectangle between them, straight sloping sides,
        // then the tip disc. All reuse the existing exact disc/box integrals.
        area = triangleRectangle(vec2(max(lo.x,baseEnd),lo.y),
                                 vec2(min(hi.x,tipStart),hi.y),h,w);
        float baseLo = max(lo.x,-h);
        float baseHi = min(hi.x,baseEnd);
        if (baseHi > baseLo) {
            float centerY = w-tangent;
            area += (baseHi-baseLo)*max(min(hi.y,centerY)-max(lo.y,-centerY),0.0);
            vec2 arcLo = vec2(baseLo+h-radius, max(lo.y-centerY,0.0));
            vec2 arcHi = vec2(baseHi+h-radius, max(hi.y-centerY,0.0));
            area += discRectangle(arcLo,arcHi,radius);
            arcLo.y = min(lo.y+centerY,0.0);
            arcHi.y = min(hi.y+centerY,0.0);
            area += discRectangle(arcLo,arcHi,radius);
        }
        if (hi.x > tipStart) {
            area += discRectangle(vec2(max(lo.x,tipStart)-tipCenter,lo.y),
                                  vec2(hi.x-tipCenter,hi.y),radius);
        }
    }
    return clamp(area/(pixelSize*pixelSize),0.0,1.0);
}

void main() {
    float coverage;
    if (primitiveKind == 0 || primitiveKind == 6) {
        // Exact box coverage makes a subpixel stroke equally strong regardless
        // of whether it falls on one pixel column or between two columns.
        vec2 lo = max(localPosition - 0.5*pixelSize, -halfSize);
        vec2 hi = min(localPosition + 0.5*pixelSize,  halfSize);
        vec2 covered = max(hi - lo, vec2(0.0)) / pixelSize;
        coverage = primitiveKind == 0 ? covered.y : covered.x;
    } else if (primitiveKind == 1) {
        coverage = discCoverage(localPosition, halfSize.x);
    } else if (primitiveKind >= 7 && primitiveKind <= 10) {
        coverage = bendCoverage(localPosition, halfSize, primitiveKind);
    } else if (primitiveKind >= 17) {
        coverage = cappedLineCoverage(localPosition, halfSize, primitiveKind);
    } else {
        bool rounded = primitiveKind >= 11;
        coverage = triangleCoverage(localPosition, halfSize,
                                    rounded ? primitiveKind-9 : primitiveKind, rounded);
    }
    finalColor = primitiveColor * colDiffuse;
    finalColor.a *= coverage;
}
)glsl";

} // namespace arrow::render
