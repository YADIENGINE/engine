#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "Editor/Ui/Commands/EditorActions.h"
#include "Editor/Ui/Panels/Agents/AgentsDrawer.h"
#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "ToolUi/ToolTheme.h"

namespace {
struct ContextUi {
    std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> gui{ImGui::CreateContext(), ImGui::DestroyContext};
    std::vector<SceneDefinition> catalog{SceneDefinition{}};
    SceneParameters parameters;
    EditorDiagnostics diagnostics;
    std::string status;
    EditorSnapshot snapshot{catalog, &catalog.front(), parameters, {}, diagnostics, {}, EditorPhase::Ready, status};
    EditorPreferences preferences;
    int fontSize = 100;
    ContextUi() {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1000, 700};
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontDefaultVector();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(ImTextureID{1});
        ApplyToolTheme();
        catalog.front().id = "example.scene";
        snapshot.ready = true;
        snapshot.canvas = {800, 450};
        snapshot.capabilities.enabled[static_cast<std::size_t>(EditorCommand::Pause)] = true;
        preferences.visible.fill(false);
    }
};
} // namespace

TEST_CASE("Editor.Context.TargetPreviewsKeepDrawerAndGeneralRoutingSeparate", "[engine-editor]") {
    std::vector<ConsoleSessionInfo> infos{
        {.id = 1, .title = "Codex", .agent = AgentKind::Codex, .selected = true, .target = true},
        {.id = 2, .title = "Codex 2", .agent = AgentKind::Codex},
        {.id = 3, .title = "Shell"}};
    CHECK(AgentContextTarget(infos, AgentKind::Codex).session == 1);
    CHECK(SessionContextTarget(infos, 2).title == "Codex 2");
    CHECK(PasteContextTarget(infos).session == 1);
    CHECK(infos[0].selected);
    CHECK_FALSE(infos[1].selected);
    CHECK(AgentContextTarget(infos, AgentKind::ClaudeCode).title == "New Claude session");
    CHECK_FALSE(AgentContextTarget(infos, AgentKind::ClaudeCode).session);
    infos[0].selected = false;
    infos[2].selected = true;
    CHECK(PasteContextTarget(infos).session == 1); // Last agent target still beats a selected shell.
    infos[0].target = false;
    CHECK(PasteContextTarget(infos).title == "Shell");
    infos[1].state = SessionState::Exited;
    CHECK_FALSE(SessionContextTarget(infos, 2).unavailable.empty());
    CHECK_FALSE(SessionContextTarget(infos, std::nullopt).unavailable.empty());
    CHECK_FALSE(SessionContextTarget(infos, 3).session); // A shell is never a drawer agent.
    CHECK(PasteContextTarget({}).title == "New Shell session");
}

TEST_CASE("Editor.Context.CommandsNameTheirRealTargetAndKeepPasteAndSubmitDistinct", "[engine-editor]") {
    ContextUi fixture;
    fixture.diagnostics.BeginAttempt();
    fixture.diagnostics.FinishAttempt(EditorBuildResult::BuildFailed);
    EditorShellResult result;
    EditorActionContext context{.snapshot = fixture.snapshot,
                                .actions = {},
                                .preferences = fixture.preferences,
                                .fontSize = fixture.fontSize,
                                .agentsDrawerOpen = true,
                                .agentContextTarget = "Codex",
                                .sceneContextTarget = "Codex 2",
                                .result = result};
    const auto list = EditorActionList(context);
    const auto find = [&](const char* label) -> const EditorAction& {
        const auto found = std::ranges::find(list, label, &EditorAction::label);
        REQUIRE(found != list.end());
        return *found;
    };
    find("Paste scene context to Codex 2").run();
    CHECK(result.agents.sendScene);
    CHECK_FALSE(result.agent);
    find("Paste build failure to Codex").run();
    CHECK(result.agent == EditorAgentRequest::SendBuildFailure);
    find("Ask agent to fix build with Codex").run();
    CHECK(result.agent == EditorAgentRequest::FixBuild);
    context.sceneContextUnavailable = "Restart this agent.";
    const auto disabled = EditorActionList(context);
    const auto scene = std::ranges::find(disabled, EditorKey::SendScene, &EditorAction::key);
    REQUIRE(scene != disabled.end());
    CHECK_FALSE(scene->enabled);
    CHECK(scene->reason == "Restart this agent.");
}

