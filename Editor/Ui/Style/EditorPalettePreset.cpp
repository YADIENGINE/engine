#include "Editor/Ui/Style/EditorPalettePreset.h"
#include <algorithm>
#include <array>

namespace {
constexpr ImVec4 Rgb(unsigned int rgb) {
    return {static_cast<float>((rgb >> 16) & 0xff) / 255.0f, static_cast<float>((rgb >> 8) & 0xff) / 255.0f,
            static_cast<float>(rgb & 0xff) / 255.0f, 1.0f};
}
ToolColorPalette Slate() {
    ToolColorPalette colors;
    colors.window = Rgb(0x121419);
    colors.panel = Rgb(0x1b1e24);
    colors.elevated = Rgb(0x252a33);
    colors.content = Rgb(0x101217);
    colors.control = Rgb(0x2a2f38);
    colors.hover = Rgb(0x353c47);
    colors.pressed = Rgb(0x232830);
    colors.selected = Rgb(0x233b59);
    colors.accent = Rgb(0x6aa9ff);
    colors.accentHover = Rgb(0x8bbcff);
    colors.accentPressed = Rgb(0x4f8ce6);
    colors.accentText = Rgb(0x0d1726);
    colors.text = Rgb(0xe7eaf0);
    colors.secondary = Rgb(0xa9b1bd);
    colors.muted = Rgb(0x8993a2);
    colors.disabled = Rgb(0x737f90);
    colors.border = Rgb(0x3d4654);
    colors.separator = Rgb(0x303742);
    colors.sliderGrab = Rgb(0x515b6b);
    colors.information = colors.secondary;
    return colors;
}
ToolColorPalette Paper() {
    ToolColorPalette colors;
    colors.window = Rgb(0xe3e7ed);
    colors.panel = Rgb(0xf6f8fb);
    colors.elevated = Rgb(0xffffff);
    colors.content = Rgb(0xebeff5);
    colors.control = Rgb(0xdde3eb);
    colors.hover = Rgb(0xcdd7e4);
    colors.pressed = Rgb(0xc3cfde);
    colors.selected = Rgb(0xd3e3f8);
    colors.accent = Rgb(0x245da8);
    colors.accentHover = Rgb(0x1b4f92);
    colors.accentPressed = Rgb(0x17447f);
    colors.accentText = Rgb(0xffffff);
    colors.text = Rgb(0x202832);
    colors.secondary = Rgb(0x4f5d6d);
    colors.muted = Rgb(0x626f80);
    colors.disabled = Rgb(0x697787);
    colors.border = Rgb(0xb1bdcc);
    colors.separator = Rgb(0xcbd3de);
    colors.sliderGrab = Rgb(0xb1bdcc);
    colors.success = Rgb(0x2c6f42);
    colors.warning = Rgb(0x8b570d);
    colors.error = Rgb(0xad3546);
    colors.information = colors.secondary;
    colors.agent = Rgb(0x6451a4);
    colors.activity = Rgb(0x1e6778);
    colors.stopText = Rgb(0x8c3041);
    colors.successHover = Rgb(0x265f39);
    colors.successPressed = Rgb(0x205232);
    colors.successText = Rgb(0xffffff);
    colors.assetFont = Rgb(0x6f641c);
    colors.tableRowAlt = {0, 0, 0, 0.02f};
    colors.modalDim = {0, 0, 0, 0.32f};
    colors.navWindowingHighlight = {0.20f, 0.20f, 0.20f, 0.70f};
    colors.navWindowingDim = {0.20f, 0.20f, 0.20f, 0.20f};
    return colors;
}
EditorPalettePreset Preset(EditorTheme theme, const ToolColorPalette& colors) {
    const auto& definition = EditorThemeInfo(theme);
    return {definition.id, definition.key, definition.title, colors};
}
} // namespace

std::span<const EditorPalettePreset> EditorPalettePresets() {
    static const std::array Presets{Preset(EditorTheme::Cabinet, {}), Preset(EditorTheme::Slate, Slate()),
                                    Preset(EditorTheme::Paper, Paper())};
    return Presets;
}
const EditorPalettePreset& ResolveEditorPalette(EditorTheme theme) {
    const auto presets = EditorPalettePresets();
    const auto found = std::ranges::find(presets, theme, &EditorPalettePreset::id);
    return found == presets.end() ? presets.front() : *found;
}
