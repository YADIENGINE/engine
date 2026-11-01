#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include <limits>
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "ToolUi/ToolTheme.h"

TEST_CASE("Editor.Workspace.AgentLayoutKeepsViewportLargerAndConsoleInFront", "[engine-editor]") {
    for (const auto size : {ImVec2{1440, 900}, ImVec2{2560, 1440}}) {
        CAPTURE(size.x, size.y);
        const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                      ImGui::DestroyContext);
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = size;
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontDefaultVector();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(ImTextureID{1});
        ApplyToolTheme();
        const std::vector<SceneDefinition> catalog;
        const SceneParameters parameters;
        const EditorDiagnostics diagnostics;
        const std::string status;
        const EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
        EditorPreferences preferences;
        preferences.layout = EditorWorkspaceLayout::Agent;
        preferences.Visible(EditorPanel::Catalog) = false;
        preferences.Visible(EditorPanel::Tuning) = false;
        preferences.Visible(EditorPanel::Console) = true;
        EditorWorkspace workspace;
        int fontSize = 100;
        for (int frame = 0; frame < 4; ++frame) {
            ImGui::NewFrame();
            (void)workspace.Draw(snapshot, {}, {}, preferences, fontSize);
            ImGui::Render();
        }
        const auto* view = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Viewport).window);
        const auto* console = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Console).window);
        const auto* inspector = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Inspector).window);
        REQUIRE(view);
        REQUIRE(console);
        REQUIRE(inspector);
        CHECK(view->Size.x > console->Size.x);
        CHECK(console->DockId == inspector->DockId);
        CHECK(console->DockTabIsVisible);
        CHECK_FALSE(inspector->DockTabIsVisible);
    }
}

TEST_CASE("Editor.Agent.SetupAcceptsAvailableCommandAndCancelPreservesPreferences", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavCursorVisibleAlways = true;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();
    AgentPreferences preferences{AgentKind::Custom, "cmd", false};
    const auto before = preferences;
    EditorAgentSetup setup;
    const auto frame = [&] {
        ImGui::NewFrame();
        const auto result = setup.Draw(preferences);
        ImGui::Render();
        return result;
    };
    setup.Open(preferences);
    CHECK(frame() == EditorAgentSetupResult::None);
    CHECK(setup.Active());
    CHECK(preferences == before);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    CHECK(frame() == EditorAgentSetupResult::Canceled);
    CHECK(preferences == before);
    io.AddKeyEvent(ImGuiKey_Escape, false);
    (void)frame(); // Let the closed popup leave ImGui's active-window set before reopening it.
    setup.Open(preferences);
    (void)frame();
    (void)frame();
    // The available primary action gets default keyboard focus.
    io.AddKeyEvent(ImGuiKey_Enter, true);
    CHECK(frame() == EditorAgentSetupResult::Started);
    CHECK(preferences.configured);
    CHECK_FALSE(preferences.control);
    CHECK_FALSE(setup.Active());
}

TEST_CASE("Editor.Catalog.WorkCategorySortsFirstInProjectPanel", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {800, 600};
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();
    SceneDefinition ordinary;
    ordinary.id = "myproject.alpha";
    ordinary.name = "Ordinary subject";
    ordinary.category = "Alpha"; // Alphabetically before Work, with a negative explicit order.
    ordinary.categoryOrder = std::numeric_limits<int>::lowest() + 1;
    SceneDefinition work;
    work.id = "myproject.work.subject";
    work.name = "Work subject";
    work.category = WorkSceneCategory;
    work.categoryOrder = WorkSceneCategoryOrder;
    const std::vector catalog{ordinary, work};
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    CatalogPanel panel;
    EditorPreferences preferences;
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({700, 500});
    ImGui::Begin("Project");
    ImGui::LogToBuffer();
    panel.Draw(snapshot, {}, preferences);
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    REQUIRE(text.find("Work") != std::string::npos);
    REQUIRE(text.find("Alpha") != std::string::npos);
    CHECK(text.find("Work") < text.find("Alpha"));
}

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
    ApplyToolTheme();
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
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

