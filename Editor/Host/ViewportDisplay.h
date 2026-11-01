#pragma once
#include <raylib.h>
#include <cstddef>
#include <span>
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"

class ViewportDisplay {
public:
    ViewportDisplay();
    ~ViewportDisplay();
    [[nodiscard]] bool Upload(int width, int height, std::span<const std::byte> pixels, std::uint64_t generation,
                              std::uint64_t sequence, double transferMs);
    [[nodiscard]] ViewportImage Image() const;

private:
    Texture2D _texture{};
    ViewportImage _image;

public:
    ViewportDisplay(const ViewportDisplay&) = delete;
    ViewportDisplay& operator=(const ViewportDisplay&) = delete;
};
