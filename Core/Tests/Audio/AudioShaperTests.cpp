#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>
#include "Audio/Internals/AudioShaper.h"
#include "Audio/SoundSpace.h"

namespace {
constexpr int Rate = 48000;
constexpr float Tau = 2.0f * std::numbers::pi_v<float>;

std::vector<float> Dc(int frames, float value = 1.0f) { return std::vector<float>(static_cast<std::size_t>(frames) * 2, value); }
std::vector<float> Sine(float hz, int frames) {
    std::vector<float> samples(static_cast<std::size_t>(frames) * 2);
    for (int i = 0; i < frames; ++i)
        samples[static_cast<std::size_t>(i) * 2] = samples[static_cast<std::size_t>(i) * 2 + 1] =
            std::sin(Tau * hz * static_cast<float>(i) / Rate);
    return samples;
}
std::vector<float> Run(AudioShaper& shaper, const std::vector<float>& in, std::uint32_t block = 480) {
    std::vector<float> out(in.size());
    const auto frames = static_cast<std::uint32_t>(in.size() / 2);
    for (std::uint32_t at = 0; at < frames; at += block)
        shaper.Process(in.data() + at * 2, out.data() + at * 2, std::min(block, frames - at));
    return out;
}
float Rms(const std::vector<float>& stereo, std::size_t channel, std::size_t fromFrame = 0) {
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = fromFrame * 2 + channel; i < stereo.size(); i += 2, ++count)
        sum += static_cast<double>(stereo[i]) * stereo[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}
float LargestJump(const std::vector<float>& stereo, std::size_t channel) {
    float largest = 0.0f;
    for (std::size_t i = channel + 2; i < stereo.size(); i += 2)
        largest = std::max(largest, std::abs(stereo[i] - stereo[i - 2]));
    return largest;
}
} // namespace

TEST_CASE("Engine.AudioShaper.PassesThroughUntouchedByDefault", "[engine][audio]") {
    AudioShaper shaper(Rate);
    const auto in = Sine(1000, 4800);
    const auto out = Run(shaper, in);
    CHECK(out == in);
}

TEST_CASE("Engine.AudioShaper.EarsGlideInsteadOfJumping", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.Snap({.left = 1.0f, .right = 0.0f});
    auto out = Run(shaper, Dc(960));
    CHECK(out[1000] == 1.0f);
    CHECK(out[1001] == 0.0f);
    shaper.SetEars(0.0f, 1.0f); // From one side to the other in one call
    out = Run(shaper, Dc(4800));
    CHECK(LargestJump(out, 0) <= Catch::Approx(1.0f / (AudioShaper::GainGlideSeconds * Rate)).epsilon(0.01));
    CHECK(out[9000] == 0.0f);
    CHECK(out[9001] == 1.0f);
}

TEST_CASE("Engine.AudioShaper.FadeTakesItsTimeAndEndsExactly", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.FadeTo(0.0f, 1.0f);
    const auto out = Run(shaper, Dc(Rate + 480));
    // Halfway through the position is 0.5, and the gain is sin(45°) = -3 dB: the constant-power curve.
    CHECK(out[Rate] == Catch::Approx(std::cos(std::numbers::pi_v<float> / 4.0f)).margin(0.01));
    CHECK(out[Rate * 2 - 2] < 0.002f);
    CHECK(out[Rate * 2 + 400] == 0.0f);
    CHECK(shaper.Volume() == 0.0f);
}

TEST_CASE("Engine.AudioShaper.ASettledFadeEndsOnTheAskedVolume", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.FadeTo(0.37f, 0.1f);
    const auto out = Run(shaper, Dc(9600));
    CHECK(out[out.size() - 2] == 0.37f);
    CHECK(shaper.Volume() == 0.37f);
}

TEST_CASE("Engine.AudioShaper.TwoCrossingFadesKeepTheirPower", "[engine][audio]") {
    AudioShaper down(Rate), up(Rate);
    up.Snap({.volume = 0.0f});
    down.FadeTo(0.0f, 1.0f);
    up.FadeTo(1.0f, 1.0f);
    const auto a = Run(down, Dc(Rate));
    const auto b = Run(up, Dc(Rate));
    for (std::size_t i = 0; i < a.size(); i += 2) {
        const float power = a[i] * a[i] + b[i] * b[i];
        REQUIRE(power == Catch::Approx(1.0f).margin(0.002));
    }
}

TEST_CASE("Engine.AudioShaper.AFadeInterruptedLeavesFromWhereItIs", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.FadeTo(0.0f, 1.0f);
    auto out = Run(shaper, Dc(Rate / 2));
    const float reached = shaper.Volume();
    CHECK(reached == Catch::Approx(0.7071f).margin(0.01));
    shaper.FadeTo(1.0f, 0.5f);
    out = Run(shaper, Dc(4800));
    CHECK(std::abs(out[0] - reached) < 0.001f); // No jump where the new fade took over
    CHECK(LargestJump(out, 0) < 0.001f);
}

TEST_CASE("Engine.AudioShaper.SnapForgetsAFadeLeftFromTheVoicesLastLife", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.FadeTo(0.0f, 1.0f); // Never processed: the voice ended
    shaper.Snap({.volume = 0.8f});
    const auto out = Run(shaper, Dc(4800));
    CHECK(out[0] == 0.8f);
    CHECK(out[out.size() - 2] == 0.8f);
}

