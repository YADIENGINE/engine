#include "Ui/Looks/Neon/NeonLooks.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"
#include "Ui/Looks/UiButtonLook.h"
#include "Ui/Looks/UiLabelLook.h"
#include "Ui/Looks/UiLayerLook.h"
#include "Ui/Looks/UiPanelLook.h"

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
    return looks;
}