TEST_CASE("Editor.Context.DeliveryWaitsForTerminalInputAndNeverSucceedsForAnExitedSession", "[engine-editor]") {
    ConsoleSessions sessions;
    const auto id = sessions.AddAgent(AgentKind::Custom, "__missing_editor_test_program__.exe");
    sessions.SendTo(id, "scene context", false, "Scene context");
    CHECK(sessions.TakeContextDeliveries().empty());
    sessions.Update(nullptr);
    auto delivery = sessions.TakeContextDeliveries();
    REQUIRE(delivery.size() == 1);
    CHECK_FALSE(delivery.front().problem.empty());
    sessions.Paste("mark", "Scene mark");
    delivery = sessions.TakeContextDeliveries();
    REQUIRE(delivery.size() == 1);
    CHECK_FALSE(delivery.front().problem.empty());
    sessions.SendTo(id, "fix", true, "Fix request");
    sessions.Close(id);
    delivery = sessions.TakeContextDeliveries();
    REQUIRE(delivery.size() == 1);
    CHECK_FALSE(delivery.front().problem.empty());
}

TEST_CASE("Editor.Context.DrawerNamesItsSelectedTargetInBothViews", "[engine-editor]") {
    ContextUi fixture;
    const std::vector<ConsoleSessionInfo> infos{
        {.id = 1, .title = "Codex", .agent = AgentKind::Codex, .selected = true, .target = true},
        {.id = 2, .title = "Codex 2", .agent = AgentKind::Codex}};
    AgentsDrawer drawer;
    ConsoleViews views;
    drawer.Open(2);
    for (int view = 0; view < 2; ++view) {
        ImGui::NewFrame();
        ImGui::LogToBuffer();
        (void)drawer.Draw(infos, {}, views, fixture.snapshot, {}, {0, 40}, {1000, 660});
        const std::string log = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::Render();
        CHECK(log.find("Scene context to: Codex 2") != std::string::npos);
        CHECK(log.find("Paste only") != std::string::npos);
        CHECK(log.find("Paste scene context") != std::string::npos);
        drawer.SwitchView();
    }
}

TEST_CASE("Editor.Mark.AltDragRequestsAnExplicitPauseAndCancelBeatsRelease", "[engine-editor]") {
    ContextUi fixture;
    ViewportPanel panel;
    auto& io = ImGui::GetIO();
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Viewport");
        const auto result = panel.Draw(fixture.snapshot, {}, {1, 800, 450}, {}, false, false);
        ImGui::End();
        ImGui::Render();
        return result;
    };
    auto result = frame();
    const auto& viewport = result.viewport;
    io.AddMousePosEvent(viewport.position.x + viewport.size.x * 0.5f, viewport.position.y + viewport.size.y * 0.5f);
    io.AddKeyEvent(ImGuiMod_Alt, true);
    (void)frame();
    SECTION("Running scene") {
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        result = frame();
        CHECK(result.pauseForMarking);
        fixture.snapshot.paused = true;
        io.AddKeyEvent(ImGuiKey_Escape, true);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        result = frame();
        CHECK_FALSE(result.mark);
        CHECK_FALSE(result.markDragging);
        CHECK_FALSE(result.pauseForMarking);
    }
    SECTION("Previously paused scene") {
        fixture.snapshot.paused = true;
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        result = frame();
        CHECK_FALSE(result.pauseForMarking);
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        result = frame();
        CHECK_FALSE(result.mark);
    }
}

TEST_CASE("Editor.Context.NoticesRemainVisibleWithoutViewportOrDrawerAndSetupCancelIsNotSuccess", "[engine-editor]") {
    ContextUi fixture;
    EditorWorkspace workspace;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::LogToBuffer();
        (void)workspace.Draw(fixture.snapshot, {}, {}, fixture.preferences, fixture.fontSize);
        const std::string log = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::Render();
        return log;
    };
    (void)frame();
    fixture.preferences.visible.fill(false);
    workspace.NotifyContext("Scene mark capture failed: missing frame", true);
    CHECK(frame().find("Scene mark capture failed: missing frame") != std::string::npos);
    const auto* notice = ImGui::FindWindowByName("##editor-context-notice");
    REQUIRE(notice);
    CHECK((notice->Flags & ImGuiWindowFlags_NoInputs) == ImGuiWindowFlags_NoInputs);
    CHECK(workspace.Sessions().Empty());
    fixture.preferences.agent = {AgentKind::Custom, "__missing_editor_test_program__.exe", false};
    (void)workspace.SendToAgent("scene", false, fixture.preferences, "Scene context");
    const auto waiting = frame();
    CHECK(waiting.find("waiting for agent setup") != std::string::npos);
    CHECK(waiting.find("pasted to") == std::string::npos);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    const auto canceled = frame();
    CHECK(canceled.find("Context paste canceled") != std::string::npos);
    CHECK(canceled.find("pasted to") == std::string::npos);
    CHECK(workspace.Sessions().Empty());
}
