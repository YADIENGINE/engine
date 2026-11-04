#include "Ui/Looks/Neon/NeonLooks.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"
#include "Ui/Looks/UiButtonLook.h"
#include "Ui/Looks/UiLabelLook.h"
#include "Ui/Looks/UiLayerLook.h"
#include "Ui/Looks/UiPanelLook.h"
#include "Ui/Looks/UiScrollLook.h"
#include "Ui/Looks/UiSliderLook.h"
#include "Ui/Looks/UiStepperLook.h"
#include "Ui/Looks/UiTabLook.h"
#include "Ui/Looks/UiToggleLook.h"
#include "Ui/Looks/UiTooltipLook.h"

namespace {
constexpr NeonFrameStyle Frame{.outerWidth = 4.5f,
                               .innerWidth = 2.2f,
                               .outerAlpha = 0.12f,
                               .innerAlpha = 0.28f,
                               .coreWidth = 0.4f,
                               .coreMinWidth = 1.0f,
                               .coreWhiten = 0.6f};
constexpr NeonTextStyle Text{.haloRadius = 5.0f, .haloAlpha = 0.45f, .coreWhiten = 0.2f};
constexpr Color4 ButtonBody{24, 19, 50, 255};
constexpr Color4 ButtonShadow{5, 3, 14, 255};

constexpr float LabelGlow = 0.6f;
constexpr float DisabledOpacity = 0.45f;
constexpr float RowNotch = 8.0f;
constexpr float RowTint = 0.3f;
constexpr float RowFrameWidth = 2.0f;
constexpr float RowLabelInset = 16.0f;
constexpr float SwitchHeight = 28.0f;
constexpr float SwitchNotch = 6.0f;
constexpr float KnobInset = 4.0f;
constexpr float SliderHeight = 14.0f;
constexpr float ValueTextScale = 0.85f;
constexpr float ArrowSize = 8.0f;
constexpr float ArrowInset = 6.0f;

void DrawLabel(Painter& paint, const UiLabelView& view) {
    if (!view.state.font) return;
    const Color4 color = view.color.value_or(CurrentTheme().palette.text);
    const TextStyle style = UiLabelTextStyle(view, color);
    const FloatRect& rect = view.state.rect;
    const float x = view.align == TextAlign::Left     ? rect.x
                    : view.align == TextAlign::Center ? rect.Center().x
                                                      : rect.Right();
    DrawNeonText(paint, view.text, {x, rect.y}, style, LabelGlow, NeonUiStyles().text);
}

void DrawButton(Painter& paint, const UiButtonView& view) {
    auto faded = paint.Opacity(view.state.enabled ? 1.0f : DisabledOpacity);
    const Color4 accent = view.accent.value_or(CurrentTheme().palette.accent);
    const ButtonLook look{.animation = view.animation, .accent = accent};
    DrawArcadeButton(paint, view.state.rect, view.label, view.textSize, look, static_cast<float>(paint.Time()),
                     NeonUiStyles().button);
}

void DrawPanel(Painter& paint, const UiPanelView& view) {
    DrawNeonPanel(paint, view.state.rect, view.accent.value_or(CurrentTheme().palette.accent), NeonUiStyles().panel);
}

// A row with a label on the left, for a control on the right.
void DrawControlRow(Painter& paint, const UiControlView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.accent.value_or(palette.accent);
    const float highlight = std::clamp(view.animation.highlight, 0.0f, 1.0f);
    const FloatRect rect = view.state.rect;
    paint.ChamferRect(rect, RowNotch, Fill(Mix(ButtonBody, accent, highlight * RowTint)));
    const Color4 frame = Mix(NeonUiStyles().button.frameColor, accent, highlight);
    DrawNeonFrame(paint, rect, RowNotch, RowFrameWidth, frame, highlight, Frame);
    if (!view.state.font) return;
    const float lineHeight = view.state.font->LineHeight(view.textSize);
    const float room = std::max(view.area.x - rect.x - 2.0f * RowLabelInset, 1.0f);
    DrawNeonText(paint, view.label, {rect.x + RowLabelInset, rect.Center().y - lineHeight * 0.5f},
                 FittedText(view.textSize, palette.textSoft, room), highlight, Text);
}

void DrawToggle(Painter& paint, const UiToggleView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawControlRow(paint, view.control);
    const Color4 accent = view.control.accent.value_or(CurrentTheme().palette.accent);
    const FloatRect track = FloatRect::FromCenter(view.control.area.Center(), {view.control.area.width, SwitchHeight});
    const float on = std::clamp(view.on, 0.0f, 1.0f);
    paint.ChamferRect(track, SwitchNotch, Fill(NeonUiStyles().progressBar.track));
    const Color4 frame = Mix(NeonUiStyles().button.frameColor, accent, on);
    DrawNeonFrame(paint, track, SwitchNotch, RowFrameWidth, frame, on, Frame);
    const float knobWidth = track.width * 0.5f - KnobInset;
    const FloatRect knob{track.x + KnobInset + (track.width - 2.0f * KnobInset - knobWidth) * on, track.y + KnobInset,
                         knobWidth, track.height - 2.0f * KnobInset};
    paint.ChamferRect(knob, SwitchNotch * 0.5f, Fill(frame));
}

void DrawSlider(Painter& paint, const UiSliderView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawControlRow(paint, view.control);
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.control.accent.value_or(palette.accent);
    const FloatRect bar = FloatRect::FromCenter(view.track.Center(), {view.track.width, SliderHeight});
    DrawProgressBar(paint, bar, {.fill = view.fraction, .color = accent}, NeonUiStyles().progressBar);
    const UiDrawState& state = view.control.state;
    if (!state.font) return;
    const float size = view.control.textSize * ValueTextScale;
    const Float2 at{view.control.area.Right(), view.track.Center().y - state.font->LineHeight(size) * 0.5f};
    const TextStyle number{.size = size, .color = palette.textSoft, .align = TextAlign::Right};
    DrawNeonText(paint, view.valueText, at, number, 0.0f, Text);
}

void DrawStepper(Painter& paint, const UiStepperView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawControlRow(paint, view.control);
    const ThemePalette& palette = CurrentTheme().palette;
    const UiDrawState& state = view.control.state;
    const FloatRect area = view.control.area;
    const float middle = area.Center().y;
    const Color4 arrow = state.focused ? view.control.accent.value_or(palette.accent) : palette.textDim;
    const float left = area.x + ArrowInset;
    const float right = area.Right() - ArrowInset;
    paint.Triangle({left, middle}, {left + ArrowSize, middle - ArrowSize}, {left + ArrowSize, middle + ArrowSize},
                   Fill(arrow));
    paint.Triangle({right, middle}, {right - ArrowSize, middle + ArrowSize}, {right - ArrowSize, middle - ArrowSize},
                   Fill(arrow));
    if (!state.font) return;
    const float size = view.control.textSize;
    const Float2 at{area.Center().x, middle - state.font->LineHeight(size) * 0.5f};
    const float room = area.width - 4.0f * (ArrowInset + ArrowSize);
    DrawNeonText(paint, view.valueText, at, CenteredText(size, palette.text, room), state.focused ? 1.0f : 0.0f, Text);
}

void DrawTab(Painter& paint, const UiTabView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.accent.value_or(palette.accent);
    const float highlight = std::clamp(view.animation.highlight, 0.0f, 1.0f);
    const FloatRect rect = view.state.rect;
    paint.ChamferRect(rect, RowNotch, Fill(Mix(ButtonBody, accent, highlight * RowTint)));
    const float glow = view.cursor ? 1.0f : 0.0f;
    const Color4 frame = view.selected ? accent : NeonUiStyles().button.frameColor;
    DrawNeonFrame(paint, rect, RowNotch, view.cursor ? RowFrameWidth * 1.5f : RowFrameWidth, frame, glow, Frame);
    if (!view.state.font) return;
    const Float2 center = rect.Center();
    const Color4 text = view.selected ? palette.text : palette.textSoft;
    const float top = center.y - view.state.font->LineHeight(view.textSize) * 0.5f;
    const TextStyle style = CenteredText(view.textSize, text, rect.width - 2.0f * RowLabelInset);
    DrawNeonText(paint, view.label, {center.x, top}, style, view.selected ? 1.0f : 0.0f, Text);
}

void DrawScrollBar(Painter& paint, const UiScrollView& view) {
    const Color4 accent = CurrentTheme().palette.accent;
    paint.Rect(view.state.rect, Fill(NeonUiStyles().progressBar.track));
    const bool lit = view.dragging || view.state.hovered;
    paint.ChamferRect(view.thumb, view.thumb.width * 0.25f, Fill(lit ? accent : NeonUiStyles().button.frameColor));
}

void DrawTooltip(Painter& paint, const UiDrawState& state) {
    DrawNeonPanel(paint, state.rect, CurrentTheme().palette.accent, NeonUiStyles().panel);
}
} // namespace

