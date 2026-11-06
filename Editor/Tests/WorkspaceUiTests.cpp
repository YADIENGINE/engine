#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <algorithm>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include <limits>
#include "Editor/Ui/Panels/Agents/AgentsDrawer.h"
#include "Editor/Ui/Panels/Assets/AssetsPanel.h"
#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
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
    ConsoleSessions sessions;
    ConsoleViews views;
    ConsolePanel panel;
    // The selected session's row, counted from the top of the list.
    const auto selectedRow = [&] {
        const auto infos = sessions.Infos();
        return static_cast<std::size_t>(
            std::ranges::find_if(infos, [](const ConsoleSessionInfo& info) { return info.selected; }) - infos.begin());
    };
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
        const auto request = panel.Draw(sessions, views).add;
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
    REQUIRE(sessions.Infos().size() == 1); // First draw still opens a shell.
    REQUIRE(list);
    REQUIRE(terminal);
    REQUIRE(rows); // One session is listed like any number of them, so the list keeps its look.
    CHECK(terminal->Pos.x + terminal->Size.x <= list->Pos.x);
    CHECK(terminal->Size.x > list->Size.x);
    const float oneSessionWidth = list->Size.x;
    sessions.OpenShell();
    (void)frame();
    REQUIRE(selectedRow() == 1);
    CHECK(list->Size.x == oneSessionWidth);
    (void)click({rows->WorkRect.Min.x + 30, rows->WorkRect.Min.y + rowHeight * 0.5f});
    CHECK(selectedRow() == 0);
    CHECK(sessions.Infos()[0].selected);

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
        CHECK(sessions.Infos().size() == 2); // Workspace, not Draw, starts the requested program.
        (void)frame();
    }
    // Closing another row leaves the selected session selected.
    (void)click({rows->WorkRect.Max.x - rowHeight * 0.5f, rows->WorkRect.Min.y + rowStride + rowHeight * 0.5f});
    CHECK(sessions.Infos().size() == 1);
    CHECK(selectedRow() == 0);

    // A shell labelled as an agent exercises confirmation without requiring an installed agent CLI.
    sessions.AddAgent(AgentKind::Codex, {});
    (void)frame();
    REQUIRE(sessions.Infos()[1].state != SessionState::Exited);
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
    CHECK(sessions.Infos().size() == 2);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    (void)frame();
    CHECK_FALSE(ImGui::GetTopMostPopupModal());
    CHECK(sessions.Infos().size() == 2);
    (void)click({rows->WorkRect.Max.x - rowHeight * 0.5f, rows->WorkRect.Min.y + rowStride + rowHeight * 0.5f});
    (void)frame();
    const auto* confirmation = ImGui::GetTopMostPopupModal();
    REQUIRE(confirmation);
    (void)click({confirmation->WorkRect.Min.x + 40, confirmation->WorkRect.Max.y - rowHeight * 0.5f});
    CHECK(sessions.Infos().size() == 1);
}

