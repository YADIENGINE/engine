#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"

// Server end of the editor's control endpoint (EditorControlProtocol.h), polled from the frame loop without blocking
// (PIPE_NOWAIT, like ControlChannel). Up to Instances clients are connected at once, so one agent's long request (a
// build, a seek) does not keep another's status or note waiting. When a client leaves, its instance listens again.
class EditorControlPipe {
public:
    static constexpr int Instances = 8;
    struct Line {
        std::uint64_t client = 0;
        std::string text;
    };

    EditorControlPipe();
    [[nodiscard]] const std::string& Name() const { return _name; }
    // Counts connections from 1; a reply belongs to the client that asked.
    [[nodiscard]] bool Connected(std::uint64_t client) const;
    // Complete lines from every connected client. Also flushes queued replies and notices clients that left.
    [[nodiscard]] std::vector<Line> Poll();
    // Queues a line for `client`; nothing when it has left.
    void Send(std::uint64_t client, std::string_view line);

private:
    struct Instance {
        NativeHandle pipe;
        std::string incoming;
        std::string outgoing;
        std::uint64_t client = 0;
        bool connected = false;
    };
    void Listen(Instance& instance);
    void Read(Instance& instance, std::vector<Line>& lines);
    static void Drop(Instance& instance);
    static void Flush(Instance& instance);
    std::string _name;
    std::vector<std::unique_ptr<Instance>> _instances;
    std::uint64_t _lastClient = 0;
};
