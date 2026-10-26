#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include <imgui.h>
#include <algorithm>
#include <ctime>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
const char* SourceLabel(EditorLogSource source) {
    switch (source) {
    case EditorLogSource::Editor: return "Editor";
    case EditorLogSource::Worker: return "Worker";
    case EditorLogSource::Build : return "Build";
    }
    return "";
}
const char* LevelLabel(EditorLogLevel level) {
    switch (level) {
    case EditorLogLevel::Info   : return "Info";
    case EditorLogLevel::Warning: return "Warning";
    case EditorLogLevel::Error  : return "Error";
    }
    return "";
}
void FilterCheckbox(const char* label, bool& value) {
    ContinueEditorToolbar(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
                          ImGui::CalcTextSize(label).x);
    ImGui::Checkbox(label, &value);
}
std::string DisplayLine(const EditorLogEntry& entry) {
    const auto time = std::chrono::system_clock::to_time_t(entry.time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::array<char, 16> stamp{};
    std::strftime(stamp.data(), stamp.size(), "%H:%M:%S", &local);
    return std::string(stamp.data()) + " [" + SourceLabel(entry.source) + "/" + LevelLabel(entry.level) + "] " +
           entry.text;
}
} // namespace
void OutputPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences) {
    auto& filter = preferences.outputFilter;
    if (!_initialized) {
        std::ranges::copy(filter.search, _search.begin());
        _initialized = true;
    }
    ImGui::Checkbox("Info", &filter.info);
    FilterCheckbox("Warning", filter.warning);
    FilterCheckbox("Error", filter.error);
    FilterCheckbox("Follow", preferences.followOutput);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search output...", _search.data(), _search.size()))
        filter.search = _search.data();
    std::vector<const EditorLogEntry*> visible;
    const auto& entries = snapshot.diagnostics.Entries();
    std::erase_if(_selected, [&](auto id) {
        return std::ranges::none_of(entries, [id](const auto& e) { return e.sequence == id; });
    });
    for (const auto& entry : entries) {
        if (!filter.Matches(entry)) continue;
        visible.push_back(&entry);
    }
    const bool copyFiltered = ImGui::SmallButton("Copy filtered");
    ContinueEditorToolbar("Copy selected");
    const bool hasSelection =
        std::ranges::any_of(visible, [&](const auto* entry) { return _selected.contains(entry->sequence); });
    ImGui::BeginDisabled(!hasSelection);
    const bool copySelected = ImGui::SmallButton("Copy selected");
    ImGui::EndDisabled();
    if (copyFiltered || copySelected) {
        std::string text;
        for (const auto* entry : visible) {
            if (copyFiltered || _selected.contains(entry->sequence)) text += DisplayLine(*entry) + "\n";
        }
        ImGui::SetClipboardText(text.c_str());
    }
    ContinueEditorToolbar("Clear");
    if (ImGui::SmallButton("Clear")) {
        actions.clearOutput();
        _selected.clear();
    }
    ImGui::Separator();
    if (ImGui::BeginChild("lines", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        const bool follow = preferences.followOutput && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& entry = *visible[static_cast<std::size_t>(i)];
                const auto id = std::to_string(entry.sequence);
                ImGui::PushID(id.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, EditorLevelColor(entry.level));
                const auto line = DisplayLine(entry);
                const auto position = ImGui::GetCursorScreenPos();
                const float width =
                    std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(line.c_str(), nullptr, false).x);
                if (ImGui::Selectable("##entry", _selected.contains(entry.sequence), ImGuiSelectableFlags_None,
                                      {width, ImGui::GetTextLineHeight()})) {
                    const bool hadSelection = _selected.contains(entry.sequence);
                    if (!ImGui::GetIO().KeyCtrl) _selected.clear();
                    if (hadSelection)
                        _selected.erase(entry.sequence);
                    else
                        _selected.insert(entry.sequence);
                }
                ImGui::GetWindowDrawList()->AddText(position, ImGui::GetColorU32(ImGuiCol_Text), line.c_str());
                EditorTooltip(entry.text.c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
        if (follow) ImGui::SetScrollHereY(1);
    }
    ImGui::EndChild();
    actions.acknowledgeOutput();
}
