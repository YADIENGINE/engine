#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

inline constexpr int PreviewMaxFrameWidth = 1920;
inline constexpr int PreviewMaxFrameHeight = 1080;

struct PreviewPixels {
    int width = 0;
    int height = 0;
    std::uint64_t sequence = 0;
    double capturedAt = 0.0;
    std::vector<std::byte> rgba;
};

class SharedFrames {
public:
    SharedFrames(const std::string& session, bool owner);
    ~SharedFrames();
    // The producer/consumer skips a busy buffer instead of blocking a render thread.
    [[nodiscard]] bool Publish(std::uint64_t generation, int width, int height, std::span<const std::byte> rgba);
    [[nodiscard]] std::optional<PreviewPixels> Read(std::uint64_t generation);

private:
    struct State;
    std::unique_ptr<State> _state;
    std::uint64_t _lastSequence = 0;

public:
    SharedFrames(const SharedFrames&) = delete;
    SharedFrames& operator=(const SharedFrames&) = delete;
};

[[nodiscard]] double PreviewTimestamp();
