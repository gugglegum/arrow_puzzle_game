#pragma once

#include <raylib.h>

#include <cstddef>
#include <memory>

namespace arrow {

// UI text is rasterized at its displayed size and drawn at integer pixels.
// Windows uses the user's native font smoothing settings, including ClearType.
class UiText {
public:
    static constexpr std::size_t kMaximumCachedBytes = 2 * 1024 * 1024;

    UiText();
    ~UiText();
    UiText(const UiText&) = delete;
    UiText& operator=(const UiText&) = delete;

    Vector2 measure(const char* text, float size, float spacing);
    void draw(const char* text, Vector2 position, float size, float spacing, Color color);
    void release(); // Call before closing the graphics context.

    bool usesWindowsSmoothing() const;
    std::size_t cachedBytes() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace arrow