TEST_CASE("Editor.Agents.DrawerKeepsOrderShowsOneTerminalAndStartsAgents", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1400, 800};
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();

    ConsoleSessions console;
    const auto workingId = console.AddAgent(AgentKind::ClaudeCode, {});
    const auto waitingId = console.AddAgent(AgentKind::Codex, {}); // Selected in the Console
    const auto shellId = console.OpenShell();
    console.Select(workingId);
    const auto now = std::chrono::steady_clock::now();
    std::vector<ConsoleSessionInfo> infos{
        {.id = workingId,
         .title = "Working agent",
         .agent = AgentKind::ClaudeCode,
         .state = SessionState::Working,
         .started = now - std::chrono::minutes(8)},
        {.id = waitingId,
         .title = "Waiting agent",
         .agent = AgentKind::Codex,
         .state = SessionState::Waiting,
         .started = now - std::chrono::minutes(5)},
        {.id = shellId, .title = "Shell", .state = SessionState::Idle, .started = now}};
    EditorAgentReport report;
    report.sessions[std::to_string(waitingId)].note = AgentNote{AgentActivityState::Waiting, "Choose the target"};
    report.sessions[std::to_string(waitingId)].noteTime = now;
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    std::deque<EditorControlEvent> events;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    snapshot.agents = &report;
    snapshot.controlEvents = &events;
    AgentsDrawer drawer;
    ConsoleViews views;
    std::string log;
    ImGuiWindow* list = nullptr;
    const auto frame = [&](bool captureLog = false) {
        ImGui::NewFrame();
        if (captureLog) ImGui::LogToBuffer();
        const auto request = drawer.Draw(infos, {}, views, snapshot, {}, {0, 40}, {1400, 760});
        if (captureLog) {
            log = ImGui::GetCurrentContext()->LogBuffer.c_str();
            ImGui::LogFinish();
        }
        if (auto* window = ImGui::FindWindowByName("Agents###editor.agents-drawer"))
            for (auto* child : window->DC.ChildWindows)
                if (child->ChildId == window->GetID("agents-chips")) list = child;
        ImGui::Render();
        return request;
    };
    const auto click = [&](ImVec2 point) {
        io.AddMousePosEvent(point.x, point.y);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        return frame();
    };

    CHECK_FALSE(drawer.Shown(infos)); // Closed: the Console keeps every terminal
    drawer.Open(waitingId);
    REQUIRE(drawer.Shown(infos) == waitingId);
    (void)frame(true);
    REQUIRE(list);
    // Rows keep the Console's order however the states change; shells stay in the Console
    REQUIRE(log.find("Working agent") != std::string::npos);
    CHECK(log.find("Working agent") < log.find("Waiting agent"));
    CHECK(log.find("Shell") == std::string::npos);
    CHECK(log.find("Outside the Console") == std::string::npos);

    // Choosing a chip shows its terminal there and leaves the Console's selection alone
    (void)click({list->WorkRect.Min.x + 12, list->WorkRect.Min.y + ImGui::GetFrameHeight() * 0.5f});
    CHECK(drawer.Shown(infos) == workingId);
    CHECK(console.Selected() == workingId);
    console.Select(waitingId);
    (void)frame();
    CHECK(drawer.Shown(infos) == workingId);

    // What agents outside the Console did gets its row, which has no terminal
    events.push_back({.time = std::chrono::system_clock::now(), .text = "Opened a scene", .session = ""});
    (void)frame(true);
    CHECK(log.find("Outside the") != std::string::npos);

    // Keys: chips by number, the next and previous one (around the end), Terminal / Activity
    drawer.Pick(1);
    CHECK(drawer.Shown(infos) == waitingId);
    CHECK(drawer.ChosenSession() == waitingId);
    drawer.Step(1); // To the outside chip: no terminal, no session to send to
    (void)frame();
    CHECK_FALSE(drawer.Shown(infos));
    CHECK_FALSE(drawer.ChosenSession());
    drawer.Step(1);
    CHECK(drawer.Shown(infos) == workingId);
    drawer.Step(-1);
    drawer.Step(-1);
    CHECK(drawer.ChosenSession() == waitingId);
    drawer.Pick(8); // No ninth chip: nothing changes
    CHECK(drawer.ChosenSession() == waitingId);
    drawer.SwitchView(); // Activity draws no terminal, so the Console may show it
    CHECK_FALSE(drawer.Shown(infos));
    drawer.SwitchView();
    CHECK(drawer.Shown(infos) == waitingId);

    // The bottom-left corner resizes it, also on Activity, whose list once covered the corner
    drawer.SwitchView();
    (void)frame();
    const auto* window = ImGui::FindWindowByName("Agents###editor.agents-drawer");
    REQUIRE(window);
    const ImVec2 before = window->Size;
    const ImVec2 corner{window->Pos.x + 5, window->Pos.y + window->Size.y - 5};
    io.AddMousePosEvent(corner.x, corner.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    (void)frame();
    io.AddMousePosEvent(corner.x - 60, corner.y + 40);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    (void)frame();
    (void)frame();
    CHECK(window->Size.x > before.x + 40);
    CHECK(window->Size.y > before.y + 20);
    drawer.SwitchView();

    drawer.Toggle(std::nullopt);
    CHECK_FALSE(drawer.IsOpen());
    CHECK_FALSE(drawer.Shown(infos));

    // With no agent, the drawer offers to start one
    infos.resize(0);
    events.clear();
    drawer.Open();
    (void)frame(true);
    CHECK(log.find("No agent running") != std::string::npos);
    CHECK(log.find("Custom command") != std::string::npos);
}

