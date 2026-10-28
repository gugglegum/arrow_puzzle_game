#pragma once

#include "arrow/Persistence.hpp"

#include <raylib.h>

namespace arrow {

struct BoardPalette {
    Color background;
    Color surface;
    Color hud;
    Color ink;
    Color muted;
    Color border;
    Color flight;
    Color error;
    Color dot;
    Color guide;
    Color button;
    Color buttonHovered;
    Color buttonPressed;
    bool brightenGuides;
};

inline constexpr BoardPalette kLightBoardPalette{
    {245, 245, 245, 255}, {248, 250, 253, 255}, {245, 245, 245, 255},
    {22, 29, 48, 255}, {104, 115, 132, 255}, {226, 232, 241, 255},
    {16, 132, 255, 255}, {238, 67, 74, 255}, {195, 202, 214, 255},
    {148, 158, 176, 255}, {249, 250, 252, 255}, {232, 241, 253, 255},
    {218, 233, 252, 255}, false
};

inline constexpr BoardPalette kDarkBoardPalette{
    {10, 19, 34, 255}, {18, 30, 48, 255}, {14, 25, 43, 255},
    {209, 222, 240, 255}, {145, 166, 197, 255}, {45, 63, 86, 255},
    {91, 181, 255, 255}, {255, 111, 130, 255}, {55, 75, 101, 255},
    {108, 136, 170, 255}, {25, 42, 65, 255}, {34, 55, 83, 255},
    {44, 71, 104, 255}, true
};

[[nodiscard]] inline constexpr const BoardPalette& boardPalette(BoardTheme theme) {
    return theme == BoardTheme::Light ? kLightBoardPalette : kDarkBoardPalette;
}

} // namespace arrow
