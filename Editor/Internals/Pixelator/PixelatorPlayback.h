#pragma once
#include <cstddef>
#include <span>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"

// The Pixelator's frame strip: which frame shows, and whether the frames play at their own durations (looping).
class PixelatorPlayback {
public:
    // New frames: back to the first one; plays when there is more than one frame and `play`.
    void Reset(std::span<const PixelatorFrame> frames, bool play = true);
    // Moves on by `seconds` of real time while playing.
    void Advance(std::span<const PixelatorFrame> frames, double seconds);
    // One frame forward or back (wrapping), pausing.
    void Step(std::span<const PixelatorFrame> frames, int steps);
    void Select(std::span<const PixelatorFrame> frames, std::size_t frame);
    void SetPlaying(bool playing) { _playing = playing; }
    [[nodiscard]] bool Playing() const { return _playing; }
    [[nodiscard]] std::size_t Frame() const { return _frame; }

private:
    std::size_t _frame = 0;
    double _shown = 0.0; // Seconds the current frame has been on screen
    bool _playing = false;
};
