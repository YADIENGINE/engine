#pragma once
#include <atomic>
#include <cstdint>

// What every voice and every bus does to the sound going through it: a gain for each ear, a volume with fades and a
// low-pass filter for muffling. It knows no audio library, so that a test can feed it samples directly. The main thread
// states where things should go; the audio thread moves them there sample by sample, so no change can click and none
// waits for a frame. Fades move in a space where two crossing fades keep their power: the gain at position p is
// sin(p * pi / 2), so that one fading out while the other fades in adds up to the same loudness all the way.
class AudioShaper {
public:
    struct Shape {
        float left = 1.0f;
        float right = 1.0f;
        float volume = 1.0f;
        float muffle = 0.0f;
        bool held = false;
    };

    static constexpr float GainGlideSeconds = 0.02f;    // How long a change of ears takes from one extreme to the other
    static constexpr float SmallestFadeSeconds = 0.01f; // A shorter one would click
    static constexpr float HoldSeconds = 0.005f;
    static constexpr float MuffleLowestHz = 300.0f;     // Where the filter ends up at muffle 1
    static constexpr float MuffleHighestHz = 20000.0f;  // Where it starts, above muffle 0

    explicit AudioShaper(float sampleRate);

    // From the main thread. Snap puts everything in place at once, for a voice that is about to start.
    void Snap(const Shape& shape);
    void SetEars(float left, float right);
    void FadeTo(float volume, float seconds);
    void MuffleTo(float amount, float seconds);
    void SetHeld(bool held);
    // How far the fade and the hold have come, as of the last block.
    [[nodiscard]] float Volume() const { return _volumeNow.load(std::memory_order_relaxed); }
    [[nodiscard]] bool HeldQuiet() const { return _heldQuiet.load(std::memory_order_relaxed); }

    // On the audio thread. Interleaved stereo; `in` may be null for silence, and may be the same as `out`.
    void Process(const float* in, float* out, std::uint32_t frames);

    // The filter's cutoff at a muffle amount.
    [[nodiscard]] static float MuffleHz(float amount);

private:
    void ApplyCommands();
    void MoveMuffle(std::uint32_t frames);
    void UpdateFilter();

    float _rate;

    // Main thread to audio thread.
    std::atomic<float> _left{1.0f}, _right{1.0f};
    std::atomic<float> _fadeTarget{1.0f}, _fadeSeconds{0.0f};
    std::atomic<float> _muffleTarget{0.0f}, _muffleSeconds{0.0f};
    std::atomic<float> _snapVolume{1.0f}, _snapMuffle{0.0f};
    std::atomic<bool> _heldTarget{false}, _snapHeld{false};
    std::atomic<std::uint32_t> _fadeOrder{0}, _muffleOrder{0}, _snapOrder{0};

    // Audio thread to main thread.
    std::atomic<float> _volumeNow{1.0f};
    std::atomic<bool> _heldQuiet{false};

    // Audio thread only.
    std::uint32_t _fadeSeen = 0, _muffleSeen = 0, _snapSeen = 0;
    float _nowLeft = 1.0f, _nowRight = 1.0f;
    float _position = 1.0f, _positionTarget = 1.0f, _positionStep = 0.0f;
    float _fadeGain = 1.0f, _fadeFinal = 1.0f;
    float _hold = 1.0f;
    float _muffle = 0.0f, _muffleGoal = 0.0f, _muffleStep = 0.0f;
    float _wet = 0.0f; // Between the dry sound and the filter's, so that the filter can come and go without a click
    float _a1 = 0.0f, _a2 = 0.0f, _a3 = 0.0f;
    float _state1[2]{}, _state2[2]{};

public:
    AudioShaper(const AudioShaper&) = delete;
    AudioShaper& operator=(const AudioShaper&) = delete;
};
