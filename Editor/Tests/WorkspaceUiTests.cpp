#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <memory>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"

TEST_CASE("Editor.Workspace.BuildFailurePopupIsOncePerFailedAttempt", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1366, 768};
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyEditorTheme();
    const std::vector<PreviewDefinition> catalog;
    const PreviewParameters parameters;
    EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Error, status};
    EditorPreferences preferences;
    EditorWorkspace workspace;
    EditorUiActions actions;
    int fontSize = 100;
    const auto frame = [&] {
        ImGui::NewFrame();
        (void)workspace.Draw(snapshot, actions, {}, preferences, fontSize);
        const bool popup = ImGui::IsPopupOpen("Build & Refresh failed");
        ImGui::Render();
        return popup;
    };
    CHECK_FALSE(frame());
    diagnostics.BeginAttempt();
    diagnostics.Append("compile error", EditorLogLevel::Error, EditorLogSource::Build);
    diagnostics.BuildExited(1);
    diagnostics.FinishAttempt(EditorBuildResult::BuildFailed);
    CHECK(frame());
    io.AddKeyEvent(ImGuiKey_Escape, true);
    CHECK_FALSE(frame());
    io.AddKeyEvent(ImGuiKey_Escape, false);
    CHECK_FALSE(frame());
    diagnostics.BeginAttempt();
    diagnostics.FinishAttempt(EditorBuildResult::Canceled);
    CHECK_FALSE(frame());
    diagnostics.BeginAttempt();
    diagnostics.BuildExited(0);
    diagnostics.Append("startup error", EditorLogLevel::Error);
    diagnostics.FinishAttempt(EditorBuildResult::RefreshFailed);
    CHECK(frame());
}

TEST_CASE("Editor.Workspace.ShortcutsCancelPreviewInputAndPaletteOwnsKeys", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1366, 768};
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyEditorTheme();
    const std::vector<PreviewDefinition> catalog;
    const PreviewParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    snapshot.capabilities.enabled[static_cast<std::size_t>(EditorCommand::Open)] = true;
    EditorPreferences preferences;
    EditorWorkspace workspace;
    int fontSize = 100;
    int executed = 0;
    EditorUiActions actions;
    actions.command = [&](EditorCommand command) {
        CHECK(command == EditorCommand::Open);
        ++executed;
    };
    const auto frame = [&] {
        ImGui::NewFrame();
        const auto result = workspace.Draw(snapshot, actions, {}, preferences, fontSize);
        ImGui::Render();
        return result;
    };
    (void)frame();
    (void)frame();
    io.AddKeyEvent(ImGuiKey_F5, true);
    CHECK(frame().cancelPreviewInput);
    CHECK(executed == 1);
    io.AddKeyEvent(ImGuiKey_F5, false);
    (void)frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_P, true);
    CHECK(frame().cancelPreviewInput);
    io.AddKeyEvent(ImGuiKey_P, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_F5, true);
    CHECK(frame().cancelPreviewInput);
    CHECK(executed == 1);
    io.AddKeyEvent(ImGuiKey_F5, false);
    io.AddInputCharactersUTF8("layout inspect");
    (void)frame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    CHECK(frame().cancelPreviewInput);
    CHECK(preferences.layout == EditorWorkspaceLayout::Inspect);
    CHECK_FALSE(preferences.Visible(EditorPanel::Catalog));
    CHECK_FALSE(preferences.customDocking.empty());
}
