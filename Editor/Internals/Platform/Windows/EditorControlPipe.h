#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"

// Server end of the editor's control endpoint (EditorControlProtocol.h): one client at a time, polled from the frame
// loop without blocking (PIPE_NOWAIT, like ControlChannel). When a client leaves, the pipe listens for the next one.
class EditorControlPipe {
public:
    EditorControlPipe();
    [[nodiscard]] const std::string& Name() const { return _name; }
    [[nodiscard]] bool Connected() const { return _connected; }
    // Counts connections; a reply belongs to the client that asked.
    [[nodiscard]] std::uint64_t Client() const { return _client; }
    // Complete lines from the current client. Also flushes queued replies and notices a client that left.
    [[nodiscard]] std::vector<std::string> Poll();
    void Send(std::string_view line);

private:
    void Listen();
    void Drop();
    void Flush();
    std::string _name;
    NativeHandle _pipe;
    std::string _incoming;
    std::string _outgoing;
    std::uint64_t _client = 0;
    bool _connected = false;
};
