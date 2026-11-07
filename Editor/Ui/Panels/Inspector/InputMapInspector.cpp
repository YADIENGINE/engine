#include "Editor/Ui/Panels/Inspector/InputMapInspector.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
// ImGui's keys and the input map's names for them (Input/Input.h InputKey).
constexpr std::array<std::pair<ImGuiKey, const char*>, 37> NamedKeys{{
    {ImGuiKey_UpArrow, "Up"},
    {ImGuiKey_DownArrow, "Down"},
    {ImGuiKey_LeftArrow, "Left"},
    {ImGuiKey_RightArrow, "Right"},
    {ImGuiKey_Enter, "Enter"},
    {ImGuiKey_KeypadEnter, "Enter"},
    {ImGuiKey_Space, "Space"},
    {ImGuiKey_LeftShift, "LeftShift"},
    {ImGuiKey_RightShift, "RightShift"},
    {ImGuiKey_LeftCtrl, "LeftControl"},
    {ImGuiKey_RightCtrl, "RightControl"},
    {ImGuiKey_LeftAlt, "LeftAlt"},
    {ImGuiKey_RightAlt, "RightAlt"},
    {ImGuiKey_Tab, "Tab"},
    {ImGuiKey_Backspace, "Backspace"},
    {ImGuiKey_Delete, "Delete"},
    {ImGuiKey_Insert, "Insert"},
    {ImGuiKey_Home, "Home"},
    {ImGuiKey_End, "End"},
    {ImGuiKey_PageUp, "PageUp"},
    {ImGuiKey_PageDown, "PageDown"},
    {ImGuiKey_Minus, "Minus"},
    {ImGuiKey_Equal, "Equal"},
    {ImGuiKey_KeypadSubtract, "Subtract"},
    {ImGuiKey_KeypadAdd, "Add"},
    {ImGuiKey_F1, "F1"},
    {ImGuiKey_F2, "F2"},
    {ImGuiKey_F3, "F3"},
    {ImGuiKey_F4, "F4"},
    {ImGuiKey_F5, "F5"},
    {ImGuiKey_F6, "F6"},
    {ImGuiKey_F7, "F7"},
    {ImGuiKey_F8, "F8"},
    {ImGuiKey_F9, "F9"},
    {ImGuiKey_F10, "F10"},
    {ImGuiKey_F11, "F11"},
    {ImGuiKey_F12, "F12"},
}};
constexpr std::array<std::pair<ImGuiMouseButton, const char*>, 3> MouseNames{{
    {ImGuiMouseButton_Left, "MouseLeft"},
    {ImGuiMouseButton_Right, "MouseRight"},
    {ImGuiMouseButton_Middle, "MouseMiddle"},
}};
constexpr std::array<const char*, 10> DigitNames{"Zero", "One", "Two",   "Three", "Four",
                                                 "Five", "Six", "Seven", "Eight", "Nine"};

// The input map's name for the key pressed this frame; empty when none (or one it cannot bind).
std::string PressedKey() {
    for (int key = ImGuiKey_A; key <= ImGuiKey_Z; ++key)
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
            return std::string(1, static_cast<char>('A' + (key - ImGuiKey_A)));
    for (int key = ImGuiKey_0; key <= ImGuiKey_9; ++key)
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false)) return DigitNames[key - ImGuiKey_0];
    for (const auto& [key, name] : NamedKeys)
        if (ImGui::IsKeyPressed(key, false)) return name;
    return {};
}

const std::vector<std::string>* Keys(const InputMapDocument& map, std::size_t player, std::size_t button) {
    if (player >= map.players.size() || button >= map.players[player].size()) return nullptr;
    const auto& keys = map.players[player][button];
    return keys ? &*keys : nullptr;
}

// Differs from the committed map; unknown without a commit.
bool Modified(const TuningFileState& file, std::size_t player, std::size_t button) {
    if (!file.committedInput || !file.input) return false;
    const auto& committed = *file.committedInput;
    const auto found = std::ranges::find(committed.buttons, file.input->buttons[button]);
    if (found == committed.buttons.end()) return true;
    const auto* was = Keys(committed, player, static_cast<std::size_t>(found - committed.buttons.begin()));
    const auto* now = Keys(*file.input, player, button);
    const std::vector<std::string> none;
    return (was ? *was : none) != (now ? *now : none);
}

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}
} // namespace

