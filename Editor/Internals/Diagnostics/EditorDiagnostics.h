#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
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

class EditorDiagnostics {
public:
    void Append(std::string_view text, EditorLogLevel level = EditorLogLevel::Info,
                EditorLogSource source = EditorLogSource::Editor);
    void Clear();
    void Acknowledge();
    [[nodiscard]] std::size_t UnreadErrors() const;
    [[nodiscard]] const std::deque<EditorLogEntry>& Entries() const { return _entries; }

private:
    std::deque<EditorLogEntry> _entries;
    std::uint64_t _nextSequence = 1;
    std::uint64_t _acknowledged = 0;
};

[[nodiscard]] EditorLogLevel ClassifyBuildOutput(std::string_view text);
[[nodiscard]] std::string EditorSearchText(std::string text);