TEST_CASE("Editor.Agents.DrawerMenusOpenInFrontOfIt", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
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
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorAgentReport report;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    snapshot.agents = &report;
    EditorPreferences preferences;
    // A custom agent that is only a shell, so the drawer starts a session without an agent CLI installed
    preferences.agent.kind = AgentKind::Custom;
    preferences.agent.customCommand = "cmd /d /k";
    preferences.agent.configured = true;
    EditorWorkspace workspace;
    int fontSize = 100;
    const auto frame = [&] {
        ImGui::NewFrame();
        (void)workspace.Draw(snapshot, {}, {}, preferences, fontSize);
        ImGui::Render();
    };
    const auto drawerWindow = [] { return ImGui::FindWindowByName("Agents###editor.agents-drawer"); };
    (void)frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    (void)frame();
    // No agent yet: the drawer opens on its empty state, and Enter starts the usual agent
    REQUIRE(drawerWindow());
    CHECK(workspace.Sessions().Empty());
    io.AddKeyEvent(ImGuiKey_Enter, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    for (int index = 0; index < 3; ++index)
        (void)frame();
    CHECK_FALSE(workspace.Sessions().Empty());
    auto* drawer = drawerWindow();
    REQUIRE(drawer);
    REQUIRE(drawer->Active);
    ImGuiWindow* chips = nullptr;
    for (auto* child : drawer->DC.ChildWindows)
        if (child->ChildId == drawer->GetID("agents-chips")) chips = child;
    REQUIRE(chips);
    // "+" ends the chip row: its menu must open over the drawer and stay there
    const ImVec2 add{chips->Pos.x + chips->ContentSize.x - ImGui::GetFrameHeight() * 0.5f,
                     chips->Pos.y + ImGui::GetFrameHeight() * 0.5f};
    io.AddMousePosEvent(add.x, add.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    for (int index = 0; index < 3; ++index)
        (void)frame();
    auto& g = *ImGui::GetCurrentContext();
    REQUIRE_FALSE(g.OpenPopupStack.empty());
    auto* menu = g.OpenPopupStack.back().Window;
    REQUIRE(menu);
    CHECK(ImGui::FindWindowDisplayIndex(menu) > ImGui::FindWindowDisplayIndex(drawerWindow()));
}

TEST_CASE("Editor.Console.ShowsWhereTheDrawerHasATerminal", "[engine-editor]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 700};
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    ApplyToolTheme();
    ConsoleSessions sessions;
    ConsoleViews views;
    ConsolePanel panel;
    const auto agent = sessions.AddAgent(AgentKind::Codex, {});
    std::string log;
    ImGuiWindow* terminal = nullptr;
    const auto frame = [&](std::optional<std::uint64_t> elsewhere) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20});
        ImGui::SetNextWindowSize({900, 500});
        ImGui::Begin("Console");
        ImGui::LogToBuffer();
        const auto request = panel.Draw(sessions, views, elsewhere);
        log = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        auto* window = ImGui::GetCurrentWindow();
        for (auto* child : window->DC.ChildWindows)
            if (child->ChildId == window->GetID("console-terminal")) terminal = child;
        ImGui::End();
        ImGui::Render();
        return request;
    };
    // The drawer shows the selected session: the Console says so instead of drawing the screen a second time
    (void)frame(agent);
    CHECK(log.find("is open in Agents") != std::string::npos);
    CHECK(log.find("Show here") != std::string::npos);
    REQUIRE(terminal);
    (void)frame(std::nullopt);
    CHECK(log.find("is open in Agents") == std::string::npos);
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

