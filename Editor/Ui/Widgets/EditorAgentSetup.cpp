#include "Editor/Ui/Widgets/EditorAgentSetup.h"
#include <imgui.h>
#include <algorithm>
#include "Platform/Shell.h"
#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr const char* SetupPopup = "Set up agent###editor.agent-setup";
constexpr const char* KnowledgeHint =
    "Let the agent know the engine: add @Engine/AgentDocs/Agent.md to your CLAUDE.md (or AGENTS.md)";
int ResizeCommand(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto& text = *static_cast<std::string*>(data->UserData);
        text.resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = text.data();
    }
    return 0;
}
} // namespace

void EditorAgentChoice(AgentPreferences& preferences, float commandWidth) {
    for (const auto& kind : AgentKinds) {
        if (ImGui::RadioButton(kind.name, preferences.kind == kind.id)) preferences.kind = kind.id;
        if (*kind.command) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", kind.command);
        }
    }
    if (preferences.kind == AgentKind::Custom) {
        ImGui::SetNextItemWidth(commandWidth);
        (void)ImGui::InputTextWithHint("##agent-command", "Executable or full path, followed by arguments",
                                       preferences.customCommand.data(), preferences.customCommand.capacity() + 1,
                                       ImGuiInputTextFlags_CallbackResize, ResizeCommand, &preferences.customCommand);
        ToolTooltip("Command line run in the project folder. Quote paths containing spaces.");
    }
}
void EditorAgentSetup::Open(const AgentPreferences& preferences) {
    if (_active) return;
    _draft = preferences;
    _checkedCommand.clear();
    _available = false;
    _active = _opening = true;
}
EditorAgentSetupResult EditorAgentSetup::Draw(AgentPreferences& preferences) {
    if (!_active) return EditorAgentSetupResult::None;
    if (_opening) {
        ImGui::OpenPopup(SetupPopup);
        _opening = false;
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float scale = ToolUiScale();
    const float width = std::max(1.0f, std::min(540 * scale, viewport->Size.x - 32 * scale));
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(SetupPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return EditorAgentSetupResult::None;
    ImGui::PushTextWrapPos(0);
    ImGui::TextWrapped("Run your agent CLI in the Console, using its existing account and permissions.");
    ImGui::SeparatorText("Agent CLI");
    EditorAgentChoice(_draft);
    const auto command = AgentCommand(_draft);
    const auto program = AgentProgram(command);
    if (command != _checkedCommand || ImGui::IsWindowAppearing()) {
        _checkedCommand = command;
        _available = CommandOnPath(program);
    }
    ImGui::Spacing();
    const bool fullPath = program.find_first_of("\\/") != std::string::npos;
    ToolStatusBadge(_available ? (fullPath ? "Executable found" : "Found on PATH")
                               : (command.empty() ? "No command set" : "Command not found"),
                    ToolToneColor(_available ? ToolTone::Success : ToolTone::Warning));
    ToolTooltip(_available ? "The command is on PATH or names an existing executable."
                           : "The command must be on PATH, or use the full path to its executable.");
    ImGui::SameLine();
    if (ImGui::TextLink("Check again")) _available = CommandOnPath(program);
    const auto& kind = AgentKindInfo(_draft.kind);
    if (*kind.install) {
        ImGui::SameLine();
        if (ImGui::TextLink("Install...")) OpenExternalUrl(kind.install);
        ToolTooltip(kind.install);
    }
    if (!_available)
        ImGui::TextWrapped(
            "Install the CLI, then check again. If PATH changed, restart the editor or use a full path.");
    ImGui::SeparatorText("Engine knowledge");
    ImGui::TextWrapped("%s", KnowledgeHint);
    if (ToolButton("Copy hint")) CopyToClipboard(KnowledgeHint);
    ToolTooltip("Copy this instruction to the clipboard.");
    ImGui::Separator();
    EditorAgentSetupResult result = EditorAgentSetupResult::None;
    ImGui::BeginDisabled(!_available);
    if (ToolButton("Start", {100 * scale, 0}, true)) {
        // Control can be changed outside the draft; setup only accepts the CLI choice.
        preferences.kind = _draft.kind;
        preferences.customCommand = _draft.customCommand;
        preferences.configured = true;
        result = EditorAgentSetupResult::Started;
    }
    if (_available) ImGui::SetItemDefaultFocus();
    ImGui::EndDisabled();
    ToolTooltip(_available ? "Open the agent session." : "Choose an available command before starting.");
    ImGui::SameLine();
    if (ToolButton("Cancel", {100 * scale, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        result = EditorAgentSetupResult::Canceled;
    if (!_available) ImGui::SetItemDefaultFocus();
    if (result != EditorAgentSetupResult::None) {
        _active = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndPopup();
    return result;
}
