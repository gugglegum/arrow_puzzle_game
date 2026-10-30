#pragma once

#include <raylib.h>
#include "arrow/Localization.hpp"

#include <optional>

namespace arrow {

class UiText;
enum class BoardTheme;

// Stateless menu rendering: no animation clock or additional texture cache.
class MenuScreen {
public:
    enum class Page { Main, Difficulty, Settings };
    struct Layout {
        Rectangle panel{};
        float rowTop{};
        float rowHeight{};
        float rowGap{};
        [[nodiscard]] Rectangle button(int index) const;
    };

    [[nodiscard]] static Layout layout(int width, int height, int count);
    static void drawBackground(int width, int height);
    static void drawHeading(UiText& text, Page page, int width, int height, int count,
                            Language language = Language::English);
    static void drawButton(UiText& text, Rectangle bounds, const char* label, Page page,
                           int index, bool enabled, bool selected, bool armed,
                           std::optional<bool> toggle = std::nullopt,
                           std::optional<BoardTheme> theme = std::nullopt,
                           std::optional<Language> languageChoice = std::nullopt,
                           Language language = Language::English);
    static void drawFooter(UiText& text, const char* label, int width, int height, int count);
    [[nodiscard]] static Color mutedColor();
};

} // namespace arrow