TEST_CASE("Editor.Catalog.CategoryIsOneGroupWhateverItsEntriesOrders", "[engine-editor]") {
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
    const auto scene = [](const char* id, const char* category, int order) {
        SceneDefinition definition;
        definition.id = id;
        definition.name = id;
        definition.category = category;
        definition.categoryOrder = order;
        return definition;
    };
    // Shared is split by order and has Middle between its entries; General also arrives as an empty category.
    const std::vector catalog{scene("one", "Shared", -1), scene("two", "Middle", 0), scene("three", "Shared", 0),
                              scene("four", "", 0), scene("five", "General", 0)};
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
    const auto occurrences = [&](const std::string& word) {
        std::size_t count = 0;
        for (auto at = text.find(word); at != std::string::npos; at = text.find(word, at + word.size()))
            ++count;
        return count;
    };
    CHECK(occurrences("Shared") == 1);
    CHECK(occurrences("General") == 1);
    // A category sorts by its lowest order.
    CHECK(text.find("Shared") < text.find("General"));
    CHECK(text.find("Shared") < text.find("Middle"));
}

TEST_CASE("Editor.Catalog.FavoriteControlOwnsItsClick", "[engine-editor]") {
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
    SceneDefinition scene;
    scene.id = "myproject.scene";
    scene.name = "Scene";
    scene.category = "General";
    const std::vector catalog{scene};
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    snapshot.selected = &catalog.front();
    CatalogPanel panel;
    EditorPreferences preferences;
    bool opened = false;
    EditorUiActions actions;
    actions.select = [&](const std::string&) { opened = true; };
    actions.command = [&](EditorCommand) { opened = true; };
    ImGuiWindow* entries = nullptr;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20});
        ImGui::SetNextWindowSize({400, 500});
        ImGui::Begin("Project");
        panel.Draw(snapshot, actions, preferences);
        auto* project = ImGui::GetCurrentWindow();
        for (auto* child : project->DC.ChildWindows)
            if (child->ChildId == project->GetID("catalog-entries")) entries = child;
        const float scale = ToolUiScale();
        const float starSize = ImGui::GetTextLineHeight() + 4 * scale;
        const ImVec2 star{entries->WorkRect.Max.x - ToolMetrics::Space * scale - starSize * 0.5f,
                          entries->DC.CursorPos.y - scale - starSize * 0.5f};
        ImGui::End();
        ImGui::Render();
        return star;
    };
    const auto star = frame();
    REQUIRE(entries);
    io.AddMousePosEvent(star.x, star.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    (void)frame();
    CHECK(preferences.IsFavorite(scene.id));
    CHECK_FALSE(opened);
}

TEST_CASE("Editor.Widgets.RepeatedChoiceNamesDoNotConflict", "[engine-editor]") {
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
    SceneParameter parameter;
    parameter.kind = SceneParameterKind::Choice;
    parameter.choiceButtons = true;
    parameter.choices = {"Same", "Same"};
    SceneValue value = std::string("Other");
    bool conflict = false;
    // ImGui only reports a conflict on the hovered item, a frame after it hovers.
    for (int frame = 0; frame < 4; ++frame) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({700, 500});
        ImGui::Begin("Inspector");
        const auto start = ImGui::GetCursorScreenPos();
        io.AddMousePosEvent(start.x + 8, start.y + 8);
        (void)EditorParameterWidget(parameter, value);
        ImGui::End();
        ImGui::Render();
        conflict = conflict || ImGui::GetCurrentContext()->HoveredIdPreviousFrameItemCount > 1 ||
                   ImGui::GetCurrentContext()->DebugDrawIdConflictsId != 0;
    }
    CHECK(ImGui::GetCurrentContext()->HoveredIdPreviousFrame != 0);
    CHECK_FALSE(conflict);
}

