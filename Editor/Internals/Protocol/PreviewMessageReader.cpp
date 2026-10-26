#include "Editor/Internals/Protocol/PreviewMessageReader.h"
#include <stdexcept>
#include <utility>

PreviewMessageReader::PreviewMessageReader(std::string session)
    : _session(std::move(session)) {}
std::vector<PreviewMessage> PreviewMessageReader::Push(std::span<const std::byte> bytes) {
    // Callers feed bounded chunks; an incomplete message never grows beyond the protocol limit.
    if (bytes.size() > PreviewMaxMessageBytes + 4) throw std::runtime_error("Protocol chunk exceeds capacity");
    _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
    std::vector<PreviewMessage> messages;
    std::size_t offset = 0;
    while (_buffer.size() - offset >= 4) {
        std::uint32_t size = 0;
        for (unsigned int i = 0; i < 4; ++i)
            size |= std::to_integer<std::uint32_t>(_buffer[offset + i]) << (i * 8);
        if (size == 0 || size > PreviewMaxMessageBytes) throw std::runtime_error("Invalid protocol message length");
        if (_buffer.size() - offset - 4 < size) break;
        const auto* first = reinterpret_cast<const char*>(_buffer.data() + offset + 4);
        messages.push_back(DecodePreviewMessage(nlohmann::json::parse(first, first + size), _session));
        if (messages.size() > PreviewMaxQueuedMessages) throw std::runtime_error("Protocol receive queue overflow");
        offset += size + 4;
    }
    _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    if (_buffer.size() > PreviewMaxMessageBytes + 4) throw std::runtime_error("Protocol receive capacity exceeded");
    return messages;
}
