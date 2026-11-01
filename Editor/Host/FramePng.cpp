#include "Editor/Host/FramePng.h"
#include <raylib.h>
#include <stdexcept>

std::vector<std::byte> EncodeFramePng(const PreviewPixels& frame) {
    // raylib reads the pixels without taking ownership; the cast only satisfies its non-const field.
    const Image image{const_cast<std::byte*>(frame.rgba.data()), frame.width, frame.height, 1,
                      PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    int size = 0;
    unsigned char* encoded = ExportImageToMemory(image, ".png", &size);
    if (!encoded || size <= 0) throw std::runtime_error("PNG encoding failed");
    const auto* bytes = reinterpret_cast<const std::byte*>(encoded);
    std::vector<std::byte> result(bytes, bytes + size);
    MemFree(encoded);
    return result;
}
