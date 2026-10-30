#include "UiText.hpp"
#include "UiFontData.hpp"
#include "arrow/Localization.hpp"

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
#include <cmath>
#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace arrow {
namespace {

constexpr int kPadding = 2;
constexpr std::size_t kMaximumCachedLabels = 128;

bool containsCjk(const char* text) {
    for (const char* p = text; *p;) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(p, &bytes);
        if (codepoint >= 0x2E80) return true;
        p += bytes;
    }
    return false;
}

std::vector<int> codepointsForFont(bool cjk) {
    std::vector<int> result;
    for (int cp = 32; cp < 127; ++cp) result.push_back(cp);
    const auto append = [&](const char* label) {
        for (const char* p = label; *p;) {
            int bytes = 0;
            const int cp = GetCodepointNext(p, &bytes);
            if ((cjk && cp >= 0x2E80) || (!cjk && cp < 0x2E80)) result.push_back(cp);
            p += bytes;
        }
    };
    for (int l = 1; l < static_cast<int>(Language::Count); ++l) {
        const auto language = static_cast<Language>(l);
        append(languageName(language));
        for (int t = 0; t < static_cast<int>(Text::Count); ++t) append(translate(static_cast<Text>(t), language));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

// An alpha-only font atlas discards ClearType's three independent coverages.
// Multiply the destination by the native RGB mask, then add the covered ink.
// This also works over gradients, translucent panels, and arbitrary gameplay;
// there is no baked background rectangle and no extra screen-sized texture.
constexpr const char* kCoverageShader = R"glsl(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform int inkPass;
out vec4 finalColor;
void main() {
    vec3 mask = texture(texture0, fragTexCoord).rgb;
    vec3 value = (inkPass == 0)
        ? mix(vec3(1.0), mask, fragColor.a)
        : (vec3(1.0) - mask) * fragColor.rgb * fragColor.a;
    finalColor = vec4(value, 0.0);
}
)glsl";

} // namespace

struct UiText::Impl {
    struct Label {
        std::string text;
        int size;
        float spacing;
        Vector2 extent;
        Texture2D texture;

        std::size_t bytes() const {
            return static_cast<std::size_t>(texture.width) * texture.height * 4;
        }
    };

    std::list<Label> labels;
    std::size_t textureBytes{};
    std::map<std::pair<int, bool>, Font> fallbackFonts;
    Shader shader{};
    int inkPassLocation{-1};
#if defined(_WIN32)
    HANDLE fontResource{};
    HANDLE cjkFontResource{};
    HDC dc{};
    std::map<std::pair<int, bool>, HFONT> fonts;
    DWORD quality{DEFAULT_QUALITY};

    HFONT fontForSize(int size, bool cjk) {
        const auto key = std::pair{size, cjk};
        if (const auto found = fonts.find(key); found != fonts.end()) {
            return found->second;
        }
        // Positive height requests the full character cell, matching the UI's
        // pixel-height layout rather than enlarging it to an em-sized cell.
        HFONT font = CreateFontW(
            size, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quality,
            DEFAULT_PITCH, cjk ? L"Arrow Puzzle CJK" : L"Inter Medium"
        );
        fonts.emplace(key, font);
        return font;
    }

