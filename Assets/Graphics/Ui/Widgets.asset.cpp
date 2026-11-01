#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Ui/UiDraw.h"

namespace {
constexpr float WideShare = 0.32f;  // A wide widget's height, of the box
constexpr float LabelShare = 0.11f; // Label size, of the box
constexpr float NotchShare = 0.06f;
constexpr float LineShare = 0.012f;
constexpr float BarShare = 0.08f;
constexpr float BarFill = 0.65f;
enum class Widget { Panel, Frame, Button, RoundButton, Chip, KeyCap, ProgressBar };

FloatRect Wide(const FloatRect& box) {
    return FloatRect::FromCenter(box.Center(), {box.width, box.height * WideShare});
}

GraphicAsset MakeWidgets() {
    GraphicAsset asset;
    asset.items = {{"Panel", "ui"},      {"Frame", "ui"},  {"Button", "ui"},     {"RoundButton", "ui"},
                   {"Chip", "ui label"}, {"KeyCap", "ui"}, {"ProgressBar", "ui"}};
    asset.usage = "DrawUi{item}(paint, ...);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float) {
        const Color4 accent = CurrentTheme().palette.accent;
        const float label = box.height * LabelShare;
        switch (static_cast<Widget>(item)) {
        case Widget::Panel : DrawUiPanel(paint, box, accent); break;
        case Widget::Frame : DrawUiFrame(paint, box, box.width * NotchShare, box.width * LineShare, accent); break;
        case Widget::Button: {
            ButtonLook look;
            look.accent = accent;
            DrawUiButton(paint, Wide(box), "Button", label, look);
            break;
        }
        case Widget::RoundButton: DrawUiRoundButton(paint, Wide(box), "Round", label, accent); break;
        case Widget::Chip       : DrawUiChip(paint, "Chip", box.Center(), label, accent); break;
        case Widget::KeyCap     : DrawUiKeyCap(paint, Wide(box), "E", label); break;
        case Widget::ProgressBar: {
            ProgressBarLook look;
            look.fill = BarFill;
            look.color = accent;
            DrawUiProgressBar(paint, FloatRect::FromCenter(box.Center(), {box.width, box.height * BarShare}), look);
            break;
        }
        }
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeWidgets);
} // namespace
