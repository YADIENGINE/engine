#include <catch2/catch_test_macros.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "Editor/Ui/Style/EditorPalettePreset.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "ToolUi/ToolTheme.h"

namespace {
void CheckColor(ImVec4 actual, ImVec4 expected) {
    CHECK(actual.x == expected.x);
    CHECK(actual.y == expected.y);
    CHECK(actual.z == expected.z);
    CHECK(actual.w == expected.w);
}
struct PaletteFixture {
    PaletteFixture() {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1440, 900};
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontDefaultVector();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(ImTextureID{1});
        ApplyToolTheme();
    }
    ~PaletteFixture() { RegisterToolColors(nullptr); }
    void Apply(const ToolColorPalette& colors) {
        RegisterToolColors(&colors);
        ApplyToolPalette(ImGui::GetStyle().Colors, colors);
    }
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext};
};
} // namespace

TEST_CASE("Editor.Theme.ChangesOnlyColorsAndReturnsExactlyToCabinet", "[engine-editor]") {
    PaletteFixture fixture;
    auto& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.25f);
    style.FontScaleMain = 1.25f;
    const auto* fonts = ImGui::GetIO().Fonts;
    const auto flags = ImGui::GetIO().ConfigFlags;
    std::array<unsigned char, sizeof(ImGuiStyle)> before;
    std::memcpy(before.data(), &style, sizeof(style));
    std::array<ImVec4, ImGuiCol_COUNT> cabinet;
    std::ranges::copy(style.Colors, cabinet.begin());
    // Compare the same object's bytes outside Colors; no struct-copy padding or cosmetic hash is involved.
    constexpr auto ColorBegin = offsetof(ImGuiStyle, Colors);
    constexpr auto ColorEnd = ColorBegin + sizeof(style.Colors);
    for (const auto id : {EditorTheme::Slate, EditorTheme::Paper, EditorTheme::Cabinet}) {
        fixture.Apply(ResolveEditorPalette(id).colors);
        std::array<unsigned char, sizeof(ImGuiStyle)> after;
        std::memcpy(after.data(), &style, sizeof(style));
        CHECK(std::equal(before.begin(), before.begin() + ColorBegin, after.begin()));
        CHECK(std::equal(before.begin() + ColorEnd, before.end(), after.begin() + ColorEnd));
        CHECK(ImGui::GetIO().Fonts == fonts);
        CHECK(ImGui::GetIO().ConfigFlags == flags);
    }
    for (std::size_t i = 0; i < cabinet.size(); ++i)
        CheckColor(style.Colors[i], cabinet[i]);
    CheckColor(ToolColors().accent, ToolPalette::Accent);
    CheckColor(ToolToneColor(ToolTone::Information), ToolPalette::Secondary);
}

TEST_CASE("Editor.Theme.CustomColorDataReachesWidgetsAndSemanticRoles", "[engine-editor]") {
    PaletteFixture fixture;
    auto custom = ResolveEditorPalette(EditorTheme::Paper).colors;
    custom.accent = {0.12f, 0.34f, 0.56f, 1};
    custom.warning = {0.67f, 0.45f, 0.23f, 1};
    custom.agent = {0.45f, 0.23f, 0.67f, 1};
    fixture.Apply(custom);
    CheckColor(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), custom.accent);
    CheckColor(ToolSurfaceColor(ToolSurface::Content), custom.content);
    CheckColor(ToolToneColor(ToolTone::Accent), custom.accent);
    CheckColor(ToolToneColor(ToolTone::Warning), custom.warning);
    CheckColor(EditorAgentColor(), custom.agent);
    CheckColor(EditorInfoColor(), custom.activity);
    auto* other = ImGui::CreateContext();
    ImGui::SetCurrentContext(other);
    CheckColor(ToolColors().accent, ToolPalette::Accent);
    ImGui::DestroyContext(other);
    ImGui::SetCurrentContext(fixture.context.get());
    CheckColor(ToolColors().accent, custom.accent);
    RegisterToolColors(nullptr);
    CheckColor(ToolColors().window, ToolPalette::Window);
}

TEST_CASE("Editor.Theme.WorkspaceGeometryDockingAndPreferencesStayTheSame", "[engine-editor]") {
    PaletteFixture fixture;
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    EditorPreferences preferences;
    int fontSize = 100;
    EditorWorkspace workspace;
    const auto draw = [&] {
        ImGui::NewFrame();
        (void)workspace.Draw(snapshot, {}, {}, preferences, fontSize);
        ImGui::Render();
    };
    draw();
    draw();
    draw();
    struct PanelGeometry {
        ImGuiID dock;
        ImVec2 position;
        ImVec2 size;
    };
    std::vector<PanelGeometry> before;
    for (const auto& panel : EditorPanels)
        if (const auto* window = ImGui::FindWindowByName(panel.window))
            before.push_back({window->DockId, window->Pos, window->Size});
    REQUIRE_FALSE(before.empty());
    const auto saved = preferences;
    for (const auto& preset : EditorPalettePresets()) {
        fixture.Apply(preset.colors);
        draw();
        std::size_t i = 0;
        for (const auto& panel : EditorPanels)
            if (const auto* window = ImGui::FindWindowByName(panel.window)) {
                REQUIRE(i < before.size());
                CHECK(window->DockId == before[i].dock);
                CHECK(window->Pos.x == before[i].position.x);
                CHECK(window->Pos.y == before[i].position.y);
                CHECK(window->Size.x == before[i].size.x);
                CHECK(window->Size.y == before[i].size.y);
                ++i;
            }
        CHECK(i == before.size());
        CHECK(preferences == saved);
        CHECK(fontSize == 100);
    }
}
