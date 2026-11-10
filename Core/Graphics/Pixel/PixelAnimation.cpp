#include "Graphics/Pixel/PixelAnimation.h"
#include <cmath>
#include <stdexcept>
#include <utility>

PixelAnimation::PixelAnimation(std::initializer_list<PixelFrame> frames, bool loops)
    : PixelAnimation(std::vector<PixelFrame>(frames), loops) {}

PixelAnimation::PixelAnimation(std::vector<PixelFrame> frames, bool loops)
    : _frames(std::move(frames))
    , _loops(loops) {
    if (_frames.empty()) throw std::invalid_argument("PixelAnimation: no frames");
    double length = 0.0;
    for (const auto& frame : _frames) {
        if (!std::isfinite(frame.seconds) || frame.seconds <= 0.0f)
            throw std::invalid_argument("PixelAnimation: every frame must last longer than 0 seconds");
        length += frame.seconds;
    }
    _length = static_cast<float>(length);
}

PixelAnimation PixelAnimation::Even(std::size_t count, float framesPerSecond, std::size_t first, bool loops) {
    if (!std::isfinite(framesPerSecond) || framesPerSecond <= 0.0f)
        throw std::invalid_argument("PixelAnimation: frames per second must be above 0");
    std::vector<PixelFrame> frames;
    frames.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
        frames.push_back({first + index, 1.0f / framesPerSecond});
    return PixelAnimation(std::move(frames), loops);
}

std::size_t PixelAnimation::FrameAt(float time) const {
    if (!(time > 0.0f)) return 0; // Also NaN
    double at = time;
    if (_loops) {
        at = std::fmod(at, static_cast<double>(_length));
    } else if (at >= _length) {
        return _frames.size() - 1;
    }
    for (std::size_t index = 0; index < _frames.size(); ++index) {
        if (at < _frames[index].seconds) return index;
        at -= _frames[index].seconds;
    }
    return _frames.size() - 1; // Rounding at the very end
}