TEST_CASE("Editor.Workspace.BuildFailureShowsOutputOncePerAttempt", "[engine-editor]") {
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
    actions.acknowledgeOutput = [] {};
    int fontSize = 100;
    // Whether Output came forward: it takes a frame to notice the failure and another to apply it.
    const auto outputShown = [&] {
        for (int frame = 0; frame < 2; ++frame) {
            ImGui::NewFrame();
            (void)workspace.Draw(snapshot, actions, {}, preferences, fontSize);
            ImGui::Render();
        }
        const bool shown = preferences.Visible(EditorPanel::Output);
        preferences.Visible(EditorPanel::Output) = false;
        return shown;
    };
    CHECK_FALSE(outputShown());
    diagnostics.BeginAttempt();
    diagnostics.Append("compile error", EditorLogLevel::Error, EditorLogSource::Build);
    diagnostics.BuildExited(1);
    diagnostics.FinishAttempt(EditorBuildResult::BuildFailed);
    CHECK(outputShown());
    CHECK_FALSE(outputShown()); // Once per attempt
    diagnostics.BeginAttempt();
    diagnostics.FinishAttempt(EditorBuildResult::Canceled);
    CHECK_FALSE(outputShown());
    diagnostics.BeginAttempt();
    diagnostics.BuildExited(0);
    diagnostics.Append("startup error", EditorLogLevel::Error);
    diagnostics.FinishAttempt(EditorBuildResult::RefreshFailed);
    CHECK(outputShown());
    // An attempt started through the control endpoint reports to its client instead.
    diagnostics.BeginAttempt();
    diagnostics.BuildExited(1);
    diagnostics.FinishAttempt(EditorBuildResult::BuildFailed);
    snapshot.controlAttempt = diagnostics.Attempt().id;
    CHECK_FALSE(outputShown());
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

TEST_CASE("Editor.Search.ClearTextKeepsScopeAndResetRestoresAll", "[engine-editor]") {
    const bool inspector = GENERATE(false, true);
    CAPTURE(inspector);
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
    SceneDefinition glow;
    glow.id = "myproject.glow";
    glow.name = "Glow Red";
    glow.category = "Effects";
    glow.parameters = {{"glow", "Glow Red", SceneParameterKind::Float, 1.0, 0, 4},
                       {"blue", "Unchanged Blue", SceneParameterKind::Float, 0.0, 0, 4}};
    auto blue = glow;
    blue.id = "myproject.blue";
    blue.name = "Unchanged Blue";
    const std::vector catalog{glow, blue};
    auto parameters = SceneDefaults(glow);
    parameters["glow"] = 2.0;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, &catalog.front(), parameters, {}, diagnostics, {}, EditorPhase::Ready, status};
    snapshot.ready = true;
    EditorPreferences preferences;
    preferences.ToggleFavorite(glow.id);
    CatalogPanel scenes;
    InspectorPanel properties;
    int edits = 0;
    EditorUiActions actions;
    actions.parameter = [&](const std::string&, const SceneValue&) { ++edits; };
    actions.command = [&](EditorCommand) { ++edits; };
    ImGuiID search = 0, scope = 0, showSearch = 0;
    ImVec2 clearPosition, resetPosition;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({600, 580});
        ImGui::Begin("Search test");
        ImGui::LogToBuffer();
        if (inspector)
            properties.Draw(snapshot, actions);
        else
            scenes.Draw(snapshot, actions, preferences);
        const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        if (inspector) ImGui::PushID(glow.id.c_str());
        const char* field = inspector ? "##property-search" : "##search";
        search = ImGui::GetID(field);
        const auto& gui = *ImGui::GetCurrentContext();
        if (gui.NavWindow && gui.NavId == search) {
            auto rect = gui.NavWindow->NavRectRel[ImGuiNavLayer_Main];
            rect.Translate(gui.NavWindow->Pos);
            clearPosition = {rect.Max.x + ImGui::GetStyle().ItemSpacing.x + rect.GetHeight() * 0.5f,
                             rect.GetCenter().y};
        }
        auto* window = ImGui::GetCurrentWindow();
        for (auto* child : window->DC.ChildWindows)
            if (child->ChildId == window->GetID(inspector ? "##inspector-content" : "catalog-entries"))
                resetPosition = {child->DC.CursorPosPrevLine.x - 4,
                                 child->DC.CursorPosPrevLine.y + ImGui::GetFrameHeight() * 0.5f};
        ImGui::PushID(inspector ? "Modified (1)###modified" : "favorites-filter");
        scope = ImGui::GetID(inspector ? "##segment" : "##filter");
        ImGui::PopID();
        ImGui::PushID("Search");
        showSearch = ImGui::GetID("##segment");
        ImGui::PopID();
        if (inspector) ImGui::PopID();
        ImGui::End();
        ImGui::Render();
        return text;
    };
    const auto click = [&](ImVec2 position) {
        io.AddMousePosEvent(position.x, position.y);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        (void)frame();
    };
    (void)frame();
    ImGui::ActivateItemByID(scope);
    CHECK(frame().find("Reset filters") != std::string::npos);
    if (inspector) {
        ImGui::ActivateItemByID(showSearch);
        (void)frame();
        (void)frame(); // Apply the newly opened field's focus before typing.
    }
    ImGui::ActivateItemByID(search);
    (void)frame();
    io.AddInputCharactersUTF8("red glow");
    const auto matches = frame();
    CHECK(matches.find("No matching") == std::string::npos);
    CHECK(matches.find("No modified") == std::string::npos);
    io.AddInputCharactersUTF8(" absent");
    const auto noMatches = frame();
    CHECK(noMatches.find(inspector ? "No modified properties match your search" : "No matching favorite scenes") !=
          std::string::npos);
    click(clearPosition);
    CHECK(frame().find("Reset filters") != std::string::npos);
    // Clear returns focus to the field; the unchanged / non-favorite entry remains outside the scope.
    io.AddInputCharactersUTF8("blue");
    CHECK(frame().find(inspector ? "No modified properties match your search" : "No matching favorite scenes") !=
          std::string::npos);
    click(resetPosition);
    const auto all = frame();
    CHECK(all.find("Reset filters") == std::string::npos);
    CHECK(all.find("No matching") == std::string::npos);
    CHECK(all.find("No modified") == std::string::npos);
    CHECK(preferences.IsFavorite(glow.id));
    CHECK(edits == 0);
}

