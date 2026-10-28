#include "MenuScreen.hpp"
#include "UiText.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace arrow {
namespace {

constexpr Color kGold{255, 218, 126, 255};
constexpr Color kLight{232, 240, 254, 255};
constexpr Color kQuiet{151, 174, 209, 255};
constexpr Color kCyan{128, 210, 255, 255};
constexpr Color kTeal{120, 224, 194, 255};
constexpr Color kDark{22, 37, 60, 255};

void centered(UiText& text, const char* label, float x, float y, float size, float spacing, Color color) {
    const Vector2 measured = text.measure(label, size, spacing);
    text.draw(label, {std::round(x - measured.x * 0.5F), std::round(y)}, size, spacing, color);
}

void arrowPath(std::span<const Vector2> points, float thickness, Color color) {
    for (std::size_t index = 1; index < points.size(); ++index) {
        DrawLineEx(points[index - 1], points[index], thickness, color);
        DrawCircleV(points[index - 1], thickness * 0.5F, color);
    }
    const Vector2 tip = points.back();
    const Vector2 previous = points[points.size() - 2];
    const float length = std::hypot(tip.x - previous.x, tip.y - previous.y);
    const Vector2 direction{(tip.x - previous.x) / length, (tip.y - previous.y) / length};
    const float size = thickness * 3.5F;
    const Vector2 base{tip.x - direction.x * size, tip.y - direction.y * size};
    DrawTriangle(tip, {base.x + direction.y * size * 0.58F, base.y - direction.x * size * 0.58F},
                 {base.x - direction.y * size * 0.58F, base.y + direction.x * size * 0.58F}, color);
}

void icon(MenuScreen::Page page, int index, Vector2 center, Color ink) {
    if (page == MenuScreen::Page::Difficulty && index < 4) {
        for (int bar = 0; bar < 4; ++bar) {
            const float height = 5 + bar * 4.0F;
            DrawRectangleRounded({center.x - 13 + bar * 7.0F, center.y + 10 - height, 4, height},
                0.4F, 4, Fade(ink, bar <= index ? 1.0F : 0.18F));
        }
    } else if ((page != MenuScreen::Page::Main && index == 4)) {
        DrawLineEx({center.x + 9, center.y}, {center.x - 9, center.y}, 1.8F, ink);
        DrawLineEx({center.x - 9, center.y}, {center.x - 3, center.y - 6}, 1.8F, ink);
        DrawLineEx({center.x - 9, center.y}, {center.x - 3, center.y + 6}, 1.8F, ink);
    } else if (page == MenuScreen::Page::Main && index < 2) {
        DrawTriangle({center.x - 4, center.y - 9}, {center.x - 4, center.y + 9}, {center.x + 8, center.y}, ink);
        if (index == 1) DrawLineEx({center.x - 10, center.y - 9}, {center.x - 10, center.y + 9}, 2, ink);
    } else if (page == MenuScreen::Page::Main && index == 3) {
        DrawLineEx({center.x - 5, center.y - 10}, {center.x - 10, center.y - 10}, 1.8F, ink);
        DrawLineEx({center.x - 10, center.y - 10}, {center.x - 10, center.y + 10}, 1.8F, ink);
        DrawLineEx({center.x - 10, center.y + 10}, {center.x - 5, center.y + 10}, 1.8F, ink);
        DrawLineEx({center.x - 4, center.y}, {center.x + 11, center.y}, 1.8F, ink);
        DrawLineEx({center.x + 11, center.y}, {center.x + 5, center.y - 6}, 1.8F, ink);
        DrawLineEx({center.x + 11, center.y}, {center.x + 5, center.y + 6}, 1.8F, ink);
    } else if (page == MenuScreen::Page::Main) {
        for (int row = -1; row <= 1; ++row) {
            const float y = center.y + row * 7.0F;
            DrawLineEx({center.x - 11, y}, {center.x + 11, y}, 1.5F, ink);
            DrawCircleV({center.x + (row == 0 ? -4.0F : 5.0F), y}, 3.2F, ink);
        }
    } else if (index == 0) {
        const std::array<Vector2, 5> path{{{center.x - 10, center.y + 8}, {center.x + 9, center.y + 8},
            {center.x + 9, center.y - 9}, {center.x - 9, center.y - 9}, {center.x - 9, center.y + 1}}};
        arrowPath(path, 1.8F, ink);
    } else if (index == 1) {
        DrawLineEx({center.x - 12, center.y - 8}, {center.x - 3, center.y + 1}, 1.8F, ink);
        DrawLineEx({center.x + 12, center.y - 8}, {center.x + 3, center.y + 1}, 1.8F, ink);
        DrawLineEx({center.x - 4, center.y + 6}, {center.x - 7, center.y + 11}, 1.5F, ink);
        DrawLineEx({center.x + 4, center.y + 6}, {center.x + 7, center.y + 11}, 1.5F, ink);
        DrawCircleV(center, 2, ink);
    } else if (index == 2) {
        DrawRing(center, 6, 7.5F, 0, 360, 24, ink);
        DrawLineEx({center.x - 12, center.y}, {center.x + 12, center.y}, 1.5F, ink);
        DrawLineEx({center.x, center.y - 12}, {center.x, center.y + 12}, 1.5F, ink);
    } else {
        DrawRectangleRec({center.x - 11, center.y - 4, 4, 8}, ink);
        const std::array<Vector2, 4> cone{{{center.x - 7, center.y - 4}, {center.x - 7, center.y + 4},
            {center.x, center.y + 10}, {center.x, center.y - 10}}};
        DrawTriangleFan(cone.data(), static_cast<int>(cone.size()), ink);
        DrawRing({center.x, center.y}, 7, 8.5F, -50, 50, 16, ink);
        DrawRing({center.x, center.y}, 11, 12.5F, -50, 50, 16, ink);
    }
}

const char* description(MenuScreen::Page page, int index, bool enabled) {
    constexpr std::array<const char*, 4> main{{"Choose your next challenge", "Continue your saved puzzle",
        "Sound and gameplay preferences", "Close Arrow Puzzle"}};
    constexpr std::array<const char*, 4> difficulty{{"20 x 20 cells", "40 x 40 cells", "80 x 80 cells", "160 x 160 cells"}};
    constexpr std::array<const char*, 4> settings{{"Allow loops in newly generated puzzles.",
        "Moving arrows can cost a life on contact.", "Highlight the cursor's row and column.",
        "Play arrow, collision and celebration sounds."}};
    if (page == MenuScreen::Page::Main) return index == 1 && !enabled ? "No unfinished puzzle to resume" : main[index];
    if (index == 4) return nullptr;
    return page == MenuScreen::Page::Settings ? settings[index] : difficulty[index];
}

} // namespace

