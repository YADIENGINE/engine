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
const char* ResultLabel(EditorBuildResult result) {
    switch (result) {
    case EditorBuildResult::None         : return "No build attempt yet";
    case EditorBuildResult::Running      : return "Running";
    case EditorBuildResult::Succeeded    : return "Succeeded";
    case EditorBuildResult::BuildFailed  : return "Build failed";
    case EditorBuildResult::RefreshFailed: return "Build succeeded, refresh failed";
    case EditorBuildResult::Canceled     : return "Canceled";
    }
    return "";
}
std::string TimeLabel(std::chrono::system_clock::time_point value) {
    const auto time = std::chrono::system_clock::to_time_t(value);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::array<char, 16> stamp{};
    std::strftime(stamp.data(), stamp.size(), "%H:%M:%S", &local);
    return stamp.data();
}
void SeverityFilter(EditorLogLevel level, std::size_t count, bool& value, bool sameLine) {
    const std::string text = std::string(LevelLabel(level)) + " " + std::to_string(count);
    const std::string label = text + "###severity";
    if (sameLine) ContinueEditorToolbar(text.c_str());
    ImGui::PushID(static_cast<int>(level));
    if (EditorSegment(label.c_str(), value)) value = !value;
    EditorTooltip(value ? "Hide this severity" : "Show this severity");
    ImGui::PopID();
}
std::string DisplayLine(const EditorLogEntry& entry) {
    return TimeLabel(entry.time) + " [" + SourceLabel(entry.source) + "/" + LevelLabel(entry.level) + "] " + entry.text;
}
} // namespace

