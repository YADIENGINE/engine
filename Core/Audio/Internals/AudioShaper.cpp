#include "Audio/Internals/AudioShaper.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr std::uint32_t FilterBlockFrames = 64; // The cutoff follows a moving muffle this often
constexpr float WetSeconds = 0.005f;
constexpr float QuarterTurn = std::numbers::pi_v<float> / 2.0f;
constexpr float ButterworthQ = std::numbers::sqrt2_v<float> / 2.0f;

[[nodiscard]] float PositionOf(float volume) { return std::asin(std::clamp(volume, 0.0f, 1.0f)) / QuarterTurn; }
[[nodiscard]] float Toward(float now, float target, float step) {
    return now + std::clamp(target - now, -step, step);
}
} // namespace

AudioShaper::AudioShaper(float sampleRate)
    : _rate(sampleRate) {}

float AudioShaper::MuffleHz(float amount) {
    return MuffleHighestHz * std::pow(MuffleLowestHz / MuffleHighestHz, std::clamp(amount, 0.0f, 1.0f));
}

void AudioShaper::Snap(const Shape& shape) {
    _left.store(shape.left, std::memory_order_relaxed);
    _right.store(shape.right, std::memory_order_relaxed);
    _snapVolume.store(shape.volume, std::memory_order_relaxed);
    _snapMuffle.store(shape.muffle, std::memory_order_relaxed);
    _snapHeld.store(shape.held, std::memory_order_relaxed);
    _heldTarget.store(shape.held, std::memory_order_relaxed);
    _volumeNow.store(shape.volume, std::memory_order_relaxed);
    _heldQuiet.store(shape.held, std::memory_order_relaxed);
    // Through the same doors as FadeTo and MuffleTo, so that one left over from the voice's last life cannot follow.
    _fadeTarget.store(shape.volume, std::memory_order_relaxed);
    _fadeSeconds.store(0.0f, std::memory_order_relaxed);
    _fadeOrder.fetch_add(1, std::memory_order_relaxed);
    _muffleTarget.store(shape.muffle, std::memory_order_relaxed);
    _muffleSeconds.store(0.0f, std::memory_order_relaxed);
    _muffleOrder.fetch_add(1, std::memory_order_relaxed);
    _snapOrder.fetch_add(1, std::memory_order_release);
}
void AudioShaper::SetEars(float left, float right) {
    _left.store(left, std::memory_order_relaxed);
    _right.store(right, std::memory_order_relaxed);
}
void AudioShaper::FadeTo(float volume, float seconds) {
    _fadeTarget.store(volume, std::memory_order_relaxed);
    _fadeSeconds.store(seconds, std::memory_order_relaxed);
    _fadeOrder.fetch_add(1, std::memory_order_release);
}
void AudioShaper::MuffleTo(float amount, float seconds) {
    _muffleTarget.store(amount, std::memory_order_relaxed);
    _muffleSeconds.store(seconds, std::memory_order_relaxed);
    _muffleOrder.fetch_add(1, std::memory_order_release);
}
void AudioShaper::SetHeld(bool held) {
    _heldTarget.store(held, std::memory_order_relaxed);
    if (!held) _heldQuiet.store(false, std::memory_order_relaxed);
}

void AudioShaper::ApplyCommands() {
    const std::uint32_t snap = _snapOrder.load(std::memory_order_acquire);
    if (snap != _snapSeen) {
        _snapSeen = snap;
        _fadeFinal = _snapVolume.load(std::memory_order_relaxed);
        _fadeGain = _fadeFinal;
        _position = PositionOf(_fadeFinal);
        _nowLeft = _left.load(std::memory_order_relaxed);
        _nowRight = _right.load(std::memory_order_relaxed);
        _hold = _snapHeld.load(std::memory_order_relaxed) ? 0.0f : 1.0f;
        _muffle = _snapMuffle.load(std::memory_order_relaxed);
        _wet = _muffle > 0.0f ? 1.0f : 0.0f;
        _state1[0] = _state1[1] = _state2[0] = _state2[1] = 0.0f;
        UpdateFilter();
    }
    const std::uint32_t fade = _fadeOrder.load(std::memory_order_acquire);
    if (fade != _fadeSeen) {
        _fadeSeen = fade;
        _fadeFinal = _fadeTarget.load(std::memory_order_relaxed);
        _positionTarget = PositionOf(_fadeFinal);
        const float seconds = std::max(_fadeSeconds.load(std::memory_order_relaxed), SmallestFadeSeconds);
        _positionStep = std::abs(_positionTarget - _position) / (seconds * _rate);
    }
    const std::uint32_t muffle = _muffleOrder.load(std::memory_order_acquire);
    if (muffle != _muffleSeen) {
        _muffleSeen = muffle;
        _muffleGoal = _muffleTarget.load(std::memory_order_relaxed);
        const float seconds = std::max(_muffleSeconds.load(std::memory_order_relaxed), SmallestFadeSeconds);
        _muffleStep = std::abs(_muffleGoal - _muffle) / (seconds * _rate);
    }
}