    Label rasterize(const char* text, int size, float spacing) {
        Label label{text, size, spacing, {}, {}};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (length <= 1) return label;
        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), length);
        wide.pop_back();
        HFONT font = fontForSize(size, containsCjk(text));
        if (font == nullptr) return label;
        const HGDIOBJ oldFont = SelectObject(dc, font);

        SIZE extent{};
        std::vector<int> advances(wide.size());
        const int count = static_cast<int>(wide.size());
        if (!GetTextExtentExPointW(dc, wide.data(), count, 0, nullptr,
                                  advances.data(), &extent)) {
            SelectObject(dc, oldFont);
            return label;
        }
        int previous = 0;
        for (int index = 0; index < count; ++index) {
            const int next = static_cast<int>(std::lround(
                static_cast<float>(advances[index]) + spacing *
                    static_cast<float>(std::min(index + 1, count - 1))
            ));
            advances[index] = next - previous;
            previous = next;
        }
        label.extent = {static_cast<float>(previous), static_cast<float>(extent.cy)};
        const int width = previous + 2 * kPadding;
        const int height = extent.cy + 2 * kPadding;
        if (width <= 0 || width > 8192 || height <= 0 || height > 8192) {
            SelectObject(dc, oldFont);
            return label;
        }
        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = -height; // Top-down DIB.
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS,
                                         &pixels, nullptr, 0);
        if (bitmap != nullptr && pixels != nullptr) {
            const HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
            const std::size_t area = static_cast<std::size_t>(width) * height;
            auto* source = static_cast<std::uint32_t*>(pixels);
            std::fill_n(source, area, 0x00ffffffU);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(0, 0, 0));
            SetBkColor(dc, RGB(255, 255, 255));
            const bool rendered = ExtTextOutW(
                dc, kPadding, kPadding, 0, nullptr, wide.data(),
                static_cast<UINT>(count), advances.data()
            ) != FALSE;
            GdiFlush(); // Complete GDI's batch before reading DIB memory.
            if (rendered) {
                std::vector<Color> rgba(area);
                for (std::size_t index = 0; index < area; ++index) {
                    const std::uint32_t pixel = source[index];
                    rgba[index] = {
                        static_cast<unsigned char>(pixel >> 16),
                        static_cast<unsigned char>(pixel >> 8),
                        static_cast<unsigned char>(pixel), 255
                    };
                }
                label.texture = LoadTextureFromImage({
                    rgba.data(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
                });
                SetTextureFilter(label.texture, TEXTURE_FILTER_POINT);
            }
            SelectObject(dc, oldBitmap);
        }
        if (bitmap != nullptr) DeleteObject(bitmap);
        SelectObject(dc, oldFont);
        return label;
    }
#endif

    Impl() {
#if defined(_WIN32)
        DWORD count = 0;
        fontResource = AddFontMemResourceEx(
            const_cast<unsigned char*>(assets::kUiFontData),
            static_cast<DWORD>(assets::kUiFontDataSize), nullptr, &count
        );
        cjkFontResource = AddFontMemResourceEx(
            const_cast<unsigned char*>(assets::kCjkFontData),
            static_cast<DWORD>(assets::kCjkFontDataSize), nullptr, &count
        );
        dc = CreateCompatibleDC(nullptr);
        BOOL smoothing = FALSE;
        UINT smoothingType = 0;
        if (SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0)) {
            quality = NONANTIALIASED_QUALITY;
            if (smoothing) {
                quality = ANTIALIASED_QUALITY;
                if (SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0,
                                         &smoothingType, 0) &&
                    smoothingType == FE_FONTSMOOTHINGCLEARTYPE) {
                    quality = CLEARTYPE_QUALITY;
                }
            }
        }
        if (fontResource != nullptr && dc != nullptr) {
            shader = LoadShaderFromMemory(nullptr, kCoverageShader);
            inkPassLocation = GetShaderLocation(shader, "inkPass");
        }
        if (native()) {
            TraceLog(LOG_INFO, "UI: Native Windows font smoothing (quality=%u)", quality);
        } else {
            TraceLog(LOG_WARNING, "UI: Native text unavailable; using size-matched font atlas");
        }
