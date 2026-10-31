#include "ToolUi/ToolUiContext.h"
#include <extras/FA6FreeSolidFontData.h>
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <rlImGui.h>
#include <raylib.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "ToolUi/ToolTheme.h"

namespace {
#include "ToolFontData.h"
constexpr float DefaultFontPercent = 100.0f;
// stb_truetype sizes by ascender-to-descender height, not em size. Noto's hhea span is 1362/1000 em,
// Segoe UI's is 2724/2048 em;
// normalize the face itself so a 14 px UI font has the expected em size without enlarging every control.
constexpr float NotoEmScale = 1.362f;
constexpr float SegoeEmScale = 2724.0f / 2048.0f;

ImFont* LoadSystemFont(ImFontAtlas& atlas, const wchar_t* fileName, const ImFontConfig& config) {
    // Use installed Windows fonts without bundling or depending on project assets.
    std::size_t length = 0;
    if (_wgetenv_s(&length, nullptr, 0, L"WINDIR") == 0 && length > 1) {
        std::wstring directory(length, L'\0');
        if (_wgetenv_s(&length, directory.data(), directory.size(), L"WINDIR") == 0) {
            directory.resize(length - 1);
            const auto path = std::filesystem::path(directory) / "Fonts" / fileName;
            std::error_code error;
            if (std::filesystem::is_regular_file(path, error))
                if (auto* font = atlas.AddFontFromFileTTF(path.string().c_str(), ToolMetrics::BodySize, &config))
                    return font;
        }
    }
    return nullptr;
}
ImFont* LoadUiFont(ImFontAtlas& atlas, const unsigned char* data, int size, const char* name,
                   const wchar_t* systemFile) {
    ImFontConfig config;
    config.ExtraSizeScale = SegoeEmScale;
    config.PixelSnapH = false; // Preserve kerning and fractional DPI advances; ImGui snaps glyph origins.
    config.OversampleH = 2;
    config.OversampleV = 1;
    std::snprintf(config.Name, std::size(config.Name), "%s", name);
    auto* font = LoadSystemFont(atlas, systemFile, config);
    if (!font) {
        config.FontDataOwnedByAtlas = false;
        config.ExtraSizeScale = NotoEmScale;
        std::snprintf(config.Name, std::size(config.Name), "Noto Sans fallback");
        font = atlas.AddFontFromMemoryTTF(const_cast<unsigned char*>(data), size, ToolMetrics::BodySize, &config);
    }
    static constexpr std::array<ImWchar, 3> IconRange{ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons;
    icons.MergeMode = true;
    icons.PixelSnapH = false;
    icons.GlyphMinAdvanceX = ToolMetrics::IconSize;
    icons.GlyphMaxAdvanceX = ToolMetrics::IconSize;
    icons.GlyphExtraAdvanceX = ToolMetrics::Space;
    atlas.AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, static_cast<int>(fa_solid_900_compressed_size),
                                         ToolMetrics::IconSize, &icons, IconRange.data());
    return font;
}
void LoadToolFonts(ImFontAtlas& atlas) {
    atlas.Clear();
    auto* ui = LoadUiFont(atlas, ToolRegularData.data(), static_cast<int>(ToolRegularData.size()), "Segoe UI Regular",
                          L"segoeui.ttf");
    auto* medium = LoadUiFont(atlas, ToolMediumData.data(), static_cast<int>(ToolMediumData.size()),
                              "Segoe UI Semibold", L"seguisb.ttf");
    auto* heading = LoadUiFont(atlas, ToolHeadingData.data(), static_cast<int>(ToolHeadingData.size()),
                               "Segoe UI Semibold", L"seguisb.ttf");
    ImFontConfig technicalConfig;
    technicalConfig.SizePixels = ToolMetrics::BodySize;
    technicalConfig.PixelSnapH = false;
    auto* technical = LoadSystemFont(atlas, L"consola.ttf", technicalConfig);
    if (!technical) technical = atlas.AddFontDefaultVector(&technicalConfig);
    // Consolas'ta olmayan semboller (terminal TUI'lerinin ⏺ ✻ ⎿, oklar, braille) Segoe UI Symbol'den.
    ImFontConfig symbolConfig = technicalConfig;
    symbolConfig.MergeMode = true;
    static_cast<void>(LoadSystemFont(atlas, L"seguisym.ttf", symbolConfig));
    RegisterToolFonts(ui, technical, medium, heading);
}
void FilterFontTextures(const ImDrawList*, const ImDrawCmd*) {
    const auto* textures = ImGui::GetDrawData()->Textures;
    if (!textures) return;
    for (const auto* texture : *textures) {
        if (texture->Status != ImTextureStatus_OK || texture->GetTexID() == ImTextureID_Invalid) continue;
        const Texture2D handle{static_cast<unsigned int>(texture->GetTexID()), texture->Width, texture->Height, 1,
                               PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        SetTextureFilter(handle, TEXTURE_FILTER_BILINEAR);
    }
}
} // namespace
const char* ToolFontLicense() { return ToolNotoLicense; }

ToolUiContext::ToolUiContext(const ToolUiOptions& options) {
    rlImGuiBeginInitImGui();
    LoadToolFonts(*ImGui::GetIO().Fonts);
    rlImGuiEndInitImGui();
    ApplyToolTheme();
    _baseStyle = ImGui::GetStyle();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable);
    if (options.docking) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (options.keyboardNavigation) io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
}
ToolUiContext::~ToolUiContext() {
    ClearToolFonts();
    rlImGuiShutdown();
}
void ToolUiContext::BeginFrame(int scalePercent) {
    const float dpi = std::max(1.0f, GetWindowScaleDPI().x);
    const float fontScale = static_cast<float>(scalePercent) / DefaultFontPercent;
    const float scale = dpi * fontScale;
    auto& style = ImGui::GetStyle();
    if (scale != _scale) {
        // Always scale the baseline: ScaleAllSizes truncates, so repeated ratios lose precision.
        style = _baseStyle;
        style.ScaleAllSizes(scale);
        // Keep pane and control boundaries visible below 100%; ScaleAllSizes truncates thin strokes to zero.
        style.WindowBorderSize = std::max(1.0f, style.WindowBorderSize);
        style.ChildBorderSize = std::max(1.0f, style.ChildBorderSize);
        style.PopupBorderSize = std::max(1.0f, style.PopupBorderSize);
        style.FrameBorderSize = std::max(1.0f, style.FrameBorderSize);
        style.SeparatorSize = std::max(1.0f, style.SeparatorSize);
        _scale = scale;
    }
    style.FontSizeBase = ToolMetrics::BodySize;
    style.FontScaleDpi = dpi;
    style.FontScaleMain = fontScale;
    // Floating dock groups draw their tab bars during NewFrame, before the workspace submits its panels.
    const auto framePadding = style.FramePadding;
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
        style.FramePadding.y = ToolMetrics::PanelHeaderPadding * scale;
    rlImGuiBegin();
    style.FramePadding = framePadding;
    // rlImGui uploads dynamic atlases before processing draw callbacks. Apply filtering here so
    // newly created/replaced atlases are filtered before their first draw, without a backend fork.
    ImGui::GetBackgroundDrawList()->AddCallback(FilterFontTextures, nullptr);
}
void ToolUiContext::EndFrame() const { rlImGuiEnd(); }
