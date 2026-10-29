#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

enum class EditorLogLevel { Info, Warning, Error };
enum class EditorLogSource { Editor, Worker, Build };

struct EditorLogEntry {
    std::uint64_t sequence = 0;
    std::chrono::system_clock::time_point time;
    EditorLogLevel level = EditorLogLevel::Info;
    EditorLogSource source = EditorLogSource::Editor;
    std::string text;
};

struct EditorLogFilter {
    bool info = true;
    bool warning = true;
    bool error = true;
    std::string search;
    [[nodiscard]] bool Matches(const EditorLogEntry& entry) const;
    bool operator==(const EditorLogFilter&) const = default;
};

enum class EditorBuildResult { None, Running, Succeeded, BuildFailed, RefreshFailed, Canceled };

struct EditorBuildAttempt {
    std::uint64_t id = 0;
    std::chrono::system_clock::time_point started;
    double seconds = 0;
    EditorBuildResult result = EditorBuildResult::None;
    std::optional<unsigned long> exitCode;
    std::optional<EditorLogEntry> firstError;
    bool truncated = false;
};

class EditorDiagnostics {
public:
    void Append(std::string_view text, EditorLogLevel level = EditorLogLevel::Info,
                EditorLogSource source = EditorLogSource::Editor);
    void Clear();
    void Acknowledge();
    void StartArchive(const std::filesystem::path& file);
    void BeginAttempt();
    void BuildExited(unsigned long code);
    void FinishAttempt(EditorBuildResult result);
    [[nodiscard]] std::size_t UnreadErrors() const;
    [[nodiscard]] const std::deque<EditorLogEntry>& Entries() const { return _entries; }
    [[nodiscard]] const std::deque<EditorLogEntry>& AttemptEntries() const { return _attemptEntries; }
    [[nodiscard]] const EditorBuildAttempt& Attempt() const { return _attempt; }
    [[nodiscard]] double AttemptSeconds() const;
    [[nodiscard]] const std::filesystem::path& ArchivePath(bool attempt) const;
    [[nodiscard]] const std::string& ArchiveError() const { return _archiveError; }

private:
    std::deque<EditorLogEntry> _entries;
    std::uint64_t _nextSequence = 1;
    std::uint64_t _acknowledged = 0;
    EditorBuildAttempt _attempt;
    std::deque<EditorLogEntry> _attemptEntries;
    std::size_t _attemptBytes = 0;
    std::chrono::steady_clock::time_point _attemptStarted;
    std::filesystem::path _archivePath;
    std::filesystem::path _attemptArchivePath;
    std::ofstream _archive;
    std::ofstream _attemptArchive;
    std::string _archiveError;
};

[[nodiscard]] EditorLogLevel ClassifyBuildOutput(std::string_view text);
[[nodiscard]] std::string EditorSearchText(std::string text);