#endif
    }

    bool native() const {
#if defined(_WIN32)
        return fontResource != nullptr && cjkFontResource != nullptr && dc != nullptr && inkPassLocation >= 0;
#else
        return false;
#endif
    }

    Font fallback(int size, bool cjk) {
        const auto key = std::pair{size, cjk};
        if (const auto found = fallbackFonts.find(key); found != fallbackFonts.end()) {
            return found->second;
        }
        static auto latinCodepoints = codepointsForFont(false);
        static auto cjkCodepoints = codepointsForFont(true);
        auto& codepoints = cjk ? cjkCodepoints : latinCodepoints;
        Font font = LoadFontFromMemory(
            ".ttf", cjk ? assets::kCjkFontData : assets::kUiFontData,
            static_cast<int>(cjk ? assets::kCjkFontDataSize : assets::kUiFontDataSize),
            size, codepoints.data(), static_cast<int>(codepoints.size())
        );
        SetTextureFilter(font.texture, TEXTURE_FILTER_POINT);
        fallbackFonts.emplace(key, font);
        return font;
    }

    Label* label(const char* text, int size, float spacing) {
        const auto found = std::find_if(labels.begin(), labels.end(), [&](const Label& item) {
            return item.size == size && item.spacing == spacing && item.text == text;
        });
        if (found != labels.end()) {
            labels.splice(labels.begin(), labels, found);
            return &labels.front();
        }
#if defined(_WIN32)
        Label value = rasterize(text, size, spacing);
        if (value.texture.id == 0) return nullptr;
        const std::size_t bytes = value.bytes();
        if (bytes > kMaximumCachedBytes) {
            UnloadTexture(value.texture);
            return nullptr;
        }
        while (!labels.empty() &&
               (textureBytes + bytes > kMaximumCachedBytes ||
                labels.size() >= kMaximumCachedLabels)) {
            textureBytes -= labels.back().bytes();
            UnloadTexture(labels.back().texture);
            labels.pop_back();
        }
        textureBytes += bytes;
        labels.push_front(std::move(value));
        return &labels.front();
#else
        return nullptr;
#endif
    }

    void release() {
        for (const Label& item : labels) UnloadTexture(item.texture);
        labels.clear();
        textureBytes = 0;
        if (inkPassLocation >= 0) UnloadShader(shader);
        shader = {};
        inkPassLocation = -1;
        for (const auto& [size, font] : fallbackFonts) {
            if (font.texture.id != GetFontDefault().texture.id) UnloadFont(font);
        }
        fallbackFonts.clear();
#if defined(_WIN32)
        if (dc != nullptr) DeleteDC(dc);
        dc = nullptr;
        for (const auto& [size, font] : fonts) {
            if (font != nullptr) DeleteObject(font);
        }
        fonts.clear();
        if (fontResource != nullptr) RemoveFontMemResourceEx(fontResource);
        fontResource = nullptr;
        if (cjkFontResource != nullptr) RemoveFontMemResourceEx(cjkFontResource);
        cjkFontResource = nullptr;
#endif
    }
};

UiText::UiText() : impl_(std::make_unique<Impl>()) {}
UiText::~UiText() { release(); }

Vector2 UiText::measure(const char* text, float size, float spacing) {
    const int pixels = std::max(1, static_cast<int>(std::lround(size)));
    if (impl_->native()) {
        if (const auto* label = impl_->label(text, pixels, spacing)) return label->extent;
    }
    return MeasureTextEx(impl_->fallback(pixels, containsCjk(text)), text, size, spacing);
}

float UiText::fitSize(const char* text, float size, float spacing, float width) {
    // Integer sizes preserve the raster-at-display-size contract on both backends.
    size = std::max(1.0F, std::floor(size));
    while (size > 1.0F) {
        const float measured = measure(text, size, spacing).x;
        if (measured <= width) break;
        // Jump near the fitting size: loading every intermediate size would
        // leave many unnecessary Linux font atlases alive after a long title.
        size = std::max(1.0F, std::min(size - 1.0F, std::floor(size * width / measured)));
    }
    return size;
}

void UiText::draw(const char* text, Vector2 position, float size, float spacing, Color color) {
    const int pixels = std::max(1, static_cast<int>(std::lround(size)));
    position = {std::round(position.x), std::round(position.y)};
    if (impl_->native()) {
        if (const auto* label = impl_->label(text, pixels, spacing)) {
            const int x = static_cast<int>(position.x) - kPadding;
            const int y = static_cast<int>(position.y) - kPadding;
            BeginShaderMode(impl_->shader);
            const int maskPass = 0;
            SetShaderValue(impl_->shader, impl_->inkPassLocation, &maskPass, SHADER_UNIFORM_INT);
            rlSetBlendFactorsSeparate(RL_ZERO, RL_SRC_COLOR, RL_ZERO, RL_ONE,
                                      RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            DrawTexture(label->texture, x, y, {255, 255, 255, color.a});
            // Uniforms are not part of raylib's draw batch state.
            rlDrawRenderBatchActive();
            const int inkPass = 1;
            SetShaderValue(impl_->shader, impl_->inkPassLocation, &inkPass, SHADER_UNIFORM_INT);
            rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE,
                                      RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            DrawTexture(label->texture, x, y, color);
            EndBlendMode();
            EndShaderMode();
            return;
        }
    }
    DrawTextEx(impl_->fallback(pixels, containsCjk(text)), text, position, size, spacing, color);
}

void UiText::release() { impl_->release(); }
bool UiText::usesWindowsSmoothing() const { return impl_->native(); }
std::size_t UiText::cachedBytes() const { return impl_->textureBytes; }

} // namespace arrow
