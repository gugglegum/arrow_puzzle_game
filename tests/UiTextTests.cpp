#include "UiText.hpp"
#include "UiFontData.hpp"
#include "MenuScreen.hpp"
#include "EndScreen.hpp"
#include "arrow/Persistence.hpp"
#include <rlgl.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define Rectangle WinRectangle
#include <windows.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 400;
constexpr int kHeight = 64;
constexpr const char* kText = "Arrow Puzzle 0123456789";

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::vector<Color> capture(arrow::UiText& text, RenderTexture2D target,
                           Color background, Color ink, int size = 20) {
    BeginTextureMode(target);
    ClearBackground(background);
    text.draw(kText, {8.0F, 8.0F}, static_cast<float>(size), 0.0F, ink);
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Color* pixels = LoadImageColors(image);
    std::vector<Color> result(pixels, pixels + kWidth * kHeight);
    UnloadImageColors(pixels);
    UnloadImage(image);
    return result;
}

void checkGlyphsAndLayout(arrow::UiText& text, RenderTexture2D target) {
    for (const int size : {15, 17, 18, 19, 20, 22, 24, 25, 26, 34}) {
        const Vector2 extent = text.measure(kText, static_cast<float>(size), 0.0F);
        require(std::isfinite(extent.x) && std::isfinite(extent.y) &&
                extent.x > 50 && extent.x < kWidth - 16 && extent.y > 0 && extent.y < kHeight - 16,
                "Text has invalid or clipped layout metrics");
        const Vector2 spaced = text.measure(kText, static_cast<float>(size), 1.0F);
        require(spaced.x > extent.x && spaced.y == extent.y, "Letter spacing did not affect layout");
        const auto pixels = capture(text, target, WHITE, BLACK, size);
        int ink = 0;
        int edges = 0;
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const Color pixel = pixels[y * kWidth + x];
                if (pixel.r < 250 || pixel.g < 250 || pixel.b < 250) {
                    ++ink;
                    require(x > 0 && x < kWidth - 1 && y > 0 && y < kHeight - 1,
                            "Glyphs reached the capture boundary");
                    edges += pixel.r > 0 && pixel.r < 250;
                }
            }
        }
        require(ink > 50, "Text is empty");
#if !defined(_WIN32)
        require(edges > 10, "Font atlas has no antialiased edges");
#endif
    }
    std::cout << "Nonempty antialiased glyphs and layout checked at ten UI sizes\n";
}

void checkTranslationGlyphs() {
    std::array<std::vector<int>, 2> codepoints;
    const auto append = [&](const char* label) {
        for (const char* p = label; *p;) {
            int bytes = 0;
            const int cp = GetCodepointNext(p, &bytes);
            require(cp != 0xFFFD && (cp != '?' || *p == '?'), "Invalid UTF-8 in catalog");
            codepoints[cp >= 0x2E80 ? 1 : 0].push_back(cp);
            p += bytes;
        }
    };
    for (int l = 0; l < static_cast<int>(arrow::Language::Count); ++l) {
        const auto language = static_cast<arrow::Language>(l);
        append(arrow::languageName(language));
        for (int t = 0; t < static_cast<int>(arrow::Text::Count); ++t)
            append(arrow::translate(static_cast<arrow::Text>(t), language));
    }
    for (int cjk = 0; cjk < 2; ++cjk) {
        auto& points = codepoints[cjk];
        std::sort(points.begin(), points.end());
        points.erase(std::unique(points.begin(), points.end()), points.end());
        auto* glyphs = LoadFontData(cjk ? arrow::assets::kCjkFontData : arrow::assets::kUiFontData,
            static_cast<int>(cjk ? arrow::assets::kCjkFontDataSize : arrow::assets::kUiFontDataSize),
            24, points.data(), static_cast<int>(points.size()), FONT_DEFAULT);
        require(glyphs != nullptr, "Embedded font did not load");
        bool complete = true;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (glyphs[i].image.data == nullptr) {
                std::cerr << "Missing embedded glyph U+" << std::hex << points[i] << std::dec << '\n';
                complete = false;
            }
        }
        UnloadFontData(glyphs, static_cast<int>(points.size()));
        require(complete, "Embedded fonts do not cover every translation");
    }
    std::cout << "All translated characters are present in the two embedded fonts\n";
}

