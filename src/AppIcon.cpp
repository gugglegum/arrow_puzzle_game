#include "AppIcon.hpp"
#include "AppIconData.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arrow {

void setApplicationIcon() {
    struct EncodedImage {
        const unsigned char* data;
        std::size_t size;
    };
    const std::array<EncodedImage, 7> encoded{{
        {assets::kAppIcon16Png, assets::kAppIcon16PngSize},
        {assets::kAppIcon24Png, assets::kAppIcon24PngSize},
        {assets::kAppIcon32Png, assets::kAppIcon32PngSize},
        {assets::kAppIcon48Png, assets::kAppIcon48PngSize},
        {assets::kAppIcon64Png, assets::kAppIcon64PngSize},
        {assets::kAppIcon128Png, assets::kAppIcon128PngSize},
        {assets::kAppIcon256Png, assets::kAppIcon256PngSize},
    }};
    std::array<Image, encoded.size()> images{};
    int count = 0;
    for (const EncodedImage& source : encoded) {
        Image image = LoadImageFromMemory(".png", source.data, static_cast<int>(source.size));
        if (image.data == nullptr) {
            TraceLog(LOG_WARNING, "APP: Could not decode embedded application icon");
            continue;
        }
        // SetWindowIcons requires RGBA8 even when a PNG decoder returns RGB.
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        images[count++] = image;
    }
    if (count > 0) SetWindowIcons(images.data(), count);
    // The window system copies the pixels synchronously.
    for (int index = 0; index < count; ++index) UnloadImage(images[index]);
}

} // namespace arrow
