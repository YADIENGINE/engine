#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include <limits>
#include "Editor/Ui/Panels/Assets/AssetsPanel.h"
#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "ToolUi/ToolTheme.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Editor.Console.SessionListSelectsResizesAndRequestsNewSessions", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1000, 700};
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();
    ConsolePanel panel;
    ImGuiWindow* list = nullptr;
    ImGuiWindow* terminal = nullptr;
    ImGuiWindow* rows = nullptr;
    float rowHeight = 0;
    float rowStride = 0;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20});
        ImGui::SetNextWindowSize({900, 500});
        ImGui::Begin("Console");
        rowHeight = ImGui::GetFrameHeight();
        rowStride = rowHeight + ImGui::GetStyle().ItemSpacing.y;
        const auto request = panel.Draw();
        auto* console = ImGui::GetCurrentWindow();
        for (auto* child : console->DC.ChildWindows) {
            if (child->ChildId == console->GetID("console-session-list")) list = child;
            if (child->ChildId == console->GetID("console-terminal")) terminal = child;
        }
        if (list)
            for (auto* child : list->DC.ChildWindows)
                if (child->ChildId == list->GetID("sessions")) rows = child;
        ImGui::End();
        ImGui::Render();
        return request;
    };
    const auto click = [&](ImVec2 point, int button = ImGuiMouseButton_Left) {
        io.AddMousePosEvent(point.x, point.y);
        (void)frame();
        io.AddMouseButtonEvent(button, true);
        (void)frame();
        io.AddMouseButtonEvent(button, false);
        return frame();
    };
    (void)frame();
    (void)frame();
    REQUIRE(panel.Sessions().size() == 1); // First draw still opens a shell.
    REQUIRE(list);
    REQUIRE(terminal);
    REQUIRE(rows);
    CHECK(terminal->Pos.x + terminal->Size.x <= list->Pos.x);
    CHECK(terminal->Size.x > list->Size.x);
    panel.OpenShell();
    (void)frame();
    REQUIRE(panel.Selected() == 1);
    (void)click({rows->WorkRect.Min.x + 30, rows->WorkRect.Min.y + rowHeight * 0.5f});
    CHECK(panel.Selected() == 0);
    CHECK(panel.Sessions()[0].selected);

    const float previousWidth = list->Size.x;
    const ImVec2 splitter{(terminal->Pos.x + terminal->Size.x + list->Pos.x) * 0.5f, list->Pos.y + 100};
    io.AddMousePosEvent(splitter.x, splitter.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    (void)frame();
    io.AddMousePosEvent(splitter.x - 40, splitter.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    (void)frame();
    CHECK(list->Size.x > previousWidth + 30);
    const float resizedWidth = list->Size.x;
    (void)frame();
    CHECK(list->Size.x == resizedWidth);

    const std::vector<std::optional<AgentKind>> kinds{std::nullopt, AgentKind::ClaudeCode, AgentKind::Codex,
                                                      AgentKind::Custom};
    for (std::size_t index = 0; index < kinds.size(); ++index) {
        CAPTURE(index);
        (void)click({list->WorkRect.Min.x + 40, list->WorkRect.Min.y + rowHeight * 0.5f});
        (void)frame(); // Let the popup settle at its final size and position.
        REQUIRE_FALSE(context->OpenPopupStack.empty());
        const auto* popup = context->OpenPopupStack.back().Window;
        REQUIRE(popup);
        const float stride = ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y;
        const auto request = click(
            {popup->WorkRect.Min.x + 40, popup->WorkRect.Min.y + stride * static_cast<float>(index) + stride * 0.5f});
        REQUIRE(request);
        CHECK(request->agent == kinds[index]);
        CHECK(panel.Sessions().size() == 2); // Workspace, not Draw, starts the requested program.
        (void)frame();
    }
    // Closing another row leaves the selected session selected.
    (void)click({rows->WorkRect.Max.x - rowHeight * 0.5f, rows->WorkRect.Min.y + rowStride + rowHeight * 0.5f});
    CHECK(panel.Sessions().size() == 1);
    CHECK(panel.Selected() == 0);

    // A shell labelled as an agent exercises confirmation without requiring an installed agent CLI.
    panel.AddAgent(AgentKind::Codex, {});
    (void)frame();
    REQUIRE(panel.Sessions()[1].state != SessionState::Exited);
    (void)click({rows->WorkRect.Min.x + 30, rows->WorkRect.Min.y + rowStride + rowHeight * 0.5f},
                ImGuiMouseButton_Right);
    (void)frame();
    REQUIRE_FALSE(context->OpenPopupStack.empty());
    const auto* closeMenu = context->OpenPopupStack.back().Window;
    REQUIRE(closeMenu);
    (void)click({closeMenu->WorkRect.Min.x + 30, closeMenu->WorkRect.Min.y + 5});
    (void)frame();
    REQUIRE(ImGui::GetTopMostPopupModal());
    CHECK_FALSE(panel.Focused());
    CHECK(panel.Sessions().size() == 2);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    (void)frame();
    CHECK_FALSE(ImGui::GetTopMostPopupModal());
    CHECK(panel.Sessions().size() == 2);
    (void)click({rows->WorkRect.Max.x - rowHeight * 0.5f, rows->WorkRect.Min.y + rowStride + rowHeight * 0.5f});
    (void)frame();
    const auto* confirmation = ImGui::GetTopMostPopupModal();
    REQUIRE(confirmation);
    (void)click({confirmation->WorkRect.Min.x + 40, confirmation->WorkRect.Max.y - rowHeight * 0.5f});
    CHECK(panel.Sessions().size() == 1);
}

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

TEST_CASE("Editor.Assets.TreeRowsLeaveNoGapBetweenThem", "[engine-editor]") {
    TempDirectory temp;
    std::filesystem::create_directories(temp.Path() / "Game" / "Assets" / "Graphics");
    std::filesystem::create_directories(temp.Path() / "Game" / "Assets" / "Audio");
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1000, 700};
    io.DeltaTime = 1.0f / 60.0f;
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
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Ready, status};
    AssetsPanel panel;
    panel.SetRoots(temp.Path() / "Engine", temp.Path() / "Game" / "Assets");
    EditorUiActions actions;
    bool hovered = false;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1000, 700});
        ImGui::Begin("Assets");
        panel.Draw(snapshot, actions);
        hovered = ImGui::IsAnyItemHovered();
        ImGui::End();
        ImGui::Render();
    };
    (void)frame();
    (void)frame();
    // Walk down the tree: once the first row is hit, every point until the last row is on some row.
    constexpr float TreeX = 100;
    float firstRow = -1;
    float lastRow = -1;
    int misses = 0;
    for (float y = 80; y < 400; y += 2) { // Below the toolbar
        io.AddMousePosEvent(TreeX, y);
        (void)frame();
        if (hovered) {
            if (firstRow < 0) firstRow = y;
            if (lastRow >= 0 && y - lastRow > 2) ++misses;
            lastRow = y;
        }
    }
    REQUIRE(firstRow >= 0);
    CHECK(misses == 0);
    CHECK(lastRow - firstRow > 100); // Five rows: Engine, Graphics, Game, Audio, Graphics
}
