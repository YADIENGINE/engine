#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Editor/Internals/Notes/NoteDocument.h"
#include "Editor/Internals/Notes/NoteFiles.h"

class NotesPanel {
public:
    ~NotesPanel();
    void SetProject(const std::filesystem::path& project) { _files = NoteFiles(project); }
    bool Open(const std::filesystem::path& path);
    void Renamed(const std::filesystem::path& from, const std::filesystem::path& to);
    void Tick(double now, bool focused);
    bool Flush();
    // Saves what it can, keeps edits that could not be saved in a recovered copy, and closes the note.
    void Shutdown();
    // The open note's project-relative path, as sent to an agent.
    [[nodiscard]] std::string AgentPath() const;
    // Returns a project-relative path to paste, without Enter.
    [[nodiscard]] std::string Draw();
    [[nodiscard]] bool Focused() const { return _focused; }

private:
    void Observe();
    bool Save(bool flush, bool observe = true);
    void SyncBuffer();
    void Forget();
    NoteFiles _files;
    NoteDocument _document;
    std::filesystem::path _path;
    std::vector<char> _buffer;
    std::string _error;     // From the last action; stays until an action succeeds
    std::string _pollError; // From the background disk check; stays until a check succeeds
    double _now = 0;
    double _nextPoll = 0;
    bool _reloadBuffer = false;
    bool _focused = false;
    bool _wasFocused = false;
};
