// [recipe:TextLayout-header]
#pragma once
#include "Graphics/Text/FontFace.h"

class Painter;

class RecipeTextView {
public:
    explicit RecipeTextView(const std::filesystem::path& fontFile);
    void Draw(Painter& paint) const;
    [[nodiscard]] Float2 Measured() const;

private:
    FontFace _font;
    TextBlock _block;
};
// [/recipe]
