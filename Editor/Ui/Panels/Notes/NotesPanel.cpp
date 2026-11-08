#include "Editor/Ui/Panels/Notes/NotesPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include <exception>
#include "ToolUi/ToolWidgets.h"

NotesPanel::~NotesPanel() { Shutdown(); }
void NotesPanel::Shutdown() {
    (void)Flush();
    if (_path.empty()) return;
    try {
        if (const auto unsaved = _document.UnsavedBytes()) (void)_files.SaveRecovery(_path, *unsaved);
    } catch (const std::exception&) {
        // The editor is closing and has nowhere left to show this.
    }
    Forget();
}
void NotesPanel::Forget() {
    _document = {};
    _path.clear();
    _buffer.clear();
    _error.clear();
    _pollError.clear();
    _reloadBuffer = false;
}
std::string NotesPanel::AgentPath() const {
    const auto utf8 = (std::filesystem::path("Notes") / _path.lexically_relative(_files.Root())).generic_u8string();
    return {utf8.begin(), utf8.end()};
}
void NotesPanel::SyncBuffer() {
    _reloadBuffer = true;
    _buffer.resize(_document.Text().size() + 4096);
    std::memcpy(_buffer.data(), _document.Text().c_str(), _document.Text().size() + 1);
}
void NotesPanel::Observe() {
    const auto before = _document.Text();
    _document.Observe(_files.Read(_path));
    if (before != _document.Text()) SyncBuffer();
}
bool NotesPanel::Save(bool flush, bool observe) {
    if (_path.empty()) return true;
    try {
        if (observe) Observe();
        if (const auto bytes = _document.WriteDue(_now, flush)) {
            _files.Write(_path, *bytes);
            _document.Written(*bytes);
        }
        _error.clear();
        return !_document.Dirty() && !_document.Conflict() && !_document.Deleted();
    } catch (const std::exception& error) {
        _error = error.what();
        _document.WriteFailed(_now);
        return false;
    }
}
bool NotesPanel::Flush() { return Save(true); }
bool NotesPanel::Open(const std::filesystem::path& path) {
    if (path == _path) return true;
    if (!Flush()) {
        if (_error.empty()) _error = "Resolve this note's disk change before opening another note.";
        return false;
    }
    try {
        const auto bytes = _files.Read(path);
        if (!bytes) {
            _error = "The note no longer exists.";
            return false;
        }
        _document.Open(*bytes);
        _path = path;
        SyncBuffer();
        _error.clear();
        _nextPoll = 0;
        return true;
    } catch (const std::exception& error) {
        _error = error.what();
        return false;
    }
}
void NotesPanel::Renamed(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (_path.empty()) return;
    const auto relative = _path.lexically_relative(from);
    if (!relative.empty() && *relative.begin() != "..") _path = relative == "." ? to : to / relative;
}
void NotesPanel::Tick(double now, bool focused) {
    _now = now;
    if (!_path.empty() && now >= _nextPoll) {
        try {
            Observe();
            _pollError.clear();
        } catch (const std::exception& error) {
            _pollError = error.what();
        }
        _nextPoll = now + 1.0;
    }
    if (_wasFocused && !focused)
        (void)Flush();
    else if (_document.WriteDue(now))
        (void)Save(false);
    _focused = _wasFocused = focused;
}
std::string NotesPanel::Draw() {
    _focused = !ImGui::GetIO().AppFocusLost && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (_path.empty()) {
        ToolEmptyState("Project notes", "Open a Markdown note from Assets > Notes, or create one there.");
        if (!_error.empty()) ImGui::TextWrapped("%s", _error.c_str());
        return {};
    }
    const auto relative = AgentPath();
    ImGui::TextWrapped("%s", relative.c_str());
    std::string paste;
    if (ToolButton("Send to agent")) paste = relative;
    ImGui::SameLine();
    ImGui::TextDisabled("%s", _document.Deleted()    ? "File deleted"
                              : _document.Conflict() ? "Changed on disk"
                              : _document.Dirty()    ? "Unsaved"
                                                     : "Saved");
    if (_document.Conflict()) {
        ImGui::TextWrapped("Changed on disk. Choose which version to keep.");
        if (ToolButton("Keep mine (overwrite)")) {
            try {
                Observe();
                _document.KeepMine();
                (void)Save(true, false);
            } catch (const std::exception& error) {
                _error = error.what();
            }
        }
        ImGui::SameLine();
        if (ToolButton("Use the file (discard mine)")) {
            try {
                Observe();
                _document.UseFile();
                SyncBuffer();
                _error.clear();
            } catch (const std::exception& error) {
                _error = error.what();
            }
        }
    }
    if (_document.Deleted()) {
        ImGui::TextWrapped("File deleted. Your text is kept here.");
        if (ToolButton("Recreate")) {
            try {
                Observe();
                if (_document.Deleted()) {
                    _document.Recreate();
                    if (const auto bytes = _document.WriteDue(_now, true)) {
                        _files.Write(_path, *bytes, false);
                        _document.Written(*bytes);
                        _error.clear();
                    }
                }
            } catch (const std::exception& error) {
                _document.Observe(std::nullopt);
                _error = error.what();
            }
        }
        ImGui::SameLine();
        if (ToolButton("Discard text")) {
            Forget();
            return {};
        }
    }
    if (_document.ReadOnly()) ImGui::TextWrapped("This note is over 2 MiB and is read-only.");
    if (!_error.empty()) ImGui::TextWrapped("%s", _error.c_str());
    if (!_pollError.empty()) ImGui::TextWrapped("%s", _pollError.c_str());
    if (_focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) (void)Flush();
    const auto textId = ImGui::GetID("##note-text");
    if (_reloadBuffer) {
        if (auto* state = ImGui::GetInputTextState(textId)) state->ReloadUserBufAndKeepSelection();
        _reloadBuffer = false;
    }
    const auto resize = [](ImGuiInputTextCallbackData* data) {
        auto& buffer = *static_cast<std::vector<char>*>(data->UserData);
        buffer.resize(static_cast<std::size_t>(data->BufSize));
        data->Buf = buffer.data();
        return 0;
    };
    const auto flags = ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackResize |
                       (_document.ReadOnly() ? ImGuiInputTextFlags_ReadOnly : 0);
    if (ImGui::InputTextMultiline(
            "##note-text", _buffer.data(), _buffer.size(),
            {std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(1.0f, ImGui::GetContentRegionAvail().y)}, flags,
            resize, &_buffer))
        _document.Edit(_buffer.data(), ImGui::GetTime());
    return paste;
}