Rectangle MenuScreen::Layout::button(int index) const {
    return {panel.x + 32, std::round(rowTop + index * (rowHeight + rowGap)), panel.width - 64, rowHeight};
}

MenuScreen::Layout MenuScreen::layout(int width, int height, int count) {
    const float panelWidth = std::min(620.0F, static_cast<float>(width) - 64);
    const float panelHeight = std::min(count == 4 ? 550.0F : 600.0F, static_cast<float>(height) - 72);
    const Rectangle panel{std::round((width - panelWidth) * 0.5F), std::round((height - panelHeight) * 0.5F),
                          panelWidth, panelHeight};
    const float gap = height < 700 ? 10.0F : 12.0F;
    const float available = panelHeight - 180 - 64;
    const float rowHeight = std::min(66.0F, (available - gap * (count - 1)) / count);
    const float total = rowHeight * count + gap * (count - 1);
    return {panel, panel.y + 180 + (available - total) * 0.5F, rowHeight, gap};
}

Color MenuScreen::mutedColor() { return kQuiet; }

void MenuScreen::drawBackground(int width, int height) {
    DrawRectangleGradientV(0, 0, width, height, {10, 20, 41, 255}, {22, 40, 66, 255});
    const float scale = std::min(static_cast<float>(width), static_cast<float>(height));
    DrawCircleGradient(static_cast<int>(width * 0.23F), static_cast<int>(height * 0.20F),
        scale * 0.60F, Fade(kGold, 0.065F), BLANK);
    DrawCircleGradient(static_cast<int>(width * 0.81F), static_cast<int>(height * 0.80F),
        scale * 0.62F, Fade(kCyan, 0.06F), BLANK);
    std::uint32_t state = 0xA7702026U;
    const auto random = [&]() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state & 0xFFFFFFU) / 16777216.0F;
    };
    for (int index = 0; index < 58; ++index) {
        const Vector2 point{random() * width, random() * height};
        DrawCircleV(point, index % 7 == 0 ? 1.5F : 0.8F, Fade(kQuiet, index % 3 == 0 ? 0.38F : 0.18F));
    }
    if (width < 960) return;
    // Quiet, static arrow constellations connect the menu to the puzzle itself.
    const float unit = scale * 0.05F;
    const Vector2 left{width * 0.11F, height * 0.30F};
    const std::array<Vector2, 5> leftPath{{{left.x, left.y}, {left.x + unit, left.y},
        {left.x + unit, left.y + unit * 2}, {left.x - unit, left.y + unit * 2}, {left.x - unit, left.y + unit * 3}}};
    arrowPath(leftPath, 2.0F, Fade(kGold, 0.13F));
    const Vector2 right{width * 0.89F, height * 0.58F};
    const std::array<Vector2, 5> rightPath{{{right.x, right.y}, {right.x - unit, right.y},
        {right.x - unit, right.y - unit * 2}, {right.x + unit, right.y - unit * 2}, {right.x + unit, right.y - unit * 3}}};
    arrowPath(rightPath, 2.0F, Fade(kCyan, 0.14F));
}

