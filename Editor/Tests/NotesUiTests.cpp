#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "ToolUi/ToolTheme.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Editor.Notes.WorkspaceOwnsTypingAndFlushesOnSwitch", "[engine-editor][notes]") {
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
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto first = files.Create(files.Root(), "Plan", false);
    const auto second = files.Create(files.Root(), "Handoff", false);
    files.Write(first, "Original\r\n");
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    snapshot.capabilities.enabled[static_cast<std::size_t>(EditorCommand::Open)] = true;
    EditorPreferences preferences;
    CHECK_FALSE(preferences.Visible(EditorPanel::Notes));
    EditorWorkspace workspace;
    workspace.SetNotesProject(temp.Path());
    int fontSize = 100;
    int commands = 0;
    EditorUiActions actions;
    actions.command = [&](EditorCommand) { ++commands; };
    const auto frame = [&] {
        ImGui::NewFrame();
        const auto result = workspace.Draw(snapshot, actions, {}, preferences, fontSize);
        ImGui::Render();
        return result;
    };
    (void)frame();
    workspace.OpenNote(first, preferences);
    (void)frame();
    (void)frame();
    (void)frame();
    auto* notes = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Notes).window);
    auto* viewport = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Viewport).window);
    REQUIRE(notes);
    REQUIRE(viewport);
    CHECK(notes->DockId == viewport->DockId);
    REQUIRE_FALSE(notes->DC.ChildWindows.empty());
    auto* text = notes->DC.ChildWindows.back();
    const auto clickText = [&] {
        io.AddMousePosEvent(text->Pos.x + 40, text->Pos.y + 15);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        (void)frame();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        (void)frame();
        (void)frame();
    };
    clickText();
    REQUIRE(io.WantTextInput);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    (void)frame();
    io.AddInputCharactersUTF8("New plan");
    CHECK(frame().cancelViewportInput);
    CHECK(files.Read(first) == "Original\r\n"); // Still inside the debounce interval.
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_S, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_S, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    (void)frame();
    CHECK(files.Read(first) == "New plan");
    io.AddKeyEvent(ImGuiKey_F5, true);
    CHECK(frame().cancelViewportInput);
    io.AddKeyEvent(ImGuiKey_F5, false);
    (void)frame();
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_P, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_P, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    (void)frame();
    CHECK_FALSE(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    CHECK(commands == 0);
    io.AddInputCharactersUTF8(" for next session");
    (void)frame();
    workspace.OpenNote(second, preferences);
    CHECK(files.Read(first) == "New plan for next session");
    CHECK(workspace.FlushNotes());
    workspace.OpenNote(first, preferences);
    (void)frame();
    (void)frame();
    // A focused but clean input must pick up external bytes in its internal ImGui buffer too.
    files.Write(first, "Agent report");
    io.DeltaTime = 1.1f;
    (void)frame();
    (void)frame();
    io.DeltaTime = 1.0f / 60.0f;
    auto* input = ImGui::GetInputTextState(notes->GetID("##note-text"));
    REQUIRE(input);
    CHECK(std::string(input->GetText()) == "Agent report");
    io.AddInputCharactersUTF8(" updated");
    (void)frame();
    io.AddFocusEvent(false);
    (void)frame();
    CHECK(files.Read(first)->find("updated") != std::string::npos); // App deactivation flushes too.
    io.AddFocusEvent(true);
    (void)frame();
    ImGui::FocusWindow(viewport);
    (void)frame();
    CHECK(files.Read(first)->find("updated") != std::string::npos); // Focus loss flushes without debounce.
    CHECK(workspace.FlushNotes());
    CHECK(workspace.Sessions().Infos().empty()); // Typing never started or reached a terminal.

    // Shortcuts that work everywhere still work, and closing over a conflict keeps the unsaved text.
    workspace.OpenNote(first, preferences);
    (void)frame();
    (void)frame();
    (void)frame();
    clickText();
    REQUIRE(io.WantTextInput);
    io.AddInputCharactersUTF8(" unsaved");
    (void)frame();
    files.Write(first, "Agent again");
    io.DeltaTime = 1.1f;
    (void)frame();
    io.DeltaTime = 1.0f / 60.0f;
    (void)frame();
    const auto visibleBefore = preferences.visible;
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_J, true);
    (void)frame();
    io.AddKeyEvent(ImGuiKey_J, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    (void)frame();
    CHECK(preferences.visible != visibleBefore);
    workspace.CloseNotes();
    CHECK(files.Read(first) == "Agent again");
    const auto recovered = files.Root() / "Plan (recovered).md";
    REQUIRE(std::filesystem::exists(recovered));
    CHECK(files.Read(recovered)->find("unsaved") != std::string::npos);
}
TEST_CASE("Editor.Notes.PanelFollowsRenamesAndHoldsDeletedNotes", "[engine-editor][notes]") {
    TempDirectory temp;
    NoteFiles files(temp.Path());
    const auto folder = files.Create(files.Root(), "Reports", true);
    const auto note = files.Create(folder, "Plan", false);
    const auto other = files.Create(files.Root(), "Other", false);
    NotesPanel panel;
    panel.SetProject(temp.Path());
    REQUIRE(panel.Open(note));
    CHECK(panel.AgentPath() == "Notes/Reports/Plan.md");
    const auto renamedFolder = files.Rename(folder, "Daily");
    panel.Renamed(folder, renamedFolder);
    CHECK(panel.AgentPath() == "Notes/Daily/Plan.md");
    panel.Tick(2, false);
    CHECK(panel.Flush()); // Still the same file, not a deleted one.
    std::filesystem::remove(renamedFolder / "Plan.md");
    panel.Tick(4, false);
    CHECK_FALSE(panel.Flush());
    CHECK_FALSE(panel.Open(other)); // Text stays until Recreate or Discard, even when it had no edits.
    CHECK(panel.AgentPath() == "Notes/Daily/Plan.md");
}
