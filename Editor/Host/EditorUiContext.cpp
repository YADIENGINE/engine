#include "Editor/Host/EditorUiContext.h"
#include <extras/FA6FreeSolidFontData.h>
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <rlImGui.h>
#include <raylib.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "Editor/Ui/Style/EditorTheme.h"

namespace {
constexpr float BaseFontSize = 14.0f;
constexpr float BaseIconSize = 13.0f;
constexpr float DefaultFontPercent = 100.0f;
constexpr float DefaultInterfaceScale = 1.25f;

ImFont* LoadEditorSystemFont(ImFontAtlas& atlas, const wchar_t* fileName) {
    ImFontConfig config;
    config.SizePixels = BaseFontSize;
    config.PixelSnapH = false;
    // Use installed Windows fonts without bundling or depending on project assets.
    std::size_t length = 0;
    if (_wgetenv_s(&length, nullptr, 0, L"WINDIR") == 0 && length > 1) {
        std::wstring directory(length, L'\0');
        if (_wgetenv_s(&length, directory.data(), directory.size(), L"WINDIR") == 0) {
            directory.resize(length - 1);
            const auto path = std::filesystem::path(directory) / "Fonts" / fileName;
            std::error_code error;
            if (std::filesystem::is_regular_file(path, error))
                if (auto* font = atlas.AddFontFromFileTTF(path.string().c_str(), BaseFontSize, &config)) return font;
        }
    }
    return atlas.AddFontDefaultVector(&config);
}
void LoadEditorFonts(ImFontAtlas& atlas) {
    atlas.Clear();
    auto* ui = LoadEditorSystemFont(atlas, L"segoeui.ttf");

    static constexpr std::array<ImWchar, 3> IconRange{ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig icons;
    icons.MergeMode = true;
    icons.PixelSnapH = false;
    atlas.AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, static_cast<int>(fa_solid_900_compressed_size),
                                         BaseIconSize, &icons, IconRange.data());
    auto* technical = LoadEditorSystemFont(atlas, L"consola.ttf");
    RegisterEditorFonts(ui, technical);
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
EditorUiContext::EditorUiContext() {
    rlImGuiBeginInitImGui();
    LoadEditorFonts(*ImGui::GetIO().Fonts);
    rlImGuiEndInitImGui();
    ApplyEditorTheme();
    _baseStyle = ImGui::GetStyle();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // Preview controls share the keyboard with ImGui; keep editor navigation mouse-driven.
    io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable);
    io.ConfigWindowsMoveFromTitleBarOnly = true;
}
EditorUiContext::~EditorUiContext() {
    ClearEditorFonts();
    rlImGuiShutdown();
}
void EditorUiContext::BeginFrame(int fontSize) {
    const float dpi = std::max(1.0f, GetWindowScaleDPI().x);
    const float fontScale = static_cast<float>(fontSize) / DefaultFontPercent * DefaultInterfaceScale;
    const float scale = dpi * fontScale;
    auto& style = ImGui::GetStyle();
    if (scale != _scale) {
        // Always scale the baseline: ScaleAllSizes truncates, so repeated ratios lose precision.
        style = _baseStyle;
        style.ScaleAllSizes(scale);
        // ScaleAllSizes can truncate thin separators to zero; Separator() requires positive thickness.
        style.SeparatorSize = std::max(1.0f, style.SeparatorSize);
        _scale = scale;
    }
    style.FontSizeBase = BaseFontSize;
    style.FontScaleDpi = dpi;
    style.FontScaleMain = fontScale;
    rlImGuiBegin();
    // rlImGui uploads dynamic atlases before processing draw callbacks. Apply filtering here so
    // newly created/replaced atlases are filtered before their first draw, without a backend fork.
    ImGui::GetBackgroundDrawList()->AddCallback(FilterFontTextures, nullptr);
}
void EditorUiContext::EndFrame() const { rlImGuiEnd(); }
