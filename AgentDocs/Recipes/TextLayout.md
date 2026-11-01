Load a font, measure a label, and draw a wrapped, aligned block.

APIs: Core/Graphics/Text/FontFace.h, Core/Graphics/Text/TextBlock.h, Core/Graphics/Paint/Painter.h.

Construct once with a font asset path; retain the View instead of loading the font each frame.

<!-- recipe:TextLayout-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:TextLayout -->
```cpp
#include "AgentDocs/Recipes/Code/TextLayout.h"
#include <string_view>
#include "Graphics/Paint/Painter.h"

namespace {
constexpr std::string_view Title = "READY";
constexpr std::string_view Description = "A wrapped description for MyProject.";
constexpr TextStyle DescriptionLayout{.size = 24.0f, .maxWidth = 180.0f, .wrap = TextWrap::Word};
constexpr TextStyle DescriptionInk{.color = Rgb(0xc0d0e0)};
constexpr TextStyle TitleStyle{.size = 32.0f, .align = TextAlign::Center};
constexpr Float2 TitlePosition{240.0f, 40.0f};
constexpr Float2 DescriptionPosition{40.0f, 100.0f};
} // namespace

RecipeTextView::RecipeTextView(const std::filesystem::path& fontFile)
    : _font(FontFace::FromFile(fontFile))
    , _block(_font.Layout(Description, DescriptionLayout)) {}
Float2 RecipeTextView::Measured() const { return _font.Measure(Title, TitleStyle.size); }
void RecipeTextView::Draw(Painter& paint) const {
    paint.Text(_font, Title, TitlePosition, TitleStyle);
    paint.Text(_font, _block, DescriptionPosition, DescriptionInk);
}
```
<!-- /recipe -->

Pitfalls: TextBlock borrows its font identity: keep FontFace alive. Block layout wins over the size/wrapping in draw style. Text position is the top of the first line.

Concept: `Docs/Graphics/Text.md#drawing-and-layout`.
