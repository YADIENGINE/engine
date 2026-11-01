#pragma once
#include <deque>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Protocol/WorkerMessageReader.h"

// Byte-mode PIPE_NOWAIT I/O is polled with a bounded budget; no UI or engine thread waits for a peer.
// Poll both reads and writes, so a side that stops polling lets its outgoing queue fill.
class ControlChannel {
public:
    [[nodiscard]] std::size_t QueuedBytes() const { return _queuedBytes; }
    ControlChannel(std::string session, bool server);
    // Must be delivered: a full queue is an error.
    void Send(WorkerMessage message);
    // A state snapshot: drops an unsent message of the same type and generation, then sends like Send.
    void SendLatest(WorkerMessage message);
    // Best effort (logs): refused once the queue is half full, so required messages keep room.
    [[nodiscard]] bool TrySend(WorkerMessage message);
    [[nodiscard]] std::vector<WorkerMessage> Poll();
    [[nodiscard]] bool Closed() const { return _closed; }

private:
    struct PendingWrite {
        std::vector<std::byte> bytes;
        std::size_t offset = 0;
        std::string latestType; // Non-empty: a SendLatest snapshot a newer one may replace while unsent
        std::uint64_t generation = 0;
    };
    [[nodiscard]] std::vector<std::byte> Encode(WorkerMessage& message) const;
    void Enqueue(std::vector<std::byte> bytes, std::string latestType, std::uint64_t generation);
    std::string _session;
    NativeHandle _pipe;
    WorkerMessageReader _reader;
    std::deque<PendingWrite> _outgoing;
    std::size_t _queuedBytes = 0;
    bool _connected = false;
    bool _closed = false;
};
