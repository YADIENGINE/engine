// [recipe:TextLayout]
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
// [/recipe]
