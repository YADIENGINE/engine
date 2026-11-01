#include "Editor/Internals/Platform/Windows/EditorControlPipe.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <utility>
#include "Editor/Internals/Platform/Windows/WindowsText.h"
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

namespace {
constexpr DWORD PipeBufferBytes = 64 * 1024;
constexpr std::size_t PollBudget = 64 * 1024;
constexpr std::size_t MaxQueuedReplyBytes = 4 * 1024 * 1024; // A client that stops reading is dropped
bool ClosedPipe(DWORD error) {
    return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA;
}
} // namespace

EditorControlPipe::EditorControlPipe()
    : _name("\\\\.\\pipe\\EngineEditor.Control." + NewWorkerSession()) {
    const auto name = WorkerWide(_name);
    _pipe.Reset(CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                 PipeBufferBytes, PipeBufferBytes, 0, nullptr));
    if (!_pipe) ThrowWorkerWindowsError("Create editor control endpoint");
}
std::vector<std::string> EditorControlPipe::Poll() {
    std::vector<std::string> lines;
    if (!_connected) Listen();
    if (!_connected) return lines;
    std::array<char, 4096> buffer{};
    std::size_t consumed = 0;
    while (consumed < PollBudget) {
        DWORD available = 0;
        if (!PeekNamedPipe(_pipe.Get(), nullptr, 0, nullptr, &available, nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Poll editor control endpoint");
            Drop();
            return lines;
        }
        if (!available) break;
        DWORD read = 0;
        if (!ReadFile(_pipe.Get(), buffer.data(), std::min(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Read editor control endpoint");
            Drop();
            return lines;
        }
        if (!read) break;
        consumed += read;
        _incoming.append(buffer.data(), read);
        for (auto newline = _incoming.find('\n'); newline != std::string::npos; newline = _incoming.find('\n')) {
            auto line = _incoming.substr(0, newline);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
            _incoming.erase(0, newline + 1);
        }
        if (_incoming.size() > EditorControlMaxLineBytes) { // No request is this long
            Drop();
            return {};
        }
    }
    Flush();
    return lines;
}
void EditorControlPipe::Send(std::string_view line) {
    if (!_connected) return;
    _outgoing.append(line);
    _outgoing.push_back('\n');
    if (_outgoing.size() > MaxQueuedReplyBytes) Drop();
}
void EditorControlPipe::Listen() {
    const BOOL listening = ConnectNamedPipe(_pipe.Get(), nullptr);
    const DWORD error = listening ? ERROR_SUCCESS : GetLastError();
    // In PIPE_NOWAIT mode TRUE means listening, not proof of a connection.
    if (listening || error == ERROR_PIPE_LISTENING) return;
    // ERROR_NO_DATA: the client already closed; what it wrote can still be read.
    if (error != ERROR_PIPE_CONNECTED && error != ERROR_NO_DATA)
        ThrowWorkerWindowsError("Listen on editor control endpoint");
    _connected = true;
    ++_client;
}
void EditorControlPipe::Drop() {
    DisconnectNamedPipe(_pipe.Get());
    _connected = false;
    _incoming.clear();
    _outgoing.clear();
}
void EditorControlPipe::Flush() {
    while (_connected && !_outgoing.empty()) {
        DWORD written = 0;
        const auto size = static_cast<DWORD>(std::min(_outgoing.size(), PollBudget));
        if (!WriteFile(_pipe.Get(), _outgoing.data(), size, &written, nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Write editor control endpoint");
            Drop();
            return;
        }
        if (!written) return; // The pipe buffer is full; the rest goes on a later poll
        _outgoing.erase(0, written);
    }
}
