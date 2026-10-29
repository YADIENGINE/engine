#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <utility>

namespace {
constexpr std::size_t MaxEntries = 1000;
constexpr std::size_t MaxLineBytes = 16 * 1024;
constexpr std::size_t MaxAttemptEntries = 10000;
constexpr std::size_t MaxAttemptBytes = 8 * 1024 * 1024;
void ArchiveLine(std::ofstream& stream, const EditorLogEntry& entry, std::string_view text, std::string& error) {
    if (!stream.is_open()) return;
    const auto time = std::chrono::system_clock::to_time_t(entry.time);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    constexpr std::array Sources{"Editor", "Worker", "Build"};
    constexpr std::array Levels{"Info", "Warning", "Error"};
    stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << " [" << Sources[static_cast<std::size_t>(entry.source)]
           << '/' << Levels[static_cast<std::size_t>(entry.level)] << "] ";
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream << '\n';
    stream.flush();
    if (!stream) {
        error = "Could not write output archive. The log file may be incomplete.";
        stream.close();
    }
}
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
        EditorLogEntry entry{_nextSequence++, time, level, source, std::string(line.substr(0, MaxLineBytes))};
        ArchiveLine(_archive, entry, line, _archiveError);
        if (_attempt.result == EditorBuildResult::Running) {
            ArchiveLine(_attemptArchive, entry, line, _archiveError);
            // Prefer the actual compiler diagnostic over Ninja's preceding FAILED target marker.
            if (level == EditorLogLevel::Error &&
                (!_attempt.firstError || _attempt.firstError->text.starts_with("FAILED:")))
                _attempt.firstError = entry;
            _attemptBytes += entry.text.size();
            _attemptEntries.push_back(entry);
            while (_attemptEntries.size() > MaxAttemptEntries || _attemptBytes > MaxAttemptBytes) {
                _attemptBytes -= _attemptEntries.front().text.size();
                _attemptEntries.pop_front();
                _attempt.truncated = true;
            }
        }
        _entries.push_back(std::move(entry));
        while (_entries.size() > MaxEntries)
            _entries.pop_front();
        if (newline == std::string_view::npos) break;
        text.remove_prefix(newline + 1);
    } while (!text.empty());
}
void EditorDiagnostics::StartArchive(const std::filesystem::path& file) {
    _archive.close();
    _archive.clear();
    _archiveError.clear();
    std::error_code error;
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path(), error);
    if (!error) _archive.open(file, std::ios::binary | std::ios::trunc);
    _archivePath = file;
    if (error || !_archive.is_open())
        _archiveError = "Could not create output archive. Only buffered output is available.";
}
void EditorDiagnostics::BeginAttempt() {
    const auto id = _attempt.id + 1;
    _attempt = {};
    _attempt.id = id;
    _attempt.started = std::chrono::system_clock::now();
    _attemptStarted = std::chrono::steady_clock::now();
    _attempt.result = EditorBuildResult::Running;
    _attemptEntries.clear();
    _attemptBytes = 0;
    _attemptArchive.close();
    _attemptArchive.clear();
    _attemptArchivePath.clear();
    if (!_archivePath.empty()) {
        _attemptArchivePath =
            _archivePath.parent_path() / (_archivePath.stem().string() + ".attempt-" + std::to_string(id) + ".log");
        _attemptArchive.open(_attemptArchivePath, std::ios::binary | std::ios::trunc);
        if (!_attemptArchive.is_open()) _archiveError = "Could not create the build attempt log.";
    }
    Append("Build & Refresh attempt #" + std::to_string(id) + " started.");
}
void EditorDiagnostics::BuildExited(unsigned long code) {
    if (_attempt.result == EditorBuildResult::Running) _attempt.exitCode = code;
}
void EditorDiagnostics::FinishAttempt(EditorBuildResult result) {
    if (_attempt.result != EditorBuildResult::Running) return;
    Append(result == EditorBuildResult::Succeeded  ? "Build & Refresh completed."
           : result == EditorBuildResult::Canceled ? "Build & Refresh canceled."
                                                   : "Build & Refresh failed.");
    _attempt.seconds = AttemptSeconds();
    _attempt.result = result;
    _attemptArchive.close();
}
double EditorDiagnostics::AttemptSeconds() const {
    return _attempt.result == EditorBuildResult::Running
               ? std::chrono::duration<double>(std::chrono::steady_clock::now() - _attemptStarted).count()
               : _attempt.seconds;
}
const std::filesystem::path& EditorDiagnostics::ArchivePath(bool attempt) const {
    return attempt ? _attemptArchivePath : _archivePath;
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
    if (text.starts_with("CMake Error") || text.starts_with("FAILED:") || text.starts_with("ninja: error:") ||
        text.find(": error:") != std::string_view::npos || text.find(": fatal error:") != std::string_view::npos ||
        text.find(": error C") != std::string_view::npos || text.find(": fatal error ") != std::string_view::npos ||
        text.find(": error LNK") != std::string_view::npos)
        return EditorLogLevel::Error;
    if (text.starts_with("CMake Warning") || text.find(": warning:") != std::string_view::npos ||
        text.find(": warning C") != std::string_view::npos || text.find(": warning LNK") != std::string_view::npos)
        return EditorLogLevel::Warning;
    return EditorLogLevel::Info;
}