TEST_CASE("Editor.Workspace.ShortcutsCancelViewportInputAndPaletteOwnsKeys", "[engine-editor]") {
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
    ApplyToolTheme();
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
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
    CHECK(frame().cancelViewportInput);
    CHECK(executed == 1);
    io.AddKeyEvent(ImGuiKey_F5, false);
    (void)frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_P, true);
    CHECK(frame().cancelViewportInput);
    io.AddKeyEvent(ImGuiKey_P, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_F5, true);
    CHECK(frame().cancelViewportInput);
    CHECK(executed == 1);
    io.AddKeyEvent(ImGuiKey_F5, false);
    (void)frame(); // Apply the palette's search focus before injecting text.
    io.AddInputCharactersUTF8("layout inspect");
    (void)frame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    CHECK(frame().cancelViewportInput);
    CHECK(preferences.layout == EditorWorkspaceLayout::Inspect);
    CHECK_FALSE(preferences.Visible(EditorPanel::Catalog));
    CHECK_FALSE(preferences.customDocking.empty());
}

TEST_CASE("Editor.Workspace.StartsWithViewAndTogglesBottomPanelsIndependently", "[engine-editor]") {
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
    ApplyToolTheme();
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    EditorPreferences preferences;
    preferences.Visible(EditorPanel::Viewport) = false;
    preferences.Visible(EditorPanel::Profiler) = true;
    preferences.lastBottomPanel = EditorPanel::Profiler;
    preferences.docking = "[Window][editor.viewport]\nPos=0,0\nSize=640,480\nCollapsed=1\n";
    EditorWorkspace workspace;
    EditorUiActions actions;
    bool acknowledged = false;
    actions.acknowledgeOutput = [&] { acknowledged = true; };
    int fontSize = 100;
    const auto frame = [&] {
        acknowledged = false;
        ImGui::NewFrame();
        (void)workspace.Draw(snapshot, actions, {}, preferences, fontSize);
        ImGui::Render();
        if (acknowledged) diagnostics.Acknowledge();
    };
    frame();
    frame();
    CHECK(preferences.Visible(EditorPanel::Viewport));
    const auto* view = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Viewport).window);
    REQUIRE(view);
    CHECK_FALSE(view->Collapsed);
    CHECK(view->DockTabIsVisible);
    diagnostics.Append("warning", EditorLogLevel::Warning);
    diagnostics.Append("error", EditorLogLevel::Error);
    frame();
    CHECK(diagnostics.UnreadWarnings() == 1);
    CHECK(diagnostics.UnreadErrors() == 1);
    const auto* bar = ImGui::FindWindowByName("Status###editor.status");
    REQUIRE(bar);
    const auto padding = ImGui::GetStyle().FramePadding;
    const ImVec2 outputButton{bar->WorkRect.Min.x + padding.x, bar->WorkRect.Min.y + padding.y};
    io.AddMousePosEvent(outputButton.x, outputButton.y);
    frame();
    const auto clickOutput = [&] {
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        frame();
    };
    clickOutput();
    CHECK(preferences.Visible(EditorPanel::Output));
    CHECK(preferences.Visible(EditorPanel::Profiler));
    frame(); // Bir dock sekmesinin seçilmesi sonraki karede uygulanabilir.
    CHECK(diagnostics.UnreadErrors() == 0);
    CHECK(diagnostics.UnreadWarnings() == 0);
    clickOutput();
    CHECK_FALSE(preferences.Visible(EditorPanel::Output));
    CHECK(preferences.Visible(EditorPanel::Profiler));
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_J, true);
    frame();
    CHECK(preferences.Visible(EditorPanel::Output));
    CHECK(preferences.Visible(EditorPanel::Profiler));
}