void MenuScreen::drawHeading(UiText& text, Page page, int width, int height, int count) {
    const Rectangle panel = layout(width, height, count).panel;
    const float center = static_cast<float>(width) * 0.5F;
    DrawRectangleRounded({panel.x + 4, panel.y + 8, panel.width, panel.height}, 0.075F, 16, Fade(BLACK, 0.2F));
    DrawRectangleRounded(panel, 0.075F, 16, kDark);
    DrawRectangleRoundedLinesEx(panel, 0.075F, 16, 1, Fade(kGold, 0.28F));
    const Vector2 badge{center, panel.y};
    DrawCircleV(badge, 34, {35, 47, 63, 255});
    DrawRing(badge, 32, 33, 0, 360, 64, Fade(kGold, 0.75F));
    if (page == Page::Main) {
        const std::array<Vector2, 4> path{{{center - 13, panel.y + 12}, {center - 13, panel.y - 8},
            {center + 11, panel.y - 8}, {center + 11, panel.y + 11}}};
        arrowPath(path, 3.0F, kGold);
    } else {
        icon(page == Page::Settings ? Page::Main : Page::Difficulty, 2, badge, kGold);
    }
    centered(text, page == Page::Main ? "MAIN MENU" : "ARROW PUZZLE", center, panel.y + 43, 12, 2.6F, kQuiet);
    centered(text, page == Page::Main ? "ARROW PUZZLE" : page == Page::Settings ? "SETTINGS" : "NEW GAME",
        center, panel.y + 68, page == Page::Main ? 46.0F : 42.0F, 1.5F, kGold);
    centered(text, page == Page::Main ? "Find a way for every arrow." : page == Page::Settings
        ? "Choose how you play." : "Choose your challenge.", center, panel.y + 126, 17, 0.2F, kLight);
}