void InputMapInspector::File(const TuningFileState& file, const EditorSnapshot& snapshot,
                             const EditorUiActions& actions) {
    ImGui::PushID(Utf8(file.source).c_str());
    Status(file, snapshot, actions);
    if (file.input) {
        const auto& map = *file.input;
        Listen(file, actions);
        constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                               ImGuiTableFlags_NoSavedSettings;
        const int columns = static_cast<int>(map.players.size()) + 1;
        ImGui::BeginDisabled(file.problem.has_value());
        if (ImGui::BeginTable("##input-map", columns, flags)) {
            ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed);
            for (std::size_t p = 0; p < map.players.size(); ++p)
                ImGui::TableSetupColumn(("Player " + std::to_string(p + 1)).c_str());
            ImGui::TableHeadersRow();
            for (std::size_t b = 0; b < map.buttons.size(); ++b) {
                ImGui::PushID(static_cast<int>(b));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(map.buttons[b].c_str());
                for (std::size_t p = 0; p < map.players.size(); ++p) {
                    ImGui::TableSetColumnIndex(static_cast<int>(p) + 1);
                    ImGui::PushID(static_cast<int>(p));
                    Cell(file, p, b, actions);
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        if (!map.axes.empty()) {
            std::string axes;
            for (const auto& axis : map.axes)
                axes += (axes.empty() ? "" : ", ") + axis;
            ImGui::TextDisabled("Axes: %s (the game sets their values)", axes.c_str());
        }
    }
    ImGui::PopID();
}

void InputMapInspector::Status(const TuningFileState& file, const EditorSnapshot& snapshot,
                               const EditorUiActions& actions) {
    if (file.problem) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Error));
        ImGui::TextWrapped("Line %d: %s", file.problem->line, file.problem->text.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled(file.input ? "Showing the last valid map; fix the file to edit it here."
                                       : "Fix the file to edit it here.");
        return;
    }
    if (file.target.empty()) {
        ToolStatusBadge("Not built", ToolToneColor(ToolTone::Warning));
        ToolTooltip("No build has made its header yet, so no code can bind it.");
        if (snapshot.capabilities.Can(EditorCommand::Refresh)) {
            ImGui::SameLine();
            if (ToolButton("Build & Refresh")) actions.command(EditorCommand::Refresh);
        }
    }
    ImGui::TextWrapped(
        "Keys apply the next time a scene binds the map (Reset). Buttons and axes are edited in the file "
        "and need a build.");
}

void InputMapInspector::Cell(const TuningFileState& file, std::size_t player, std::size_t button,
                             const EditorUiActions& actions) {
    const auto& map = *file.input;
    const auto* keys = Keys(map, player, button);
    const std::vector<std::string> current = keys ? *keys : std::vector<std::string>{};
    const bool modified = Modified(file, player, button);
    if (modified) ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Warning));
    for (std::size_t i = 0; i < current.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ToolButton(current[i].c_str())) {
            auto rest = current;
            rest.erase(rest.begin() + static_cast<std::ptrdiff_t>(i));
            actions.tuning({.kind = TuningEditKind::SetKeys,
                            .source = file.source,
                            .player = player,
                            .button = map.buttons[button],
                            .keys = std::move(rest)});
        }
        ToolTooltip(modified ? "Click to remove this key.\nModified since the last commit."
                             : "Click to remove this key.");
        ImGui::PopID();
        ImGui::SameLine();
    }
    if (modified) ImGui::PopStyleColor();
    const bool listening = _capture && _capture->source == file.source && _capture->player == player &&
                           _capture->button == map.buttons[button];
    if (listening) {
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Accent));
        ImGui::TextUnformatted("Press a key or click here...");
        ImGui::PopStyleColor();
        // A mouse button binds by clicking the prompt, so a click elsewhere does not bind it by accident.
        if (ImGui::IsItemHovered())
            for (const auto& [mouse, name] : MouseNames)
                if (ImGui::IsMouseClicked(mouse)) _capture->clicked = name;
        ToolTooltip("Click with a mouse button to bind it. Escape cancels.");
        return;
    }
    if (ToolIconButton("add", "+",
                       "Add a key: press it next, or click the prompt with a mouse button. Escape cancels."))
        _capture = Capture{file.source, player, map.buttons[button]};
}

void InputMapInspector::Listen(const TuningFileState& file, const EditorUiActions& actions) {
    if (!_capture || _capture->source != file.source) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        _capture.reset();
        return;
    }
    auto name = PressedKey();
    if (name.empty()) name = _capture->clicked;
    if (name.empty()) return;
    const auto& map = *file.input;
    const auto button = std::ranges::find(map.buttons, _capture->button);
    if (button != map.buttons.end()) {
        const auto* keys = Keys(map, _capture->player, static_cast<std::size_t>(button - map.buttons.begin()));
        auto next = keys ? *keys : std::vector<std::string>{};
        if (std::ranges::find(next, name) == next.end()) {
            next.push_back(name);
            actions.tuning({.kind = TuningEditKind::SetKeys,
                            .source = file.source,
                            .player = _capture->player,
                            .button = _capture->button,
                            .keys = std::move(next)});
        }
    }
    _capture.reset();
}
