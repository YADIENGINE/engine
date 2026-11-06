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
ToolColorPalette NeonArcade() {
    ToolColorPalette colors;
    colors.window = Rgb(0x160f26);
    colors.panel = Rgb(0x201730);
    colors.elevated = Rgb(0x2c2040);
    colors.content = Rgb(0x120c20);
    colors.control = Rgb(0x342548);
    colors.hover = Rgb(0x46335e);
    colors.pressed = Rgb(0x291d3c);
    colors.selected = Rgb(0x17484c);
    colors.accent = Rgb(0x25e6da);
    colors.accentHover = Rgb(0x79fff2);
    colors.accentPressed = Rgb(0x17b8b0);
    colors.accentText = Rgb(0x092825);
    colors.text = Rgb(0xefe7ff);
    colors.secondary = Rgb(0xbfb0d3);
    colors.muted = Rgb(0xa18bb9);
    colors.disabled = Rgb(0x877399);
    colors.border = Rgb(0x5e4778);
    colors.separator = Rgb(0x3d2d50);
    colors.sliderGrab = Rgb(0x715b8b);
    colors.success = Rgb(0x7ee5a5);
    colors.warning = Rgb(0xf9d66c);
    colors.error = Rgb(0xff829d);
    colors.information = Rgb(0x9dbbff);
    colors.agent = Rgb(0xff4fa3);
    colors.activity = Rgb(0x72d8ff);
    colors.stopText = Rgb(0xffb0c1);
    colors.successHover = Rgb(0xa3f0bf);
    colors.successPressed = Rgb(0x59c587);
    colors.successText = Rgb(0x10291b);
    colors.assetFont = Rgb(0xdfc280);
    return colors;
}
ToolColorPalette DeepOcean() {
    ToolColorPalette colors;
    colors.window = Rgb(0x071d26);
    colors.panel = Rgb(0x102b35);
    colors.elevated = Rgb(0x1b3944);
    colors.content = Rgb(0x061820);
    colors.control = Rgb(0x21424c);
    colors.hover = Rgb(0x2e5661);
    colors.pressed = Rgb(0x16343e);
    colors.selected = Rgb(0x174d4a);
    colors.accent = Rgb(0x35c9b0);
    colors.accentHover = Rgb(0x74e3cd);
    colors.accentPressed = Rgb(0x24a78f);
    colors.accentText = Rgb(0x072b25);
    colors.text = Rgb(0xdaf0eb);
    colors.secondary = Rgb(0x9bbfbe);
    colors.muted = Rgb(0x7fa4a7);
    colors.disabled = Rgb(0x688b91);
    colors.border = Rgb(0x3f646d);
    colors.separator = Rgb(0x264751);
    colors.sliderGrab = Rgb(0x527b83);
    colors.success = Rgb(0x8ad29b);
    colors.warning = Rgb(0xe6c17a);
    colors.error = Rgb(0xf18b8b);
    colors.information = Rgb(0x88bfdf);
    colors.agent = Rgb(0xafa5e7);
    colors.activity = Rgb(0x6ed5d0);
    colors.stopText = Rgb(0xf5b4b4);
    colors.successHover = Rgb(0xade4b9);
    colors.successPressed = Rgb(0x6cb880);
    colors.successText = Rgb(0x10291b);
    colors.assetFont = Rgb(0xd6ca8a);
    return colors;
}
ToolColorPalette SakuraInk() {
    ToolColorPalette colors;
    colors.window = Rgb(0x211923);
    colors.panel = Rgb(0x2d222e);
    colors.elevated = Rgb(0x3b2d3b);
    colors.content = Rgb(0x1c151f);
    colors.control = Rgb(0x443243);
    colors.hover = Rgb(0x584255);
    colors.pressed = Rgb(0x352737);
    colors.selected = Rgb(0x553345);
    colors.accent = Rgb(0xe9a0b5);
    colors.accentHover = Rgb(0xf6c2d0);
    colors.accentPressed = Rgb(0xcf8099);
    colors.accentText = Rgb(0x321c29);
    colors.text = Rgb(0xf0e4e9);
    colors.secondary = Rgb(0xc7afbf);
    colors.muted = Rgb(0xab91a5);
    colors.disabled = Rgb(0x92798e);
    colors.border = Rgb(0x705367);
    colors.separator = Rgb(0x493747);
    colors.sliderGrab = Rgb(0x87667d);
    colors.success = Rgb(0xa5cba1);
    colors.warning = Rgb(0xe5c58f);
    colors.error = Rgb(0xef939c);
    colors.information = Rgb(0xb3bfdc);
    colors.agent = Rgb(0xc4a9eb);
    colors.activity = Rgb(0x9acbd0);
    colors.stopText = Rgb(0xf4b7be);
    colors.successHover = Rgb(0xbeddb9);
    colors.successPressed = Rgb(0x88b584);
    colors.successText = Rgb(0x182b1a);
    colors.assetFont = Rgb(0xd7c294);
    return colors;
}
ToolColorPalette DesertTerminal() {
    ToolColorPalette colors;
    colors.window = Rgb(0x1b1d13);
    colors.panel = Rgb(0x27291c);
    colors.elevated = Rgb(0x343725);
    colors.content = Rgb(0x16180f);
    colors.control = Rgb(0x3b3e29);
    colors.hover = Rgb(0x4c5034);
    colors.pressed = Rgb(0x303320);
    colors.selected = Rgb(0x514522);
    colors.accent = Rgb(0xe9b44c);
    colors.accentHover = Rgb(0xf6d183);
    colors.accentPressed = Rgb(0xc99431);
    colors.accentText = Rgb(0x29200b);
    colors.text = Rgb(0xddd5b5);
    colors.secondary = Rgb(0xbbb58f);
    colors.muted = Rgb(0xa19d77);
    colors.disabled = Rgb(0x888761);
    colors.border = Rgb(0x686946);
    colors.separator = Rgb(0x41452c);
    colors.sliderGrab = Rgb(0x7b7d52);
    colors.success = Rgb(0xaac878);
    colors.warning = Rgb(0xe8c77b);
    colors.error = Rgb(0xe89574);
    colors.information = Rgb(0xa7bdba);
    colors.agent = Rgb(0xbbbb89);
    colors.activity = Rgb(0x8bc6b3);
    colors.stopText = Rgb(0xf1b89b);
    colors.successHover = Rgb(0xc6df96);
    colors.successPressed = Rgb(0x8cab5b);
    colors.successText = Rgb(0x202a10);
    colors.assetFont = Rgb(0xd4ce8b);
    return colors;
}
ToolColorPalette VampireVelvet() {
    ToolColorPalette colors;
    colors.window = Rgb(0x1c1018);
    colors.panel = Rgb(0x291820);
    colors.elevated = Rgb(0x38222c);
    colors.content = Rgb(0x160c13);
    colors.control = Rgb(0x412631);
    colors.hover = Rgb(0x55333f);
    colors.pressed = Rgb(0x321b27);
    colors.selected = Rgb(0x592637);
    colors.accent = Rgb(0xd65a70);
    colors.accentHover = Rgb(0xeb8295);
    colors.accentPressed = Rgb(0xb9425b);
    colors.accentText = Rgb(0x210a13);
    colors.text = Rgb(0xf2e5d7);
    colors.secondary = Rgb(0xc9b0b4);
    colors.muted = Rgb(0xad919d);
    colors.disabled = Rgb(0x947887);
    colors.border = Rgb(0x734450);
    colors.separator = Rgb(0x492936);
    colors.sliderGrab = Rgb(0x885562);
    colors.success = Rgb(0xa2c49b);
    colors.warning = Rgb(0xe1bd7e);
    colors.error = Rgb(0xff997c);
    colors.information = Rgb(0xb3b9d5);
    colors.agent = Rgb(0xbe9cdd);
    colors.activity = Rgb(0x91c6c8);
    colors.stopText = Rgb(0xffc0a8);
    colors.successHover = Rgb(0xbfdbb7);
    colors.successPressed = Rgb(0x86ad7f);
    colors.successText = Rgb(0x192a18);
    colors.assetFont = Rgb(0xd5c28c);
    return colors;
}
ToolColorPalette MatchaGarden() {
    ToolColorPalette colors;
    colors.window = Rgb(0xd3dcc7);
    colors.panel = Rgb(0xe4e9d9);
    colors.elevated = Rgb(0xf2f5ea);
    colors.content = Rgb(0xdce3cf);
    colors.control = Rgb(0xcdd7bf);
    colors.hover = Rgb(0xbdccab);
    colors.pressed = Rgb(0xb1c39e);
    colors.selected = Rgb(0xc4d6b0);
    colors.accent = Rgb(0x587344);
    colors.accentHover = Rgb(0x486338);
    colors.accentPressed = Rgb(0x3c552d);
    colors.accentText = Rgb(0xffffff);
    colors.text = Rgb(0x263b2b);
    colors.secondary = Rgb(0x4c6047);
    colors.muted = Rgb(0x607355);
    colors.disabled = Rgb(0x6b7d60);
    colors.border = Rgb(0x95a88b);
    colors.separator = Rgb(0xb7c4aa);
    colors.sliderGrab = Rgb(0x91a67f);
    colors.success = Rgb(0x35643f);
    colors.warning = Rgb(0x835b17);
    colors.error = Rgb(0xa53d40);
    colors.information = Rgb(0x365e72);
    colors.agent = Rgb(0x71558b);
    colors.activity = Rgb(0x276c66);
    colors.stopText = Rgb(0x923438);
    colors.successHover = Rgb(0x2b5535);
    colors.successPressed = Rgb(0x23472c);
    colors.successText = Rgb(0xffffff);
    colors.assetFont = Rgb(0x71601d);
    colors.tableRowAlt = {0, 0, 0, 0.02f};
    colors.modalDim = {0, 0, 0, 0.32f};
    colors.navWindowingHighlight = {0.20f, 0.20f, 0.20f, 0.70f};
    colors.navWindowingDim = {0.20f, 0.20f, 0.20f, 0.20f};
    return colors;
}
ToolColorPalette SolarFlare() {
    ToolColorPalette colors;
    colors.window = Rgb(0x211522);
    colors.panel = Rgb(0x302031);
    colors.elevated = Rgb(0x402b3d);
    colors.content = Rgb(0x1b101e);
    colors.control = Rgb(0x493044);
    colors.hover = Rgb(0x5f3e52);
    colors.pressed = Rgb(0x392335);
    colors.selected = Rgb(0x62382f);
    colors.accent = Rgb(0xff974f);
    colors.accentHover = Rgb(0xffb67e);
    colors.accentPressed = Rgb(0xe57b35);
    colors.accentText = Rgb(0x321a0b);
    colors.text = Rgb(0xf4ddd0);
    colors.secondary = Rgb(0xcdb1ae);
    colors.muted = Rgb(0xb395a0);
    colors.disabled = Rgb(0x997e8d);
    colors.border = Rgb(0x795064);
    colors.separator = Rgb(0x4d3248);
    colors.sliderGrab = Rgb(0x946077);
    colors.success = Rgb(0xa7d19b);
    colors.warning = Rgb(0xe9d379);
    colors.error = Rgb(0xff8596);
    colors.information = Rgb(0xa9c6e9);
    colors.agent = Rgb(0xc6a0ef);
    colors.activity = Rgb(0x8cd3d3);
    colors.stopText = Rgb(0xffb5bd);
    colors.successHover = Rgb(0xc3e5b9);
    colors.successPressed = Rgb(0x8ab77f);
    colors.successText = Rgb(0x1b2d17);
    colors.assetFont = Rgb(0xdac48b);
    return colors;
}
ToolColorPalette ArcticLilac() {
    ToolColorPalette colors;
    colors.window = Rgb(0xe0daee);
    colors.panel = Rgb(0xeeebf7);
    colors.elevated = Rgb(0xfaf8ff);
    colors.content = Rgb(0xe7e1f2);
    colors.control = Rgb(0xd9d1e8);
    colors.hover = Rgb(0xcbbfe0);
    colors.pressed = Rgb(0xbeafd5);
    colors.selected = Rgb(0xd9ccef);
    colors.accent = Rgb(0x7959bf);
    colors.accentHover = Rgb(0x6848ad);
    colors.accentPressed = Rgb(0x573a97);
    colors.accentText = Rgb(0xffffff);
    colors.text = Rgb(0x302943);
    colors.secondary = Rgb(0x615571);
    colors.muted = Rgb(0x786986);
    colors.disabled = Rgb(0x83758f);
    colors.border = Rgb(0xb2a2c5);
    colors.separator = Rgb(0xcec3dd);
    colors.sliderGrab = Rgb(0xab98c2);
    colors.success = Rgb(0x356747);
    colors.warning = Rgb(0x875b16);
    colors.error = Rgb(0xb43c59);
    colors.information = Rgb(0x386781);
    colors.agent = Rgb(0x7951a0);
    colors.activity = Rgb(0x316e7a);
    colors.stopText = Rgb(0x96304d);
    colors.successHover = Rgb(0x2b583b);
    colors.successPressed = Rgb(0x234a31);
    colors.successText = Rgb(0xffffff);
    colors.assetFont = Rgb(0x78601c);
    colors.tableRowAlt = {0, 0, 0, 0.02f};
    colors.modalDim = {0, 0, 0, 0.32f};
    colors.navWindowingHighlight = {0.20f, 0.20f, 0.20f, 0.70f};
    colors.navWindowingDim = {0.20f, 0.20f, 0.20f, 0.20f};
    return colors;
}
ToolColorPalette MonochromeStudio() {
    ToolColorPalette colors;
    colors.window = Rgb(0x181818);
    colors.panel = Rgb(0x232323);
    colors.elevated = Rgb(0x303030);
    colors.content = Rgb(0x121212);
    colors.control = Rgb(0x383838);
    colors.hover = Rgb(0x494949);
    colors.pressed = Rgb(0x2b2b2b);
    colors.selected = Rgb(0x454545);
    colors.accent = Rgb(0xd4d4d4);
    colors.accentHover = Rgb(0xeeeeee);
    colors.accentPressed = Rgb(0xb4b4b4);
    colors.accentText = Rgb(0x181818);
    colors.text = Rgb(0xe8e8e8);
    colors.secondary = Rgb(0xb4b4b4);
    colors.muted = Rgb(0x929292);
    colors.disabled = Rgb(0x7b7b7b);
    colors.border = Rgb(0x585858);
    colors.separator = Rgb(0x393939);
    colors.sliderGrab = Rgb(0x707070);
    colors.success = Rgb(0x9bc6a2);
    colors.warning = Rgb(0xd8bd7d);
    colors.error = Rgb(0xe39a9a);
    colors.information = Rgb(0xb4b4b4);
    colors.agent = Rgb(0xc4c4c4);
    colors.activity = Rgb(0xbcbcbc);
    colors.stopText = Rgb(0xe9b6b6);
    colors.successHover = Rgb(0xb7dabb);
    colors.successPressed = Rgb(0x80ae87);
    colors.successText = Rgb(0x18291c);
    colors.assetFont = Rgb(0xc0c0c0);
    return colors;
}
ToolColorPalette CandyCircuit() {
    ToolColorPalette colors;
    colors.window = Rgb(0x171a30);
    colors.panel = Rgb(0x222640);
    colors.elevated = Rgb(0x303553);
    colors.content = Rgb(0x111428);
    colors.control = Rgb(0x393e5a);
    colors.hover = Rgb(0x4b5270);
    colors.pressed = Rgb(0x2c314b);
    colors.selected = Rgb(0x464b2b);
    colors.accent = Rgb(0xe5f26b);
    colors.accentHover = Rgb(0xf5ff9c);
    colors.accentPressed = Rgb(0xc4d34c);
    colors.accentText = Rgb(0x24290b);
    colors.text = Rgb(0xefedff);
    colors.secondary = Rgb(0xb9bdd9);
    colors.muted = Rgb(0x989fbe);
    colors.disabled = Rgb(0x8089ab);
    colors.border = Rgb(0x596386);
    colors.separator = Rgb(0x3b4362);
    colors.sliderGrab = Rgb(0x707b9e);
    colors.success = Rgb(0x8bd9af);
    colors.warning = Rgb(0xffc77a);
    colors.error = Rgb(0xff899d);
    colors.information = Rgb(0xa0caff);
    colors.agent = Rgb(0xb8a1ff);
    colors.activity = Rgb(0x7ddfe4);
    colors.stopText = Rgb(0xffb4c3);
    colors.successHover = Rgb(0xadedca);
    colors.successPressed = Rgb(0x6cbd91);
    colors.successText = Rgb(0x102a20);
    colors.assetFont = Rgb(0xe3ce8b);
    return colors;
}
EditorPalettePreset Preset(EditorTheme theme, const ToolColorPalette& colors) {
    const auto& definition = EditorThemeInfo(theme);
    return {definition.id, definition.key, definition.title, colors};
}
} // namespace

