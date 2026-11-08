#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Pure note state: callers supply disk bytes and time, perform requested writes, then acknowledge success.
class NoteDocument {
public:
    static constexpr std::size_t MaximumEditableBytes = 2 * 1024 * 1024;
    static constexpr double RetrySeconds = 5.0;
    void Open(std::string bytes);
    void Edit(std::string text, double now);
    void Observe(const std::optional<std::string>& disk);
    [[nodiscard]] std::optional<std::string> WriteDue(double now, bool flush = false) const;
    void Written(const std::string& bytes);
    // A failed write is retried after RetrySeconds, not every frame; an explicit flush still tries at once.
    void WriteFailed(double now) { _retryAt = now + RetrySeconds; }
    // The text as it would be written, while it differs from what was last read or written.
    [[nodiscard]] std::optional<std::string> UnsavedBytes() const;
    void KeepMine();
    void UseFile();
    void Recreate();
    [[nodiscard]] const std::string& Text() const { return _text; }
    [[nodiscard]] bool Dirty() const { return _dirty; }
    [[nodiscard]] bool Conflict() const { return _conflict; }
    [[nodiscard]] bool Deleted() const { return _deleted; }
    [[nodiscard]] bool ReadOnly() const { return _readOnly; }

private:
    [[nodiscard]] std::string Encode() const;
    std::string _text;
    std::string _baseline;
    std::optional<std::string> _disk;
    std::vector<std::string> _endings;
    std::string _defaultEnding = "\n";
    bool _bom = false;
    bool _dirty = false;
    bool _conflict = false;
    bool _deleted = false;
    bool _readOnly = false;
    bool _recreating = false;
    double _editedAt = 0;
    double _retryAt = 0;
};