const NeonUiStyle& NeonUiStyles() {
    static const NeonUiStyle styles{
        .frame = Frame,
        .text = Text,
        .panel = {.body = {14, 11, 34, 220},
                  .shine = {255, 255, 255, 18},
                  .notch = 10.0f,
                  .frameWidth = 2.0f,
                  .glow = 0.5f,
                  .shineOffset = 4.0f,
                  .shineHeight = 3.0f,
                  .frame = Frame},
        .chip = {.text = {12, 8, 24, 255},
                 .paddingX = 10.0f,
                 .paddingY = 4.0f,
                 .notch = 5.0f,
                 .glowRadiusScale = 0.7f,
                 .glowAlpha = 0.35f},
        .button = {.body = ButtonBody,
                   .shadow = ButtonShadow,
                   .frameColor = {95, 85, 150, 255},
                   .label = {205, 200, 235, 255},
                   .shine = {255, 255, 255, 30},
                   .pressColor = {255, 255, 255, 255},
                   .depth = 8.0f,
                   .notch = 12.0f,
                   .grow = 0.07f,
                   .squash = 0.05f,
                   .frameWidth = 2.5f,
                   .appearDrop = 70.0f,
                   .appearAlphaScale = 3.0f,
                   .shakeAmount = 12.0f,
                   .shakeSpeed = 45.0f,
                   .pulseSpeed = 4.0f,
                   .pulseAmount = 0.25f,
                   .pressFlash = 0.45f,
                   .darkAccent = 0.35f,
                   .shadowAccent = 0.4f,
                   .labelWhiten = 0.35f,
                   .shineOffset = 4.0f,
                   .shineHeight = 3.0f,
                   .shineHighlight = 2.0f,
                   .padding = 28.0f,
                   .frame = Frame,
                   .text = Text},
        .roundButton = {.body = ButtonBody,
                        .shadow = ButtonShadow,
                        .shadowOffset = 4.0f,
                        .frameWidth = 3.0f,
                        .glowInnerWidth = 6.0f,
                        .glowOuterWidth = 3.0f,
                        .glowAlpha = 0.28f,
                        .labelWhiten = 0.4f,
                        .textGlow = 0.6f,
                        .padding = 28.0f,
                        .text = Text},
        // Body in the surface color, border and letter in the secondary text color (translucent).
        .keyCap = {.body = {18, 14, 40, 229},
                   .frame = {190, 186, 220, 178},
                   .label = {190, 186, 220, 255},
                   .roundness = 0.3f,
                   .frameWidth = 1.5f},
        // Dark slot, glowing block fill, purple neon border.
        .progressBar = {.track = {12, 9, 28, 255},
                        .ghost = {255, 255, 255, 90},
                        .previewAlpha = 0.3f,
                        .shine = {255, 255, 255, 60},
                        .shineHeight = 1.0f / 3.0f,
                        .glowAlpha = 0.3f,
                        .glowSpread = 6.0f,
                        .segment = 20.0f,
                        .segmentGap = 3.0f,
                        .flash = {255, 255, 255, 120},
                        .frame = {160, 90, 255, 255},
                        .frameOutset = 4.0f,
                        .frameWidth = 2.0f,
                        .frameGlow = 0.5f,
                        .frameStyle = Frame},
        .buttonAnimation = {.highlightStiffness = 320.0f,
                            .highlightDamping = 16.0f,
                            .maxStep = 1.0f / 30.0f,
                            .pressSeconds = 0.3f,
                            .denySeconds = 0.35f}};
    return styles;
}

