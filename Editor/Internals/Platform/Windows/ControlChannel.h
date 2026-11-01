#pragma once
#include <deque>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Protocol/WorkerMessageReader.h"

// Byte-mode PIPE_NOWAIT I/O is polled with a bounded budget; no UI or engine thread waits for a peer.
class ControlChannel {
public:
    [[nodiscard]] std::size_t QueuedBytes() const { return _queuedBytes; }
    ControlChannel(std::string session, bool server);
    void Send(WorkerMessage message);
    [[nodiscard]] std::vector<WorkerMessage> Poll();
    [[nodiscard]] bool Closed() const { return _closed; }

private:
    struct PendingWrite {
        std::vector<std::byte> bytes;
        std::size_t offset = 0;
    };
    std::string _session;
    NativeHandle _pipe;
    WorkerMessageReader _reader;
    std::deque<PendingWrite> _outgoing;
    std::size_t _queuedBytes = 0;
    bool _connected = false;
    bool _closed = false;
};