void MenuScreen::drawButton(UiText& text, Rectangle bounds, const char* label, Page page,
                            int index, bool enabled, bool focused, bool armed, std::optional<bool> toggle) {
    const bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const bool pressed = hovered && armed;
    const bool highlighted = enabled && (hovered || focused);
    const bool primary = page == Page::Main && index == 0;
    const Color accent = primary ? kGold : kCyan;
    Color fill{20, 33, 54, 255};
    if (enabled && primary) {
        fill = pressed ? Color{227, 191, 103, 255} : highlighted ? Color{255, 226, 153, 255} : kGold;
    } else if (enabled) {
        fill = pressed ? Color{43, 64, 90, 255} : highlighted ? Color{34, 54, 80, 255} : Color{26, 43, 67, 255};
    }
    const Color ink = !enabled ? Color{127, 147, 179, 255} : primary ? Color{28, 34, 48, 255} : kLight;
    const Color secondary = !enabled ? Color{105, 125, 156, 255} : primary ? Color{81, 72, 51, 255} : kQuiet;
    DrawRectangleRounded(bounds, 0.22F, 10, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.22F, 10, highlighted ? 1.5F : 1.0F,
        primary ? kGold : highlighted ? Fade(accent, 0.8F) : Color{50, 69, 96, 255});
    const Vector2 center{bounds.x + 32, bounds.y + bounds.height * 0.5F};
    icon(page, index, center, enabled ? primary ? ink : highlighted ? accent : kQuiet : secondary);
    const char* detail = description(page, index, enabled);
    const bool compact = bounds.height < 57;
    const float titleSize = compact ? 16.0F : 19.0F;
    const float detailSize = compact ? 11.0F : 13.0F;
    const float labelX = bounds.x + 64;
    if (detail != nullptr) {
        text.draw(label, {labelX, std::round(bounds.y + (compact ? 7.0F : 10.0F))}, titleSize, 0.2F, ink);
        text.draw(detail, {labelX, std::round(bounds.y + (compact ? 28.0F : 35.0F))}, detailSize, 0.1F, secondary);
    } else {
        const Vector2 size = text.measure(label, titleSize, 0.2F);
        text.draw(label, {labelX, std::round(bounds.y + (bounds.height - size.y) * 0.5F)}, titleSize, 0.2F, ink);
    }
    if (toggle.has_value()) {
        const Rectangle pill{bounds.x + bounds.width - 88, center.y - 14, 68, 28};
        DrawRectangleRounded(pill, 1, 12, *toggle ? Fade(kTeal, 0.20F) : Color{51, 66, 89, 255});
        DrawRectangleRoundedLinesEx(pill, 1, 12, 1, *toggle ? Fade(kTeal, 0.65F) : Color{75, 93, 120, 255});
        DrawCircleV({pill.x + (*toggle ? pill.width - 14 : 14), center.y}, 9, *toggle ? kTeal : kQuiet);
        centered(text, *toggle ? "ON" : "OFF", pill.x + (*toggle ? 23 : 45), center.y - 7, 11, 0.2F,
            *toggle ? kTeal : kQuiet);
    } else if (enabled && (page == Page::Main || index < 4)) {
        const Vector2 chevron{bounds.x + bounds.width - 28, center.y};
        DrawLineEx({chevron.x - 3, chevron.y - 5}, {chevron.x + 2, chevron.y}, 1.6F, primary ? ink : kQuiet);
        DrawLineEx({chevron.x + 2, chevron.y}, {chevron.x - 3, chevron.y + 5}, 1.6F, primary ? ink : kQuiet);
    }
}

void MenuScreen::drawFooter(UiText& text, const char* label, int width, int height, int count) {
    const Rectangle panel = layout(width, height, count).panel;
    centered(text, label, static_cast<float>(width) * 0.5F, panel.y + panel.height - 33, 11, 0.2F, kQuiet);
}

} // namespace arrow
