#include "Ui/Kit/UiOptionsScreen.h"
#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiPanel.h"
#include "Ui/Widgets/UiScroll.h"
#include "Ui/Widgets/UiSlider.h"
#include "Ui/Widgets/UiStepper.h"
#include "Ui/Widgets/UiTabs.h"
#include "Ui/Widgets/UiToggle.h"
#include "Options/ChoiceList.h"

namespace {
constexpr float TitleSize = 36.0f;
constexpr float RowGap = 6.0f;
constexpr int MaxPercent = 100;
constexpr int PercentStep = 5;
constexpr std::array FpsLimits{30, 60, 120, 144, 240, 0};
constexpr std::array ParticlePercents{0, 25, 50, 75, 100};
constexpr std::array EffectLevels{EffectLevel::Full, EffectLevel::Reduced, EffectLevel::Off};
constexpr std::array GlowLevels{GlowQuality::Full, GlowQuality::Reduced, GlowQuality::Off};

enum class Tab { Audio, Display, Effects, Extra };

[[nodiscard]] bool AudioRows(Ui& ui, UiOptionsValues& values, const UiOptionsTexts& texts) {
    bool changed = UiSlider(ui, "music", texts.music, values.musicPercent, 0, MaxPercent, PercentStep);
    changed |= UiSlider(ui, "effects", texts.soundEffects, values.effectsPercent, 0, MaxPercent, PercentStep);
    return changed;
}

[[nodiscard]] bool DisplayRows(Ui& ui, UiOptionsValues& values, const UiOptionsTexts& texts) {
    bool changed = UiToggle(ui, "fullscreen", texts.fullscreen, values.fullscreen);
    const std::string fps = values.fpsLimit == 0 ? std::string(texts.unlimited) : std::to_string(values.fpsLimit);
    if (const int step = UiStepper(ui, "fps", texts.fpsLimit, fps)) {
        values.fpsLimit = StepChoice(values.fpsLimit, FpsLimits, step);
        changed = true;
    }
    changed |= UiToggle(ui, "vsync", texts.vsync, values.graphics.vsync);
    changed |= UiToggle(ui, "showFps", texts.showFps, values.graphics.showFps);
    return changed;
}

[[nodiscard]] std::string_view LevelName(EffectLevel level, const UiOptionsTexts& texts) {
    switch (level) {
    case EffectLevel::Full   : return texts.full;
    case EffectLevel::Reduced: return texts.reduced;
    case EffectLevel::Off    : return texts.off;
    }
    return texts.full;
}
[[nodiscard]] std::string_view GlowName(GlowQuality level, const UiOptionsTexts& texts) {
    switch (level) {
    case GlowQuality::Full   : return texts.full;
    case GlowQuality::Reduced: return texts.reduced;
    case GlowQuality::Off    : return texts.off;
    }
    return texts.full;
}

[[nodiscard]] bool EffectRows(Ui& ui, UiOptionsValues& values, const UiOptionsTexts& texts) {
    GraphicsSettings& graphics = values.graphics;
    bool changed = false;
    if (const int step = UiStepper(ui, "glow", texts.glow, GlowName(graphics.render.glow, texts))) {
        graphics.render.glow = StepChoice(graphics.render.glow, GlowLevels, step);
        changed = true;
    }
    const std::string particles = std::to_string(graphics.fx.particlePercent) + "%";
    if (const int step = UiStepper(ui, "particles", texts.particles, particles)) {
        graphics.fx.particlePercent = StepChoice(graphics.fx.particlePercent, ParticlePercents, step);
        changed = true;
    }
    if (const int step = UiStepper(ui, "shake", texts.shake, LevelName(graphics.fx.shake, texts))) {
        graphics.fx.shake = StepChoice(graphics.fx.shake, EffectLevels, step);
        changed = true;
    }
    changed |= UiToggle(ui, "motion", texts.motion, graphics.fx.motion);
    changed |= UiToggle(ui, "flashes", texts.flashes, graphics.fx.flashes);
    changed |= UiToggle(ui, "background", texts.background, graphics.render.backgroundEffects);
    changed |= UiToggle(ui, "screen-motion", texts.screenMotion, graphics.render.screenMotion);
    return changed;
}

struct OpenTab {
    int index = 0;
};
} // namespace

UiOptionsResult UiOptionsScreen(Ui& ui, UiId id, UiOptionsValues& values, const UiOptionsConfig& config) {
    const UiOptionsTexts& texts = config.texts;
    UiOptionsResult result;
    UiElement panel = UiPanel(ui, id,
                              {.width = UiFixed(config.width),
                               .height = UiFixed(config.height),
                               .alignX = UiAlign::Center,
                               .anchor = UiAnchor::Center});
    UiLabel(ui, texts.title, {.size = TitleSize});

    std::vector<Tab> tabs;
    std::vector<std::string_view> names;
    const auto add = [&](bool wanted, Tab tab, std::string_view name) {
        if (!wanted) return;
        tabs.push_back(tab);
        names.push_back(name);
    };
    add(config.audio, Tab::Audio, texts.audio);
    add(config.display, Tab::Display, texts.display);
    add(config.effects, Tab::Effects, texts.effects);
    add(static_cast<bool>(config.extraRows), Tab::Extra, config.extraTab);

    if (!tabs.empty()) {
        OpenTab& open = panel.Remember<OpenTab>();
        static_cast<void>(UiTabs(ui, "tabs", names, open.index));
        UiScroll rows(ui, tabs[static_cast<std::size_t>(open.index)], {.gap = RowGap});
        switch (tabs[static_cast<std::size_t>(open.index)]) {
        case Tab::Audio  : result.changed = AudioRows(ui, values, texts); break;
        case Tab::Display: result.changed = DisplayRows(ui, values, texts); break;
        case Tab::Effects: result.changed = EffectRows(ui, values, texts); break;
        case Tab::Extra  : config.extraRows(ui); break;
        }
    }
    result.closed =
        static_cast<bool>(UiButton(ui, "back", texts.back, {.sound = UiSound::Back})) || static_cast<bool>(ui.Back());
    return result;
}
