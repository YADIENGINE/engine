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
    for (int index = 0; index < Instances; ++index) {
        // The first instance claims the name, so no other process can have created it before us.
        const DWORD access = PIPE_ACCESS_DUPLEX | (index == 0 ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        auto& instance = *_instances.emplace_back(std::make_unique<Instance>());
        instance.pipe.Reset(CreateNamedPipeW(
            name.c_str(), access, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
            Instances, PipeBufferBytes, PipeBufferBytes, 0, nullptr));
        if (!instance.pipe) ThrowWorkerWindowsError("Create editor control endpoint");
    }
}
bool EditorControlPipe::Connected(std::uint64_t client) const {
    return std::ranges::any_of(_instances,
                               [&](const auto& instance) { return instance->connected && instance->client == client; });
}
std::vector<EditorControlPipe::Line> EditorControlPipe::Poll() {
    std::vector<Line> lines;
    for (auto& pointer : _instances) {
        auto& instance = *pointer;
        if (!instance.connected) Listen(instance);
        if (!instance.connected) continue;
        Read(instance, lines);
        Flush(instance);
    }
    return lines;
}
void EditorControlPipe::Read(Instance& instance, std::vector<Line>& lines) {
    std::array<char, 4096> buffer{};
    std::size_t consumed = 0;
    while (consumed < PollBudget) {
        DWORD available = 0;
        if (!PeekNamedPipe(instance.pipe.Get(), nullptr, 0, nullptr, &available, nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Poll editor control endpoint");
            Drop(instance);
            return;
        }
        if (!available) return;
        DWORD read = 0;
        if (!ReadFile(instance.pipe.Get(), buffer.data(), std::min(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Read editor control endpoint");
            Drop(instance);
            return;
        }
        if (!read) return;
        consumed += read;
        instance.incoming.append(buffer.data(), read);
        for (auto newline = instance.incoming.find('\n'); newline != std::string::npos;
             newline = instance.incoming.find('\n')) {
            auto line = instance.incoming.substr(0, newline);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back({instance.client, std::move(line)});
            instance.incoming.erase(0, newline + 1);
        }
        if (instance.incoming.size() > EditorControlMaxLineBytes) { // No request is this long
            std::erase_if(lines, [&](const Line& line) { return line.client == instance.client; });
            Drop(instance);
            return;
        }
    }
}
void EditorControlPipe::Send(std::uint64_t client, std::string_view line) {
    for (auto& pointer : _instances) {
        auto& instance = *pointer;
        if (!instance.connected || instance.client != client) continue;
        instance.outgoing.append(line);
        instance.outgoing.push_back('\n');
        if (instance.outgoing.size() > MaxQueuedReplyBytes) Drop(instance);
        return;
    }
}
void EditorControlPipe::Listen(Instance& instance) {
    const BOOL listening = ConnectNamedPipe(instance.pipe.Get(), nullptr);
    const DWORD error = listening ? ERROR_SUCCESS : GetLastError();
    // In PIPE_NOWAIT mode TRUE means listening, not proof of a connection.
    if (listening || error == ERROR_PIPE_LISTENING) return;
    // ERROR_NO_DATA: the client already closed; what it wrote can still be read.
    if (error != ERROR_PIPE_CONNECTED && error != ERROR_NO_DATA)
        ThrowWorkerWindowsError("Listen on editor control endpoint");
    instance.connected = true;
    instance.client = ++_lastClient;
}
void EditorControlPipe::Drop(Instance& instance) {
    DisconnectNamedPipe(instance.pipe.Get());
    instance.connected = false;
    instance.incoming.clear();
    instance.outgoing.clear();
}
void EditorControlPipe::Flush(Instance& instance) {
    while (instance.connected && !instance.outgoing.empty()) {
        DWORD written = 0;
        const auto size = static_cast<DWORD>(std::min(instance.outgoing.size(), PollBudget));
        if (!WriteFile(instance.pipe.Get(), instance.outgoing.data(), size, &written, nullptr)) {
            if (!ClosedPipe(GetLastError())) ThrowWorkerWindowsError("Write editor control endpoint");
            Drop(instance);
            return;
        }
        if (!written) return; // The pipe buffer is full; the rest goes on a later poll
        instance.outgoing.erase(0, written);
    }
}
