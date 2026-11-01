#pragma once
#include <span>
#include "Editor/Internals/Protocol/WorkerProtocol.h"

class WorkerMessageReader {
public:
    explicit WorkerMessageReader(std::string session);
    [[nodiscard]] std::vector<WorkerMessage> Push(std::span<const std::byte> bytes);

private:
    std::string _session;
    std::vector<std::byte> _buffer;
};
