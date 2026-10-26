#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include <algorithm>
#include <cctype>

namespace {
constexpr std::size_t MaxEntries = 1000;
constexpr std::size_t MaxLineBytes = 16 * 1024;
} // namespace

std::string EditorSearchText(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}
bool EditorLogFilter::Matches(const EditorLogEntry& entry) const {
    const bool enabled = entry.level == EditorLogLevel::Info      ? info
                         : entry.level == EditorLogLevel::Warning ? warning
                                                                  : error;
    return enabled &&
           (search.empty() || EditorSearchText(entry.text).find(EditorSearchText(search)) != std::string::npos);
}
void EditorDiagnostics::Append(std::string_view text, EditorLogLevel level, EditorLogSource source) {
    const auto time = std::chrono::system_clock::now();
    do {
        const auto newline = text.find('\n');
        auto line = text.substr(0, newline);
        if (line.ends_with('\r')) line.remove_suffix(1);
        _entries.push_back({_nextSequence++, time, level, source, std::string(line.substr(0, MaxLineBytes))});
        while (_entries.size() > MaxEntries)
            _entries.pop_front();
        if (newline == std::string_view::npos) break;
        text.remove_prefix(newline + 1);
    } while (!text.empty());
}
void EditorDiagnostics::Clear() {
    _entries.clear();
    Acknowledge();
}
void EditorDiagnostics::Acknowledge() { _acknowledged = _nextSequence - 1; }
std::size_t EditorDiagnostics::UnreadErrors() const {
    return static_cast<std::size_t>(std::ranges::count_if(_entries, [this](const auto& entry) {
        return entry.sequence > _acknowledged && entry.level == EditorLogLevel::Error;
    }));
}
EditorLogLevel ClassifyBuildOutput(std::string_view text) {
    if (text.starts_with("CMake Error") || text.find(": error C") != std::string_view::npos ||
        text.find(": fatal error ") != std::string_view::npos || text.find(": error LNK") != std::string_view::npos)
        return EditorLogLevel::Error;
    if (text.starts_with("CMake Warning") || text.find(": warning C") != std::string_view::npos ||
        text.find(": warning LNK") != std::string_view::npos)
        return EditorLogLevel::Warning;
    return EditorLogLevel::Info;
}
