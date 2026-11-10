#include "Editor/Internals/Pixelator/PixelatorPlayback.h"
#include <algorithm>

namespace {
constexpr double MinFrameSeconds = 1.0 / 240.0; // A zero duration still advances
}

void PixelatorPlayback::Reset(std::span<const PixelatorFrame> frames, bool play) {
    _frame = 0;
    _shown = 0.0;
    _playing = play && frames.size() > 1;
}

void PixelatorPlayback::Advance(std::span<const PixelatorFrame> frames, double seconds) {
    if (frames.empty()) {
        _frame = 0;
        return;
    }
    _frame = std::min(_frame, frames.size() - 1);
    if (!_playing || frames.size() < 2 || seconds <= 0.0) return;
    _shown += seconds;
    for (int guard = 0; guard < 1024; ++guard) {
        const double length = std::max<double>(frames[_frame].seconds, MinFrameSeconds);
        if (_shown < length) break;
        _shown -= length;
        _frame = (_frame + 1) % frames.size();
    }
}

void PixelatorPlayback::Step(std::span<const PixelatorFrame> frames, int steps) {
    _playing = false;
    _shown = 0.0;
    if (frames.empty()) return;
    const auto count = static_cast<long long>(frames.size());
    const long long next = (static_cast<long long>(std::min(_frame, frames.size() - 1)) + steps) % count;
    _frame = static_cast<std::size_t>(next < 0 ? next + count : next);
}

void PixelatorPlayback::Select(std::span<const PixelatorFrame> frames, std::size_t frame) {
    _shown = 0.0;
    _frame = frames.empty() ? 0 : std::min(frame, frames.size() - 1);
}