std::span<const EditorPalettePreset> EditorPalettePresets() {
    static const std::array Presets{Preset(EditorTheme::Cabinet, {}),
                                    Preset(EditorTheme::Slate, Slate()),
                                    Preset(EditorTheme::Paper, Paper()),
                                    Preset(EditorTheme::NeonArcade, NeonArcade()),
                                    Preset(EditorTheme::DeepOcean, DeepOcean()),
                                    Preset(EditorTheme::SakuraInk, SakuraInk()),
                                    Preset(EditorTheme::DesertTerminal, DesertTerminal()),
                                    Preset(EditorTheme::VampireVelvet, VampireVelvet()),
                                    Preset(EditorTheme::MatchaGarden, MatchaGarden()),
                                    Preset(EditorTheme::SolarFlare, SolarFlare()),
                                    Preset(EditorTheme::ArcticLilac, ArcticLilac()),
                                    Preset(EditorTheme::MonochromeStudio, MonochromeStudio()),
                                    Preset(EditorTheme::CandyCircuit, CandyCircuit())};
    return Presets;
}
const EditorPalettePreset& ResolveEditorPalette(EditorTheme theme) {
    const auto presets = EditorPalettePresets();
    const auto found = std::ranges::find(presets, theme, &EditorPalettePreset::id);
    return found == presets.end() ? presets.front() : *found;
}
