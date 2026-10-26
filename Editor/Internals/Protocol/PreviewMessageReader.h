#pragma once
#include <span>
#include "Editor/Internals/Protocol/PreviewProtocol.h"

class PreviewMessageReader {
public:
    explicit PreviewMessageReader(std::string session);
    [[nodiscard]] std::vector<PreviewMessage> Push(std::span<const std::byte> bytes);

private:
    std::string _session;
    std::vector<std::byte> _buffer;
};
