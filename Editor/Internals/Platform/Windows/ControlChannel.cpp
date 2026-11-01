#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
bool ClosedPipe(DWORD error) {
    return error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_NO_DATA;
}
} // namespace

ControlChannel::ControlChannel(std::string session, bool server)
    : _session(std::move(session))
    , _reader(_session) {
    if (_session.size() != 32 || _session.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Invalid worker session name");
    const auto name = WorkerWide("\\\\.\\pipe\\EngineEditor." + _session);
    if (server) {
        _pipe.Reset(CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                     PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                     static_cast<DWORD>(WorkerMaxMessageBytes),
                                     static_cast<DWORD>(WorkerMaxMessageBytes), 0, nullptr));
        if (!_pipe) ThrowWorkerWindowsError("Create scene control channel");
        // In PIPE_NOWAIT mode TRUE means listening, not proof of an established connection.
        const BOOL listening = ConnectNamedPipe(_pipe.Get(), nullptr);
        const DWORD error = listening ? ERROR_SUCCESS : GetLastError();
        if (error != ERROR_SUCCESS && error != ERROR_PIPE_LISTENING && error != ERROR_PIPE_CONNECTED)
            ThrowWorkerWindowsError("Listen for scene connection");
        _connected = error == ERROR_PIPE_CONNECTED;
    } else {
        _pipe.Reset(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
        if (_pipe) {
            DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
            if (!SetNamedPipeHandleState(_pipe.Get(), &mode, nullptr, nullptr))
                ThrowWorkerWindowsError("Set control channel mode");
            _connected = true;
        }
    }
    if (!_pipe) ThrowWorkerWindowsError("Open scene control channel");
}
void ControlChannel::Send(WorkerMessage message) {
    if (_closed) throw std::runtime_error("Scene control channel is closed");
    message.session = _session;
    auto bytes = EncodeWorkerMessage(message);
    constexpr std::size_t MaxQueuedBytes = 8 * WorkerMaxMessageBytes;
    if (_outgoing.size() >= WorkerMaxQueuedMessages || _queuedBytes + bytes.size() > MaxQueuedBytes)
        throw std::runtime_error("Scene control channel queue overflow");
    _queuedBytes += bytes.size();
    _outgoing.push_back({std::move(bytes), 0});
}
std::vector<WorkerMessage> ControlChannel::Poll() {
    std::vector<WorkerMessage> messages;
    if (_closed) return messages;
    if (!_connected) {
        const BOOL connected = ConnectNamedPipe(_pipe.Get(), nullptr);
        const DWORD error = connected ? ERROR_SUCCESS : GetLastError();
        if (connected || error == ERROR_PIPE_LISTENING) return messages;
        // A peer may have closed after writing; drain buffered messages before reporting EOF.
        if (error != ERROR_PIPE_CONNECTED && error != ERROR_NO_DATA) ThrowWorkerWindowsError("Connect control channel");
        _connected = true;
    }
    constexpr std::size_t PollBudget = 64 * 1024;
    std::array<std::byte, 8192> buffer{};
    std::size_t consumed = 0;
    while (consumed < PollBudget) {
        DWORD available = 0;
        if (!PeekNamedPipe(_pipe.Get(), nullptr, 0, nullptr, &available, nullptr)) {
            const auto error = GetLastError();
            if (ClosedPipe(error)) {
                _closed = true;
                break;
            }
            ThrowWorkerWindowsError("Poll control channel");
        }
        if (!available) break;
        DWORD read = 0;
        if (!ReadFile(_pipe.Get(), buffer.data(), std::min(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr)) {
            if (ClosedPipe(GetLastError())) {
                _closed = true;
                break;
            }
            ThrowWorkerWindowsError("Read control channel");
        }
        if (!read) break;
        consumed += read;
        auto incoming = _reader.Push(std::span(buffer.data(), static_cast<std::size_t>(read)));
        messages.insert(messages.end(), std::make_move_iterator(incoming.begin()),
                        std::make_move_iterator(incoming.end()));
        if (messages.size() > WorkerMaxQueuedMessages) throw std::runtime_error("Scene receive queue overflow");
    }
    std::size_t writtenTotal = 0;
    while (!_closed && !_outgoing.empty() && writtenTotal < PollBudget) {
        auto& write = _outgoing.front();
        const auto remaining = std::min(write.bytes.size() - write.offset, PollBudget - writtenTotal);
        DWORD written = 0;
        if (!WriteFile(_pipe.Get(), write.bytes.data() + write.offset, static_cast<DWORD>(remaining), &written,
                       nullptr)) {
            const auto error = GetLastError();
            if (ClosedPipe(error)) {
                _closed = true;
                break;
            }
            ThrowWorkerWindowsError("Write control channel");
        }
        if (!written) break;
        write.offset += written;
        writtenTotal += written;
        _queuedBytes -= written;
        if (write.offset == write.bytes.size()) _outgoing.pop_front();
    }
    return messages;
}