TEST_CASE("Engine.AudioShaper.MuffleHzFollowsTheAmount", "[engine][audio]") {
    CHECK(AudioShaper::MuffleHz(0.0f) == Catch::Approx(AudioShaper::MuffleHighestHz));
    CHECK(AudioShaper::MuffleHz(1.0f) == Catch::Approx(AudioShaper::MuffleLowestHz));
    CHECK(AudioShaper::MuffleHz(0.5f) == Catch::Approx(std::sqrt(AudioShaper::MuffleHighestHz * AudioShaper::MuffleLowestHz)));
}

TEST_CASE("Engine.AudioShaper.MuffleTakesTheHighsAndLeavesTheLows", "[engine][audio]") {
    const auto level = [](float hz, float amount) {
        AudioShaper shaper(Rate);
        shaper.Snap({.muffle = amount});
        const auto out = Run(shaper, Sine(hz, 9600));
        return 20.0f * std::log10(Rms(out, 0, 4800) / (1.0f / std::numbers::sqrt2_v<float>));
    };
    CHECK(level(100, 1.0f) == Catch::Approx(0.0f).margin(0.1));
    CHECK(level(300, 1.0f) == Catch::Approx(-3.0f).margin(0.3));
    CHECK(level(1000, 1.0f) < -18.0f);
    CHECK(level(5000, 1.0f) < -45.0f);
    CHECK(level(5000, 0.5f) == Catch::Approx(-12.5f).margin(1.5f)); // Half is a cutoff near 2.4 kHz: an octave above it
}

TEST_CASE("Engine.AudioShaper.MuffleComesAndGoesWithoutAClick", "[engine][audio]") {
    // A click is a corner. The filter's lag makes the slope change as the cutoff moves, which is the effect itself, so
    // this measures corners: a clean sine of this frequency has 0.0007, and one sample out of place would have far more.
    const auto corner = [](const std::vector<float>& stereo) {
        float largest = 0.0f;
        for (std::size_t i = 4; i < stereo.size(); i += 2)
            largest = std::max(largest, std::abs(stereo[i] - 2.0f * stereo[i - 2] + stereo[i - 4]));
        return largest;
    };
    AudioShaper shaper(Rate);
    (void)Run(shaper, Sine(200, 480)); // Whole cycles, so that the sine continues. Clear so far: the filter is off
    shaper.MuffleTo(1.0f, 0.2f);
    auto out = Run(shaper, Sine(200, 24000));
    CAPTURE(corner(out));
    CHECK(corner(out) < 0.006f);
    shaper.MuffleTo(0.0f, 0.2f);
    out = Run(shaper, Sine(200, 24000));
    CAPTURE(corner(out));
    CHECK(corner(out) < 0.006f);
    // And it is really off again: the output is the input.
    const auto in = Sine(200, 4800);
    out = Run(shaper, in);
    CHECK(out == in);
}

TEST_CASE("Engine.AudioShaper.HoldGoesQuietSoftlyAndComesBack", "[engine][audio]") {
    AudioShaper shaper(Rate);
    CHECK_FALSE(shaper.HeldQuiet());
    shaper.SetHeld(true);
    auto out = Run(shaper, Dc(960));
    CHECK(LargestJump(out, 0) <= Catch::Approx(1.0f / (AudioShaper::HoldSeconds * Rate)).epsilon(0.01));
    CHECK(out[out.size() - 2] == 0.0f);
    CHECK(shaper.HeldQuiet());
    shaper.SetHeld(false);
    CHECK_FALSE(shaper.HeldQuiet());
    out = Run(shaper, Dc(960));
    CHECK(LargestJump(out, 0) <= Catch::Approx(1.0f / (AudioShaper::HoldSeconds * Rate)).epsilon(0.01));
    CHECK(out[out.size() - 2] == 1.0f);
}

TEST_CASE("Engine.AudioShaper.BlockSizeDoesNotChangeTheResult", "[engine][audio]") {
    const auto render = [](std::uint32_t block) {
        AudioShaper shaper(Rate);
        shaper.FadeTo(0.2f, 0.05f);
        shaper.SetEars(0.3f, 0.9f);
        return Run(shaper, Sine(700, 4800), block);
    };
    const auto a = render(480), b = render(4800), c = render(4800);
    REQUIRE(a.size() == b.size());
    CHECK(b == c);
    for (std::size_t i = 0; i < a.size(); ++i)
        REQUIRE(a[i] == Catch::Approx(b[i]).margin(0.0001));
}

TEST_CASE("Engine.AudioShaper.NoInputIsSilence", "[engine][audio]") {
    AudioShaper shaper(Rate);
    std::vector<float> out(960, 1.0f);
    shaper.Process(nullptr, out.data(), 480);
    CHECK(std::ranges::all_of(out, [](float sample) { return sample == 0.0f; }));
}

TEST_CASE("Engine.AudioShaper.AFadeMovesWithNoInput", "[engine][audio]") {
    AudioShaper shaper(Rate);
    shaper.FadeTo(0.0f, 0.1f);
    std::vector<float> out(9600 * 2);
    shaper.Process(nullptr, out.data(), 9600);
    CHECK(shaper.Volume() == 0.0f);
}
