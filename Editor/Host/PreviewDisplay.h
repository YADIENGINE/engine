#pragma once
#include <raylib.h>
#include <cstddef>
#include <span>
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"

class PreviewDisplay {
public:
    PreviewDisplay();
    ~PreviewDisplay();
    [[nodiscard]] bool Upload(int width, int height, std::span<const std::byte> pixels, std::uint64_t generation,
                              std::uint64_t sequence, double transferMs);
    [[nodiscard]] EditorPreviewImage Image() const;

private:
    Texture2D _texture{};
    EditorPreviewImage _image;

public:
    PreviewDisplay(const PreviewDisplay&) = delete;
    PreviewDisplay& operator=(const PreviewDisplay&) = delete;
};