void OutputPanel::ShowLastAttempt(std::uint64_t attempt) {
    _attemptSeen = attempt;
    _lastAttempt = true;
    _failureView = true;
    _failureFilter = {};
    _search.fill(0);
    _selected.clear();
    _anchor = 0;
    _jumpToError = true;
    _initialized = true;
}
void OutputPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences) {
    const auto& diagnostics = snapshot.diagnostics;
    const auto& attempt = diagnostics.Attempt();
    if (_attemptSeen != attempt.id) {
        _attemptSeen = attempt.id;
        _selected.clear();
        _anchor = 0;
        _failureView = false;
        _initialized = false;
    }
    const bool last = EditorSegment("Last Build Try", _lastAttempt);
    ContinueEditorToolbar("Full");
    const bool full = EditorSegment("Full", !_lastAttempt);
    if (last || full) {
        _lastAttempt = last;
        _failureView = false;
        _initialized = false;
        _selected.clear();
        _anchor = 0;
        _jumpToError = false;
    }
    ContinueEditorToolbar("Open Log");
    const auto& archive = diagnostics.ArchivePath(_lastAttempt);
    std::error_code fileError;
    ImGui::BeginDisabled(archive.empty() || !std::filesystem::is_regular_file(archive, fileError));
    if (ImGui::Button("Open Log")) actions.openOutputLog(_lastAttempt);
    ImGui::EndDisabled();
    EditorTooltip("Open the complete log file for this scope. Clear only removes the Full view's buffered lines.");
    if (_lastAttempt) {
        if (attempt.id) {
            ImGui::TextWrapped("Attempt #%llu | Started %s | %.1f s | %s", static_cast<unsigned long long>(attempt.id),
                               TimeLabel(attempt.started).c_str(), diagnostics.AttemptSeconds(),
                               ResultLabel(attempt.result));
            if (attempt.exitCode) ImGui::Text("Build exit code: %lu", *attempt.exitCode);
        } else {
            ImGui::TextDisabled("Use Build & Refresh to start a build attempt.");
        }
        if (attempt.truncated)
            ImGui::TextWrapped(
                "Older lines were removed from this view. The first error is pinned; use Open Log for all output.");
    } else {
        ImGui::TextWrapped(
            "Recent editor, worker, and build output (up to 1000 lines). Open Log contains the full session.");
    }
    if (!diagnostics.ArchiveError().empty()) ImGui::TextWrapped("%s", diagnostics.ArchiveError().c_str());
    auto& filter = _failureView ? _failureFilter : preferences.outputFilter;
    if (!_initialized) {
        _search.fill(0);
        std::copy_n(filter.search.begin(), std::min(filter.search.size(), _search.size() - 1), _search.begin());
        _initialized = true;
    }
    const auto& entries = _lastAttempt ? diagnostics.AttemptEntries() : diagnostics.Entries();
    std::vector<const EditorLogEntry*> scoped;
    if (_lastAttempt && attempt.firstError &&
        (entries.empty() || attempt.firstError->sequence < entries.front().sequence))
        scoped.push_back(&*attempt.firstError);
    for (const auto& entry : entries)
        scoped.push_back(&entry);
    std::array<std::size_t, 3> counts{};
    for (const auto* entry : scoped)
        ++counts[static_cast<std::size_t>(entry->level)];
    SeverityFilter(EditorLogLevel::Info, counts[0], filter.info, false);
    SeverityFilter(EditorLogLevel::Warning, counts[1], filter.warning, true);
    SeverityFilter(EditorLogLevel::Error, counts[2], filter.error, true);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search output...", _search.data(), _search.size()))
        filter.search = _search.data();
    std::vector<const EditorLogEntry*> visible;
    for (const auto* entry : scoped)
        if (filter.Matches(*entry)) visible.push_back(entry);
    std::erase_if(_selected, [&](auto id) {
        return !std::ranges::binary_search(visible, id, {}, [](const auto* e) { return e->sequence; });
    });
    bool copyVisible = false, copySelected = false;
    if (ImGui::Button("Copy")) ImGui::OpenPopup("copy-output");
    if (ImGui::BeginPopup("copy-output")) {
        copySelected = ImGui::MenuItem("Copy Selected", "Ctrl+C", false, !_selected.empty());
        copyVisible = ImGui::MenuItem("Copy Visible", nullptr, false, !visible.empty());
        ImGui::EndPopup();
    }
    ContinueEditorToolbar(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
                          ImGui::CalcTextSize("Follow").x);
    ImGui::Checkbox("Follow", &preferences.followOutput);
    EditorTooltip("Keep new output in view while at the bottom and no lines are selected.");
    ContinueEditorToolbar(ImGui::GetFrameHeight());
    ImGui::BeginDisabled(_lastAttempt || entries.empty());
    if (EditorIconButton("clear-output", EditorGlyph::Clear, "Clear Full view buffer (log files are preserved)")) {
        actions.clearOutput();
        _selected.clear();
        visible.clear();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Click to select | Ctrl: add/remove | Shift: range | Double-click: text details");
    ImGui::Separator();
    ImGui::PushFont(EditorFont(EditorFontRole::Technical), 0);
    bool openDetail = false;
    if (ImGui::BeginChild("lines", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        const bool follow = preferences.followOutput && _selected.empty() && !_jumpToError &&
                            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        if (visible.empty())
            EditorEmptyState(scoped.empty() ? "No output yet" : "No matching output",
                             scoped.empty() ? "Editor, preview, and build messages appear here."
                                            : "Adjust the severity filters or search text.");
        int jump = -1;
        if (_jumpToError && !visible.empty()) {
            const auto error =
                std::ranges::find_if(visible, [](const auto* e) { return e->level == EditorLogLevel::Error; });
            jump = error == visible.end() ? 0 : static_cast<int>(error - visible.begin());
        }
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        if (jump >= 0) clipper.IncludeItemByIndex(jump);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& entry = *visible[static_cast<std::size_t>(i)];
                const auto id = std::to_string(entry.sequence);
                ImGui::PushID(id.c_str());
                const auto line = DisplayLine(entry);
                const auto position = ImGui::GetCursorScreenPos();
                const float width =
                    std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(line.c_str(), nullptr, false).x);
                if (ImGui::Selectable("##entry", _selected.contains(entry.sequence),
                                      ImGuiSelectableFlags_AllowDoubleClick, {width, ImGui::GetTextLineHeight()})) {
                    const auto& io = ImGui::GetIO();
                    if (io.KeyShift && _anchor) {
                        const auto anchor =
                            std::ranges::find_if(visible, [&](const auto* e) { return e->sequence == _anchor; });
                        if (!io.KeyCtrl) _selected.clear();
                        if (anchor != visible.end()) {
                            const int from = static_cast<int>(anchor - visible.begin());
                            for (int j = std::min(from, i); j <= std::max(from, i); ++j)
                                _selected.insert(visible[static_cast<std::size_t>(j)]->sequence);
                        } else {
                            _selected.insert(entry.sequence);
                            _anchor = entry.sequence;
                        }
                    } else {
                        const bool selected = _selected.contains(entry.sequence);
                        if (!io.KeyCtrl) _selected.clear();
                        if (io.KeyCtrl && selected)
                            _selected.erase(entry.sequence);
                        else
                            _selected.insert(entry.sequence);
                        _anchor = entry.sequence;
                    }
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        _detail.assign(entry.text.begin(), entry.text.end());
                        _detail.push_back(0);
                        openDetail = true;
                    }
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !_selected.contains(entry.sequence)) {
                    _selected.clear();
                    _selected.insert(entry.sequence);
                    _anchor = entry.sequence;
                }
                if (i == jump) ImGui::SetScrollHereY(0.25f);
                const char* severity = line.c_str() + line.find('[');
                const char* message = line.c_str() + line.size() - entry.text.size();
                const float severityOffset = ImGui::CalcTextSize(line.c_str(), severity).x;
                const float messageOffset = ImGui::CalcTextSize(line.c_str(), message).x;
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddText(position, ImGui::GetColorU32(ImGuiCol_TextDisabled), line.c_str(), severity);
                draw->AddText({position.x + severityOffset, position.y},
                              ImGui::GetColorU32(EditorLevelColor(entry.level)), severity, message);
                draw->AddText({position.x + messageOffset, position.y}, ImGui::GetColorU32(ImGuiCol_Text), message);
                EditorTooltip(entry.text.c_str());
                ImGui::PopID();
            }
        }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && !ImGui::GetIO().WantTextInput &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A)) {
                for (const auto* entry : visible)
                    _selected.insert(entry->sequence);
                if (!visible.empty()) _anchor = visible.front()->sequence;
            }
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C)) copySelected = !_selected.empty();
        }
        if (ImGui::BeginPopupContextWindow("output-context", ImGuiPopupFlags_MouseButtonRight)) {
            copySelected |= ImGui::MenuItem("Copy Selected", "Ctrl+C", false, !_selected.empty());
            copyVisible |= ImGui::MenuItem("Copy Visible", nullptr, false, !visible.empty());
            ImGui::EndPopup();
        }
        if (follow && _selected.empty() && !ImGui::IsAnyItemActive()) ImGui::SetScrollHereY(1);
        _jumpToError = false;
    }
    ImGui::EndChild();
    if (copyVisible || copySelected) {
        std::string text;
        for (const auto* entry : visible)
            if (copyVisible || _selected.contains(entry->sequence)) text += DisplayLine(*entry) + "\n";
        ImGui::SetClipboardText(text.c_str());
    }
    if (openDetail) ImGui::OpenPopup("Output line details");
    ImGui::SetNextWindowSize({ImGui::GetFontSize() * 48, ImGui::GetFontSize() * 18}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Output line details", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Select text with the mouse, then Ctrl+C. Ctrl+A selects this message.");
        ImGui::InputTextMultiline("##detail", _detail.data(), _detail.size(), {-1, -ImGui::GetFrameHeightWithSpacing()},
                                  ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopFont();
    actions.acknowledgeOutput();
}
