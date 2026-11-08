#include "Editor/Internals/Notes/NoteDocument.h"
#include <utility>

void NoteDocument::Open(std::string bytes) {
    _baseline = std::move(bytes);
    _disk = _baseline;
    _readOnly = _baseline.size() > MaximumEditableBytes;
    _dirty = _conflict = _deleted = _recreating = false;
    _retryAt = 0;
    _text.clear();
    _endings.clear();
    _bom = _baseline.starts_with("\xEF\xBB\xBF");
    for (std::size_t i = _bom ? 3 : 0; i < _baseline.size(); ++i) {
        if (_baseline[i] == '\r') {
            const bool pair = i + 1 < _baseline.size() && _baseline[i + 1] == '\n';
            _endings.push_back(pair ? "\r\n" : "\r");
            if (pair) ++i;
            _text += '\n';
        } else {
            if (_baseline[i] == '\n') _endings.emplace_back("\n");
            _text += _baseline[i];
        }
    }
    _defaultEnding = _endings.empty() ? "\n" : _endings.front();
}
void NoteDocument::Edit(std::string text, double now) {
    if (_readOnly || text == _text) return;
    _text = std::move(text);
    _dirty = Encode() != _baseline;
    _editedAt = now;
}
void NoteDocument::Observe(const std::optional<std::string>& disk) {
    _disk = disk;
    if (!disk) {
        _deleted = true;
        _conflict = false;
        return;
    }
    // A deletion requires a deliberate choice even if another process recreates the file.
    if (_deleted) {
        _deleted = false;
        _conflict = true;
        return;
    }
    if (*disk == _baseline) return;
    if (_dirty || _conflict)
        _conflict = true;
    else
        Open(*disk);
}
std::string NoteDocument::Encode() const {
    std::string bytes = _bom ? "\xEF\xBB\xBF" : "";
    std::size_t line = 0;
    for (const char c : _text) {
        if (c == '\n') {
            bytes += line < _endings.size() ? _endings[line] : _defaultEnding;
            ++line;
        } else
            bytes += c;
    }
    return bytes;
}
std::optional<std::string> NoteDocument::WriteDue(double now, bool flush) const {
    if (!_dirty || (_readOnly && !_recreating) || _conflict || _deleted) return {};
    if (!flush && (now - _editedAt < 1.0 || now < _retryAt)) return {};
    return Encode();
}
std::optional<std::string> NoteDocument::UnsavedBytes() const {
    if (!_dirty) return {};
    return Encode();
}
void NoteDocument::Written(const std::string& bytes) { Open(bytes); }
void NoteDocument::KeepMine() {
    if (!_disk || !_conflict) return;
    _baseline = *_disk;
    _conflict = false;
    _dirty = true;
}
void NoteDocument::UseFile() {
    if (_disk) Open(*_disk);
}
void NoteDocument::Recreate() {
    if (!_deleted) return;
    _recreating = true;
    _deleted = false;
    _dirty = true;
}