void checkTranslatedScreens(arrow::UiText& text, bool screenshots) {
    using arrow::Text;
    using arrow::MenuScreen;
    constexpr std::array main{Text::NewGame, Text::ResumeGame, Text::Settings, Text::Exit};
    constexpr std::array difficulty{Text::Simple, Text::Middle, Text::Hard, Text::VeryHard, Text::Back};
    constexpr std::array settings{Text::ArrowLoops, Text::FlyingCollisions, Text::CrosshairGuides,
        Text::Sounds, Text::LeftButtonDrag, Text::BoardTheme, Text::InterfaceLanguage, Text::Back};
    for (int width : {720, 1100}) {
        const int height = width == 720 ? 600 : 900;
        SetWindowSize(width, height);
        PollInputEvents();
        for (int l = 1; l < static_cast<int>(arrow::Language::Count); ++l) {
            const auto language = static_cast<arrow::Language>(l);
            for (const auto page : {MenuScreen::Page::Main, MenuScreen::Page::Difficulty, MenuScreen::Page::Settings}) {
                const int count = page == MenuScreen::Page::Main ? 4 : page == MenuScreen::Page::Difficulty ? 5 : 8;
                const auto layout = MenuScreen::layout(width, height, count);
                BeginDrawing();
                MenuScreen::drawBackground(width, height);
                MenuScreen::drawHeading(text, page, width, height, count, language);
                for (int i = 0; i < count; ++i) {
                    const auto bounds = layout.button(i);
                    require(bounds.y >= layout.panel.y + 120 && bounds.y + bounds.height < layout.panel.y + layout.panel.height - 35,
                            "Translated settings rows overlap heading or footer");
                    const auto id = page == MenuScreen::Page::Main ? main[i] : page == MenuScreen::Page::Difficulty ? difficulty[i] : settings[i];
                    const char* label = arrow::translate(id, language);
                    const auto size = text.fitSize(label, 19, 0.2F, bounds.width - 200);
                    require(text.measure(label, size, 0.2F).x <= bounds.width - 200, "Localized title does not fit");
                    MenuScreen::drawButton(text, bounds, label, page, i, i != 1 || page != MenuScreen::Page::Main,
                        false, false,
                        page == MenuScreen::Page::Settings && i < 5 ? std::optional<bool>{i != 1} : std::nullopt,
                        page == MenuScreen::Page::Settings && i == 5 ? std::optional{arrow::BoardTheme::Dark} : std::nullopt,
                        page == MenuScreen::Page::Settings && i == 6 ? std::optional{language} : std::nullopt, language);
                }
                MenuScreen::drawFooter(text, arrow::translate(page == MenuScreen::Page::Main ? Text::MenuHint :
                    page == MenuScreen::Page::Difficulty ? Text::DifficultyHint : Text::SettingsHint, language), width, height, count);
                if (screenshots) {
                    const std::string filename = std::string(page == MenuScreen::Page::Main ? "main-" :
                        page == MenuScreen::Page::Difficulty ? "difficulty-" : "settings-") +
                        arrow::languageCode(language) + "-" + std::to_string(width) + ".png";
                    rlDrawRenderBatchActive();
                    TakeScreenshot(filename.c_str());
                }
                EndDrawing();
            }
            for (const bool won : {false, true}) {
                arrow::EndScreen screen;
                screen.show(won ? arrow::EndScreen::Outcome::Victory : arrow::EndScreen::Outcome::Defeat);
                for (int i = 0; i < 500; ++i) screen.update(1.0F / 60.0F);
                BeginDrawing();
                ClearBackground(BLACK);
                screen.draw(text, width, height, 82, std::nullopt, std::nullopt, language);
                if (screenshots) {
                    const std::string filename = std::string(won ? "victory-" : "defeat-") +
                        arrow::languageCode(language) + "-" + std::to_string(width) + ".png";
                    rlDrawRenderBatchActive();
                    TakeScreenshot(filename.c_str());
                }
                EndDrawing();
            }
        }
    }
    std::cout << "All eight languages rendered at 720x600 and 1100x900\n";
}