UiLooks NeonLooks() {
    UiLooks looks;
    looks.Set(UiLabelLook{.draw = DrawLabel});
    looks.Set(UiButtonLook{.textSize = 28.0f,
                           .minWidth = 160.0f,
                           .minHeight = 56.0f,
                           .padding = UiInsets::Symmetric(28.0f, 10.0f),
                           .animation = NeonUiStyles().buttonAnimation,
                           .draw = DrawButton});
    looks.Set(UiPanelLook{.padding = UiInsets::All(24.0f), .gap = 14.0f, .draw = DrawPanel});
    looks.Set(UiLayerLook{.seconds = 0.2f});
    const UiControlMetrics row{.textSize = 24.0f,
                               .minHeight = 50.0f,
                               .padding = UiInsets::Symmetric(18.0f, 6.0f),
                               .animation = NeonUiStyles().buttonAnimation};
    UiControlMetrics toggle = row;
    toggle.controlWidth = 72.0f;
    looks.Set(UiToggleLook{.metrics = toggle, .draw = DrawToggle});
    UiControlMetrics slider = row;
    slider.controlWidth = 250.0f;
    looks.Set(UiSliderLook{.metrics = slider, .draw = DrawSlider});
    looks.Set(UiStepperLook{.metrics = row, .draw = DrawStepper});
    looks.Set(UiTabLook{.textSize = 24.0f,
                        .minHeight = 46.0f,
                        .padding = UiInsets::Symmetric(20.0f, 6.0f),
                        .gap = 6.0f,
                        .animation = NeonUiStyles().buttonAnimation,
                        .draw = DrawTab});
    looks.Set(UiScrollLook{.barWidth = 10.0f, .draw = DrawScrollBar});
    looks.Set(UiTooltipLook{.padding = UiInsets::Symmetric(14.0f, 10.0f), .draw = DrawTooltip});
    return looks;
}
