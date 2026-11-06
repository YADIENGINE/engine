#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <format>
#include "Editor/Internals/Session/AgentContext.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float SearchMinimumWidth = 140.0f;
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
void SeverityFilter(EditorLogLevel level, std::size_t count, bool& value, bool separateGroup) {
    const std::string text = std::string(LevelLabel(level)) + " " + std::to_string(count);
    const std::string label = text + "###severity";
    ContinueToolToolbar(text.c_str(), separateGroup);
    ImGui::PushID(static_cast<int>(level));
    if (ToolSegment(label.c_str(), value)) value = !value;
    ToolTooltip(value ? "Hide this severity" : "Show this severity");
    ImGui::PopID();
}
std::string DisplayLine(const EditorLogEntry& entry) {
    return TimeLabel(entry.time) + " [" + SourceLabel(entry.source) + "/" + LevelLabel(entry.level) + "] " + entry.text;
}
} // namespace

void OutputPanel::ShowLastAttempt(std::uint64_t attempt) {
    _attemptSeen = attempt;
    _problems = false;
    _lastAttempt = true;
    _failureView = true;
    _failureFilter = {};
    _search.fill(0);
    _selected.clear();
    _anchor = 0;
    _jumpToError = true;
    _initialized = true;
}
// The latest attempt's diagnostics, one click from the code.
void OutputPanel::DrawProblems(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                               const std::string& agentTarget) {
    const auto& diagnostics = snapshot.diagnostics;
    const auto& attempt = diagnostics.Attempt();
    const auto& problems = diagnostics.Problems();
    const auto errors = diagnostics.ProblemErrors();
    const auto warnings = problems.size() - errors;
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ToolColors().muted, "%s",
                       std::format("{} error{}, {} warning{}  \xC2\xB7  {}  \xC2\xB7  {:.1f} s", errors,
                                   errors == 1 ? "" : "s", warnings, warnings == 1 ? "" : "s",
                                   ResultLabel(attempt.result), diagnostics.AttemptSeconds())
                           .c_str());
    if (errors > 0) {
        ContinueToolToolbar("Ask agent to fix build", true);
        PushEditorTintedButton(EditorAgentColor(), EditorAgentColor());
        if (ToolButton("Ask agent to fix build")) _fixAll = true;
        PopEditorButton();
        ToolTooltip("Send the build errors to the agent session and ask it to fix them. This presses Enter.");
        ImGui::TextWrapped("Context to: %s", agentTarget.c_str());
    }
    ImGui::Separator();
    if (!ImGui::BeginChild("problems")) {
        ImGui::EndChild();
        return;
    }
    if (problems.empty())
        ToolEmptyState(attempt.id ? "No problems found" : "No build yet",
                       attempt.result == EditorBuildResult::BuildFailed ||
                               attempt.result == EditorBuildResult::RefreshFailed
                           ? "This failure has no compiler messages. See Last build for the whole output."
                           : "Errors and warnings from the latest build appear here.");
    const float scale = ToolUiScale();
    const float rowHeight = ImGui::GetTextLineHeight() * 2 + ToolMetrics::Space * scale * 2;
    const float fixWidth = ImGui::CalcTextSize("Paste error").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(problems.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
    while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto& problem = problems[static_cast<std::size_t>(index)];
            ImGui::PushID(index);
            const auto start = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##problem", false, ImGuiSelectableFlags_AllowOverlap, {0, rowHeight}) &&
                !problem.file.empty())
                actions.openSource(problem.file, problem.line);
            const auto where = problem.line > 0 ? std::format("{}:{}", problem.file, problem.line) : problem.file;
            const auto message = (problem.code.empty() ? "" : problem.code + "  ") + problem.text;
            ToolTooltip((where.empty() ? message : where + "\n" + message).c_str());
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::BeginPopupContextItem("##problem-menu")) {
                if (ImGui::MenuItem("Copy")) ImGui::SetClipboardText((where + ": " + message).c_str());
                ImGui::EndPopup();
            }
            auto* draw = ImGui::GetWindowDrawList();
            const float inset = ToolMetrics::Space * scale;
            const float textX = start.x + inset * 4;
            const float textWidth = std::max(1.0f, width - inset * 5 - fixWidth);
            const float cell = std::round(6 * scale);
            const auto tone = EditorLevelColor(problem.level == BuildProblemLevel::Error ? EditorLogLevel::Error
                                                                                         : EditorLogLevel::Warning);
            const float top = start.y + inset;
            const float lineHeight = ImGui::GetTextLineHeight();
            draw->AddRectFilled({start.x + inset, top + (lineHeight - cell) * 0.5f},
                                {start.x + inset + cell, top + (lineHeight + cell) * 0.5f}, ImGui::GetColorU32(tone));
            draw->AddText({textX, top}, ImGui::GetColorU32(ImGuiCol_Text),
                          ToolClipText(message.c_str(), textWidth).c_str());
            ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
            draw->AddText({textX, top + lineHeight}, ImGui::GetColorU32(ToolColors().muted),
                          ToolClipMiddle(where.c_str(), textWidth).c_str());
            ImGui::PopFont();
            if (hovered && problem.level == BuildProblemLevel::Error) {
                const auto after = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos({start.x + width - fixWidth - inset, top});
                if (ToolButton("Paste error"))
                    _agentText =
                        "[editor] Fix this build error:\n" + (where.empty() ? message : where + ": " + message) + "\n";
                ToolTooltip("Paste this error into the agent session, without Enter.");
                // Dummy clears ImGui's pending SetCursorPos state; starting one spacing above preserves `after`.
                ImGui::SetCursorScreenPos({after.x, after.y - ImGui::GetStyle().ItemSpacing.y});
                ImGui::Dummy({0, 0});
            }
            ImGui::PopID();
        }
    ImGui::EndChild();
}
void OutputPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences,
                       const std::string& agentTarget) {
    const auto& diagnostics = snapshot.diagnostics;
    const auto& attempt = diagnostics.Attempt();
    if (_attemptSeen != attempt.id) {
        _attemptSeen = attempt.id;
        _selected.clear();
        _anchor = 0;
        _failureView = false;
        _initialized = false;
    }
    const auto problemsLabel = std::format("Problems {}###problems", diagnostics.Problems().size());
    if (ToolSegment(problemsLabel.c_str(), _problems)) _problems = true;
    ToolTooltip("Errors and warnings of the latest build. A click opens the file at the line.");
    ContinueToolToolbar("Last build");
    const bool last = ToolSegment("Last build", !_problems && _lastAttempt);
    ToolTooltip("Output of the latest Build & Refresh, until the scene is ready, fails or is canceled.");
    ContinueToolToolbar("Full");
    const bool full = ToolSegment("Full", !_problems && !_lastAttempt);
    ToolTooltip("Recent editor, worker and build output (up to 1000 lines). Open log has the whole session.");
    if (_problems && !last && !full) {
        DrawProblems(snapshot, actions, agentTarget);
        actions.acknowledgeOutput();
        return;
    }
    if (last || full) {
        _problems = false;
        _lastAttempt = last;
        _failureView = false;
        _initialized = false;
        _selected.clear();
        _anchor = 0;
        _jumpToError = false;
    }
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
    SeverityFilter(EditorLogLevel::Info, counts[0], filter.info, true);
    SeverityFilter(EditorLogLevel::Warning, counts[1], filter.warning, false);
    SeverityFilter(EditorLogLevel::Error, counts[2], filter.error, false);
    // Search takes what the row has left; the buttons after it keep their size and wrap together when it is too narrow.
    const auto& style = ImGui::GetStyle();
    const float scale = ToolUiScale();
    const float buttons = ImGui::CalcTextSize("Copy").x + ImGui::CalcTextSize("Open log").x + style.FramePadding.x * 4 +
                          ToolToggleWidth("Follow") + ImGui::GetFrameHeight() + style.ItemSpacing.x * 5;
    const float searchWidth = SearchMinimumWidth * scale;
    ContinueToolToolbar(searchWidth + buttons, true);
    ImGui::SetNextItemWidth(std::max(searchWidth, ImGui::GetContentRegionAvail().x - buttons));
    if (ImGui::InputTextWithHint("##search", "Search output...", _search.data(), _search.size()))
        filter.search = _search.data();
    std::vector<const EditorLogEntry*> visible;
    for (const auto* entry : scoped)
        if (filter.Matches(*entry)) visible.push_back(entry);
    std::erase_if(_selected, [&](auto id) {
        return !std::ranges::binary_search(visible, id, {}, [](const auto* e) { return e->sequence; });
    });
    bool copyVisible = false, copySelected = false;
    ContinueToolToolbar("Copy");
    if (ToolButton("Copy")) ImGui::OpenPopup("copy-output");
    ToolTooltip("Click a line to select it, Ctrl+click to add or remove, Shift+click for a range; double-click shows "
                "its text.");
    if (ImGui::BeginPopup("copy-output")) {
        copySelected = ImGui::MenuItem("Copy Selected", "Ctrl+C", false, !_selected.empty());
        copyVisible = ImGui::MenuItem("Copy Visible", nullptr, false, !visible.empty());
        ImGui::EndPopup();
    }
    ContinueToolToolbar(ToolToggleWidth("Follow"));
    (void)ToolToggle("Follow", preferences.followOutput);
    ToolTooltip("Keep new output in view while at the bottom and no lines are selected.");
    ContinueToolToolbar(ImGui::GetFrameHeight());
    ImGui::BeginDisabled(_lastAttempt || entries.empty());
    if (EditorIconButton("clear-output", EditorGlyph::Clear, "Clear Full view buffer (log files are preserved)")) {
        actions.clearOutput();
        _selected.clear();
        visible.clear();
    }
    ImGui::EndDisabled();
    ContinueToolToolbar("Open log");
    const auto& archive = diagnostics.ArchivePath(_lastAttempt);
    std::error_code fileError;
    ImGui::BeginDisabled(archive.empty() || !std::filesystem::is_regular_file(archive, fileError));
    if (ToolButton("Open log")) actions.openOutputLog(_lastAttempt);
    ImGui::EndDisabled();
    ToolTooltip("Open the complete log file for this scope. Clear only removes the Full view's buffered lines.");
    if (_lastAttempt) {
        ImGui::PushTextWrapPos(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ToolColors().muted);
        if (attempt.id) {
            auto summary = "Attempt #" + std::to_string(attempt.id) + "  \xC2\xB7  " + TimeLabel(attempt.started) +
                           "  \xC2\xB7  " + std::format("{:.1f} s", diagnostics.AttemptSeconds()) + "  \xC2\xB7  " +
                           ResultLabel(attempt.result);
            if (attempt.exitCode) summary += "  \xC2\xB7  exit code " + std::to_string(*attempt.exitCode);
            ImGui::TextUnformatted(summary.c_str());
        } else {
            ImGui::TextUnformatted("Use Build to start a build attempt.");
        }
        if (attempt.truncated)
            ImGui::TextUnformatted(
                "Older lines were removed from this view. The first error is pinned; use Open log for all output.");
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }
    if (!diagnostics.ArchiveError().empty()) ImGui::TextWrapped("%s", diagnostics.ArchiveError().c_str());
    ImGui::Separator();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    bool openDetail = false;
    if (ImGui::BeginChild("lines", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        const bool follow = preferences.followOutput && _selected.empty() && !_jumpToError &&
                            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
        if (visible.empty())
            ToolEmptyState(scoped.empty() ? "No output yet" : "No matching output",
                           scoped.empty() ? "Editor, scene, and build messages appear here."
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
                        if (entry.text.starts_with("Crash report: ")) {
                            actions.openSource(entry.text.substr(std::string_view("Crash report: ").size()), 1);
                        } else {
                            _detail.assign(entry.text.begin(), entry.text.end());
                            _detail.push_back(0);
                            openDetail = true;
                        }
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
                ToolTooltip(entry.text.c_str());
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
            ImGui::Separator();
            if (ImGui::MenuItem("Send Selected to Agent", nullptr, false, !_selected.empty())) {
                std::vector<EditorLogEntry> chosen;
                for (const auto* entry : visible)
                    if (_selected.contains(entry->sequence)) chosen.push_back(*entry);
                _agentText = OutputContext(chosen, AgentContextMaxLines);
            }
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
        if (ToolButton("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopFont();
    actions.acknowledgeOutput();
}