#if defined(_WIN32)
// Independently render with Windows TextOut, without a GPU or atlas, to check
// that native subpixel values survive texture upload and both blending passes.
void checkNativePixels(const std::vector<Color>& actual, int size) {
    HDC dc = CreateCompatibleDC(nullptr);
    require(dc != nullptr, "Could not create reference DC");
    BOOL smoothing = FALSE;
    UINT type = 0;
    SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0);
    SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0);
    const DWORD quality = !smoothing ? NONANTIALIASED_QUALITY
        : type == FE_FONTSMOOTHINGCLEARTYPE ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY;
    HFONT font = CreateFontW(size, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quality,
        DEFAULT_PITCH, L"Inter Medium");
    const HGDIOBJ oldFont = SelectObject(dc, font);
    wchar_t face[LF_FACESIZE]{};
    GetTextFaceW(dc, LF_FACESIZE, face);
    require(std::wstring(face) == L"Inter Medium", "Embedded Inter font was substituted");
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kWidth;
    info.bmiHeader.biHeight = -kHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* memory = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
    require(bitmap != nullptr && memory != nullptr, "Could not create reference DIB");
    const HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    auto* pixels = static_cast<std::uint32_t*>(memory);
    std::fill_n(pixels, kWidth * kHeight, 0x00ffffffU);
    SetTextColor(dc, RGB(0, 0, 0));
    SetBkColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    const std::wstring label = L"Arrow Puzzle 0123456789";
    TextOutW(dc, 8, 8, label.data(), static_cast<int>(label.size()));
    GdiFlush();
    int inkPixels = 0;
    int rgbEdges = 0;
    for (std::size_t index = 0; index < actual.size(); ++index) {
        const int red = (pixels[index] >> 16) & 255;
        const int green = (pixels[index] >> 8) & 255;
        const int blue = pixels[index] & 255;
        require(std::abs(actual[index].r - red) <= 1 &&
                std::abs(actual[index].g - green) <= 1 &&
                std::abs(actual[index].b - blue) <= 1,
                "GPU text differs from native Windows glyph pixels");
        if (red < 255 || green < 255 || blue < 255) ++inkPixels;
        if (red != green || green != blue) ++rgbEdges;
    }
    require(inkPixels > 50, "Native text is missing");
    std::cout << "Native text at " << size << "px: " << inkPixels
              << " ink pixels, " << rgbEdges << " subpixel edges\n";
    SelectObject(dc, oldBitmap);
    SelectObject(dc, oldFont);
    DeleteObject(bitmap);
    DeleteObject(font);
    DeleteDC(dc);
}
#endif

void checkComposition(arrow::UiText& text, RenderTexture2D target) {
    const auto mask = capture(text, target, WHITE, BLACK);
    const Color background{73, 119, 161, 127};
    for (const unsigned char alpha : std::array<unsigned char, 2>{255, 128}) {
        const Color ink{210, 43, 92, alpha};
        const auto actual = capture(text, target, background, ink);
        for (std::size_t index = 0; index < actual.size(); ++index) {
            const std::array<int, 3> transmittance{mask[index].r, mask[index].g, mask[index].b};
            const std::array<int, 3> back{background.r, background.g, background.b};
            const std::array<int, 3> front{ink.r, ink.g, ink.b};
            const std::array<int, 3> pixel{actual[index].r, actual[index].g, actual[index].b};
            for (int channel = 0; channel < 3; ++channel) {
                const double coverage = (255 - transmittance[channel]) / 255.0 * alpha / 255.0;
                const double expected = back[channel] * (1.0 - coverage) + front[channel] * coverage;
                require(std::abs(pixel[channel] - expected) <= 2.0,
                        "ClearType RGB coverage or ink opacity was lost");
            }
            require(actual[index].a == background.a, "Text changed destination alpha");
        }
    }
}

void checkCache(arrow::UiText& text, RenderTexture2D target) {
    const auto before = capture(text, target, WHITE, BLACK);
    for (int index = 0; index < 2000; ++index) {
        const std::string label = "ARROWS " + std::to_string(index);
        text.measure(label.c_str(), 19.0F, 0.4F);
        require(text.cachedBytes() <= arrow::UiText::kMaximumCachedBytes,
                "Text cache exceeds its memory budget");
    }
    const auto after = capture(text, target, WHITE, BLACK);
    require(before.size() == after.size(), "Capture size changed");
    for (std::size_t index = 0; index < before.size(); ++index) {
        require(ColorToInt(before[index]) == ColorToInt(after[index]),
                "Text changed after cache eviction");
    }
    std::cout << "Text cache after 2000 counter changes: " << text.cachedBytes() << " bytes\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        std::filesystem::create_directories(argv[1]);
        std::filesystem::current_path(argv[1]);
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(kWidth, kHeight, "Arrow Puzzle native text tests");
    const RenderTexture2D target = LoadRenderTexture(kWidth, kHeight);
    int result = 0;
    {
        arrow::UiText text;
        try {
            checkGlyphsAndLayout(text, target);
            checkTranslationGlyphs();
#if defined(_WIN32)
            require(text.usesWindowsSmoothing(), "Windows text backend is unavailable");
            for (const int size : {15, 17, 18, 19, 20, 22, 24, 25, 26, 34}) {
                checkNativePixels(capture(text, target, WHITE, BLACK, size), size);
            }
            checkComposition(text, target);
#endif
            checkCache(text, target);
            checkTranslatedScreens(text, argc == 2);
            text.release();
            require(text.cachedBytes() == 0, "Text cache was not released");
            std::cout << "Native UI text checks passed\n";
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            result = 1;
        }
    }
    UnloadRenderTexture(target);
    CloseWindow();
    return result;
}