void AudioShaper::MoveMuffle(std::uint32_t frames) {
    if (_muffle == _muffleGoal) return;
    _muffle = Toward(_muffle, _muffleGoal, _muffleStep * static_cast<float>(frames));
    UpdateFilter();
}

// A two-pole low-pass of the topology-preserving kind (A. Simper's state variable filter). It answers as the usual biquad
// does, and its state is what the filter holds rather than a sum that belongs to one set of numbers, so the cutoff can
// move under it without a click: a biquad in the same place clicked at 7% of full scale (measured).
void AudioShaper::UpdateFilter() {
    const float hz = std::min(MuffleHz(_muffle), _rate * 0.45f);
    const float g = std::tan(std::numbers::pi_v<float> * hz / _rate);
    const float k = 1.0f / ButterworthQ;
    _a1 = 1.0f / (1.0f + g * (g + k));
    _a2 = g * _a1;
    _a3 = g * _a2;
}

void AudioShaper::Process(const float* in, float* out, std::uint32_t frames) {
    ApplyCommands();
    const float earStep = 1.0f / (GainGlideSeconds * _rate);
    const float holdStep = 1.0f / (HoldSeconds * _rate);
    const float wetStep = 1.0f / (WetSeconds * _rate);
    const float targetLeft = _left.load(std::memory_order_relaxed);
    const float targetRight = _right.load(std::memory_order_relaxed);
    const float holdTarget = _heldTarget.load(std::memory_order_relaxed) ? 0.0f : 1.0f;

    std::uint32_t done = 0;
    while (done < frames) {
        const std::uint32_t count = std::min(frames - done, FilterBlockFrames);
        MoveMuffle(count);
        const bool filtering = _muffle > 0.0f || _muffleGoal > 0.0f || _wet > 0.0f;
        const float wetTarget = (_muffle > 0.0f || _muffleGoal > 0.0f) ? 1.0f : 0.0f;
        for (std::uint32_t i = done; i < done + count; ++i) {
            float left = in ? in[i * 2] : 0.0f;
            float right = in ? in[i * 2 + 1] : 0.0f;
            if (filtering) {
                const float dry[2]{left, right};
                float wet[2];
                for (int channel = 0; channel < 2; ++channel) {
                    const float v3 = dry[channel] - _state2[channel];
                    const float v1 = _a1 * _state1[channel] + _a2 * v3;
                    const float v2 = _state2[channel] + _a2 * _state1[channel] + _a3 * v3;
                    _state1[channel] = 2.0f * v1 - _state1[channel];
                    _state2[channel] = 2.0f * v2 - _state2[channel];
                    wet[channel] = v2;
                }
                _wet = Toward(_wet, wetTarget, wetStep);
                left = dry[0] + (wet[0] - dry[0]) * _wet;
                right = dry[1] + (wet[1] - dry[1]) * _wet;
            }
            if (_position != _positionTarget) {
                _position = Toward(_position, _positionTarget, _positionStep);
                _fadeGain = _position == _positionTarget ? _fadeFinal : std::sin(_position * QuarterTurn);
            }
            _nowLeft = Toward(_nowLeft, targetLeft, earStep);
            _nowRight = Toward(_nowRight, targetRight, earStep);
            _hold = Toward(_hold, holdTarget, holdStep);
            const float gain = _fadeGain * _hold;
            out[i * 2] = left * _nowLeft * gain;
            out[i * 2 + 1] = right * _nowRight * gain;
        }
        if (filtering && wetTarget == 0.0f && _wet == 0.0f) _state1[0] = _state1[1] = _state2[0] = _state2[1] = 0.0f;
        done += count;
    }
    _volumeNow.store(_fadeGain, std::memory_order_relaxed);
    if (holdTarget == 0.0f && _hold == 0.0f) _heldQuiet.store(true, std::memory_order_relaxed);
}
