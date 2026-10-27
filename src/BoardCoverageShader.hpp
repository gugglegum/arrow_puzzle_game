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

void main() {
    vec2 fringe = sign(vertexTexCoord) * (0.5 * pixelSize);
    // Segment ends are internal boundaries covered by the round join/cap.
    // Filtering them again would leave pale seams where primitives meet.
    if (int(vertexPosition.z + 0.5) == 0) fringe.x = 0.0;
    if (int(vertexPosition.z + 0.5) == 6) fringe.y = 0.0;
    localPosition = vertexTexCoord + fringe;
    halfSize = abs(vertexTexCoord);
    primitiveKind = int(vertexPosition.z + 0.5);
    primitiveColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition.xy + fringe, 0.0, 1.0);
}
)glsl";

inline constexpr const char* kBoardCoverageFragmentShader = R"glsl(#version 330
in vec2 localPosition;
in vec4 primitiveColor;
flat in vec2 halfSize;
flat in int primitiveKind;
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

float triangleCoverage(vec2 p, vec2 extent, int kind) {
    // Rotate each cardinal arrowhead into the same right-facing triangle.
    if (kind == 3 || kind == 5) {
        p = p.yx;
        extent = extent.yx;
    }
    if (kind == 4 || kind == 5) p.x = -p.x;
    float h = extent.x;
    float w = extent.y;
    float halfPixel = 0.5 * pixelSize;
    vec2 polygon[8];
    vec2 clipped[8];
    polygon[0] = p + vec2(-halfPixel, -halfPixel);
    polygon[1] = p + vec2( halfPixel, -halfPixel);
    polygon[2] = p + vec2( halfPixel,  halfPixel);
    polygon[3] = p + vec2(-halfPixel,  halfPixel);
    int count = 4;
    // Intersect the pixel square with the triangle's three half planes.
    for (int edge = 0; edge < 3; ++edge) {
        vec2 normal = edge == 0 ? vec2(-1.0, 0.0) :
                      edge == 1 ? vec2(w, 2.0*h) : vec2(w, -2.0*h);
        float constant = edge == 0 ? -h : -w*h;
        int outputCount = 0;
        vec2 previous = polygon[count - 1];
        float previousDistance = dot(normal, previous) + constant;
        for (int i = 0; i < 8; ++i) {
            if (i >= count) break;
            vec2 current = polygon[i];
            float currentDistance = dot(normal, current) + constant;
            if ((previousDistance <= 0.0) != (currentDistance <= 0.0)) {
                float t = previousDistance / (previousDistance - currentDistance);
                clipped[outputCount++] = mix(previous, current, t);
            }
            if (currentDistance <= 0.0) clipped[outputCount++] = current;
            previous = current;
            previousDistance = currentDistance;
        }
        count = outputCount;
        if (count == 0) return 0.0;
        for (int i = 0; i < 8; ++i) {
            if (i >= count) break;
            polygon[i] = clipped[i];
        }
    }
    float twiceArea = 0.0;
    vec2 previous = polygon[count - 1];
    for (int i = 0; i < 8; ++i) {
        if (i >= count) break;
        vec2 current = polygon[i];
        twiceArea += previous.x*current.y - previous.y*current.x;
        previous = current;
    }
    return clamp(abs(twiceArea) / (2.0 * pixelSize * pixelSize), 0.0, 1.0);
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
    } else {
        coverage = triangleCoverage(localPosition, halfSize, primitiveKind);
    }
    finalColor = primitiveColor * colDiffuse;
    finalColor.a *= coverage;
}
)glsl";

} // namespace arrow::render
