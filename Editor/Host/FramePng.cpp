#include "Editor/Host/FramePng.h"
#include <raylib.h>
#include <algorithm>
#include <stdexcept>

namespace {
constexpr int LabelFontSize = 20;
constexpr int LabelPadding = 4;
constexpr Color LabelBackground{0, 0, 0, 190};
} // namespace

ViewportPixels ReadWindowPixels() {
    Image image = LoadImageFromScreen();
    if (!image.data) throw std::runtime_error("Could not read the editor window");
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ViewportPixels pixels;
    pixels.width = image.width;
    pixels.height = image.height;
    const auto* bytes = static_cast<const std::byte*>(image.data);
    pixels.rgba.assign(bytes,
                       bytes + static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4);
    UnloadImage(image);
    return pixels;
}
std::vector<std::byte> EncodeFramePng(const ViewportPixels& frame, const std::vector<FrameCaptureLabel>& labels) {
    Image image = GenImageColor(frame.width, frame.height, BLANK); // A copy raylib may draw into
    if (!image.data) throw std::runtime_error("PNG encoding failed");
    std::copy(frame.rgba.begin(), frame.rgba.end(), static_cast<std::byte*>(image.data));
    for (const auto& label : labels) {
        const int width = MeasureText(label.text.c_str(), LabelFontSize);
        ImageDrawRectangle(&image, label.x, label.y, width + LabelPadding * 2, LabelFontSize + LabelPadding * 2,
                           LabelBackground);
        ImageDrawText(&image, label.text.c_str(), label.x + LabelPadding, label.y + LabelPadding, LabelFontSize, WHITE);
    }
    int size = 0;
    unsigned char* encoded = ExportImageToMemory(image, ".png", &size);
    UnloadImage(image);
    if (!encoded || size <= 0) throw std::runtime_error("PNG encoding failed");
    const auto* bytes = reinterpret_cast<const std::byte*>(encoded);
    std::vector<std::byte> result(bytes, bytes + size);
    MemFree(encoded);
    return result;
}