TEST_CASE("Editor.Assets.SearchCoversBothRootsAndClearReturnsToFolder", "[engine-editor]") {
    TempDirectory temp;
    const auto engine = temp.Path() / "Engine";
    const auto assets = temp.Path() / "Game" / "Assets";
    std::filesystem::create_directories(engine / "Assets" / "Audio");
    std::filesystem::create_directories(assets / "Graphics");
    std::ofstream(engine / "Assets" / "Audio" / "Glow Red.wav") << "x";
    std::ofstream(assets / "Graphics" / "Glow Red.asset.cpp") << "x";
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
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    AssetsPanel panel;
    panel.SetRoots(engine, assets);
    ImGuiID search = 0;
    ImVec2 clearPosition;
    const auto frame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Assets");
        ImGui::LogToBuffer();
        panel.Draw(snapshot, {});
        const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
        ImGui::LogFinish();
        auto* window = ImGui::GetCurrentWindow();
        for (auto* child : window->DC.ChildWindows)
            if (child->ChildId == window->GetID("asset-toolbar")) {
                search = child->GetID("##asset-search");
            }
        const auto& gui = *ImGui::GetCurrentContext();
        if (gui.NavWindow && gui.NavId == search) {
            auto rect = gui.NavWindow->NavRectRel[ImGuiNavLayer_Main];
            rect.Translate(gui.NavWindow->Pos);
            clearPosition = {rect.Max.x + ImGui::GetStyle().ItemSpacing.x + rect.GetHeight() * 0.5f,
                             rect.GetCenter().y};
        }
        ImGui::End();
        ImGui::Render();
        return text;
    };
    CHECK(frame().find("1 item") != std::string::npos);
    REQUIRE(search != 0);
    ImGui::ActivateItemByID(search);
    (void)frame();
    io.AddInputCharactersUTF8("red glow");
    const auto results = frame();
    CHECK(results.find("Searching all asset folders") != std::string::npos);
    CHECK(results.find("2 items") != std::string::npos);
    (void)frame(); // The clear button reserves its space on the frame after the first character.
    io.AddMousePosEvent(clearPosition.x, clearPosition.y);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    (void)frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    (void)frame();
    const auto folder = frame();
    CHECK(folder.find("Searching all asset folders") == std::string::npos);
    CHECK(folder.find("1 item") != std::string::npos);
}
