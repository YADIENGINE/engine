#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "Audio/AudioFiles.h"
#include "Audio/AudioSystem.h"
#include "Audio/Internals/AudioShaper.h"
#include "Audio/Internals/AudioSoundOutput.h"
#include "Audio/SoundLayers.h"
#include "Audio/SoundPlayer.h"
#include "Tests/Support/TempDirectory.h"

// The real engine with no device: what comes out is what a device would have been given.

namespace {
constexpr int Rate = 48000; // AudioOutput::Offline's
constexpr float Tau = 2.0f * std::numbers::pi_v<float>;
constexpr float Middle = 0.70710678f; // A sound in the middle at full volume

void WriteWav(const std::filesystem::path& path, int rate, int channels, const std::vector<float>& samples) {
    const auto put32 = [](std::ofstream& out, std::uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); };
    const auto put16 = [](std::ofstream& out, std::uint16_t value) { out.write(reinterpret_cast<const char*>(&value), 2); };
    std::ofstream out(path, std::ios::binary);
    const auto bytes = static_cast<std::uint32_t>(samples.size() * 4);
    out.write("RIFF", 4);
    put32(out, 36 + bytes);
    out.write("WAVEfmt ", 8);
    put32(out, 16);
    put16(out, 3); // IEEE float
    put16(out, static_cast<std::uint16_t>(channels));
    put32(out, static_cast<std::uint32_t>(rate));
    put32(out, static_cast<std::uint32_t>(rate * channels * 4));
    put16(out, static_cast<std::uint16_t>(channels * 4));
    put16(out, 32);
    out.write("data", 4);
    put32(out, bytes);
    out.write(reinterpret_cast<const char*>(samples.data()), bytes);
}

std::vector<float> Sine(float hz, int rate, int frames, float amplitude = 0.5f) {
    std::vector<float> samples(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i)
        samples[static_cast<std::size_t>(i)] = amplitude * std::sin(Tau * hz * static_cast<float>(i) / static_cast<float>(rate));
    return samples;
}

// One side of a stereo buffer.
std::vector<float> Side(const std::vector<float>& stereo, std::size_t channel) {
    std::vector<float> side(stereo.size() / 2);
    for (std::size_t i = 0; i < side.size(); ++i)
        side[i] = stereo[i * 2 + channel];
    return side;
}
float Rms(std::span<const float> samples) {
    double sum = 0.0;
    for (const float sample : samples)
        sum += static_cast<double>(sample) * sample;
    return samples.empty() ? 0.0f : static_cast<float>(std::sqrt(sum / static_cast<double>(samples.size())));
}
float Peak(std::span<const float> samples) {
    float peak = 0.0f;
    for (const float sample : samples)
        peak = std::max(peak, std::abs(sample));
    return peak;
}
float LargestJump(std::span<const float> samples) {
    float largest = 0.0f;
    for (std::size_t i = 1; i < samples.size(); ++i)
        largest = std::max(largest, std::abs(samples[i] - samples[i - 1]));
    return largest;
}
float CleanJump(float hz, float amplitude) { return amplitude * Tau * hz / Rate; }

// The sound files of a test, written before the catalog that names them is loaded.
struct Files {
    TempDirectory temp;
};
std::unique_ptr<AudioSystem> OpenOffline(const std::filesystem::path& root, AudioCatalog catalog) {
    auto audio = std::make_unique<AudioSystem>(root, AudioVolumeSettings{}, AudioOutput::Offline);
    audio->BeginLoading(std::move(catalog));
    while (!audio->FinishLoading())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return audio;
}
std::vector<float> Render(AudioSystem& audio, int frames, int block = 480) {
    std::vector<float> out(static_cast<std::size_t>(frames) * 2);
    for (int at = 0; at < frames; at += block) {
        const int count = std::min(block, frames - at);
        audio.Render(std::span(out).subspan(static_cast<std::size_t>(at) * 2, static_cast<std::size_t>(count) * 2));
        audio.Update();
    }
    return out;
}
constexpr SoundId ToneId{1};
constexpr SoundId OtherId{2};
constexpr MusicId TrackA{1};
constexpr MusicId TrackB{2};
} // namespace

TEST_CASE("Engine.AudioGraph.AnEffectComesOutWholeAndEnds", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "tone.wav", Rate, 1, Sine(1000, Rate, 4800));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "tone.wav"}}});
    CHECK(audio->SampleRate() == Rate);
    CHECK_FALSE(audio->IsEffectPlaying(ToneId));
    audio->PlayEffect(ToneId);
    CHECK(audio->IsEffectPlaying(ToneId));
    const auto out = Render(*audio, 9600);
    const auto left = Side(out, 0), right = Side(out, 1);
    CHECK(Peak(left) == Catch::Approx(0.5f * Middle).margin(0.005));
    CHECK(left == right);
    // Its last samples are there: a sound's end is not cut off.
    CHECK(Peak(std::span(left).subspan(4700, 95)) > 0.3f);
    CHECK(Peak(std::span(left).subspan(4900)) == 0.0f);
    CHECK_FALSE(audio->IsEffectPlaying(ToneId));
}

TEST_CASE("Engine.AudioGraph.ALoopHasNoSeamWhateverTheFilesRate", "[engine][audio]") {
    const int rate = GENERATE(48000, 44100, 22050);
    const bool streamed = GENERATE(false, true);
    CAPTURE(rate, streamed);
    Files files;
    WriteWav(files.temp.Path() / "loop.wav", rate, 1, Sine(static_cast<float>(rate) / 100.0f, rate, rate));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "loop.wav", {.streamed = streamed}}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    REQUIRE(voice != VoiceId{});
    std::vector<float> left;
    for (int i = 0; i < 100; ++i) { // Over three seams, in real time for the stream's reader
        const auto block = Side(Render(*audio, 4800), 0);
        left.insert(left.end(), block.begin(), block.end());
        if (streamed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const float clean = CleanJump(static_cast<float>(rate) / 100.0f, 0.5f * Middle);
    const std::span settled = std::span(left).subspan(4800);
    CHECK(LargestJump(settled) < clean * 1.05f);
    CHECK(Peak(settled) == Catch::Approx(0.5f * Middle).margin(0.01));
    CHECK(audio->IsVoiceAlive(voice));
}

TEST_CASE("Engine.AudioGraph.PanSplitsThePowerBetweenTheEars", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "tone.wav", Rate, 1, Sine(1000, Rate, 4800));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "tone.wav"}}});
    const float pan = GENERATE(-1.0f, -0.5f, 0.0f, 0.5f, 1.0f);
    CAPTURE(pan);
    const VoiceId voice = audio->StartVoice(ToneId, {.pan = pan, .loop = true, .owned = true});
    const auto out = Render(*audio, 4800);
    const float left = Rms(Side(out, 0)), right = Rms(Side(out, 1));
    const float source = 0.5f / std::numbers::sqrt2_v<float>;
    const SoundEars ears = PanEars(pan);
    CHECK(left == Catch::Approx(source * Middle * ears.left).margin(0.002));
    CHECK(right == Catch::Approx(source * Middle * ears.right).margin(0.002));
    // Constant power: the same total wherever the sound is, as loud as a middle one.
    CHECK(left * left + right * right == Catch::Approx(2.0f * source * source * Middle * Middle).margin(0.001));
    audio->StopVoice(voice, 0.0f);
}

TEST_CASE("Engine.AudioGraph.AVoiceMovesAcrossInOneCallWithoutAClick", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.pan = -1.0f, .loop = true, .owned = true});
    (void)Render(*audio, 4800);
    audio->SetVoicePlacement(voice, 1.0f, 1.0f);
    const auto out = Render(*audio, 4800);
    const auto left = Side(out, 0), right = Side(out, 1);
    const float limit = std::numbers::sqrt2_v<float> * 0.5f * Middle / (AudioShaper::GainGlideSeconds * Rate);
    CHECK(LargestJump(left) <= limit * 1.01f);
    CHECK(LargestJump(right) <= limit * 1.01f);
    CHECK(left.back() == Catch::Approx(0.0f).margin(0.0001));
    CHECK(right.back() == Catch::Approx(0.5f).margin(0.001));
}

TEST_CASE("Engine.AudioGraph.AFadeNeedsNothingFromTheMainThread", "[engine][audio]") {
    // The whole fade happens during Render alone: no Update, no call, as during a long frame or a seek.
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 480);
    audio->FadeVoice(voice, 0.0f, 0.5f);
    std::vector<float> out(static_cast<std::size_t>(Rate) * 2);
    for (std::size_t at = 0; at < out.size(); at += 960)
        audio->Render(std::span(out).subspan(at, 960));
    const auto left = Side(out, 0);
    CHECK(left[Rate / 4] == Catch::Approx(0.5f * Middle * 0.7071f).margin(0.005));
    CHECK(left[Rate * 3 / 4] == 0.0f);
    CHECK(audio->IsVoiceAlive(voice)); // Silent, not ended: only a stop ends it
}

TEST_CASE("Engine.AudioGraph.StoppingFadesThenFreesTheVoice", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 960);
    audio->StopVoice(voice, 0.1f);
    CHECK_FALSE(audio->IsVoiceAlive(voice));
    const auto out = Render(*audio, 9600);
    CHECK(LargestJump(Side(out, 0)) < 0.002f);
    CHECK(Side(out, 0).back() == 0.0f);
    // The stale ID is safe.
    audio->SetVoicePitch(voice, 2.0f);
    audio->StopVoice(voice, 0.0f);
    CHECK_FALSE(audio->IsVoiceAlive(voice));
}

TEST_CASE("Engine.AudioGraph.CopiesAndOverflowKeepTheirMeaning", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(Rate, 0.1f));
    auto audio = OpenOffline(files.temp.Path(),
                             {.effects = {{ToneId, "flat.wav", {.maxConcurrent = 2}},
                                          {OtherId, "flat.wav", {.maxConcurrent = 1, .overflow = EffectOverflow::Skip}}}});
    const float one = 0.1f * Middle;
    audio->PlayEffect(ToneId);
    audio->PlayEffect(ToneId);
    CHECK(Side(Render(*audio, 960), 0).back() == Catch::Approx(2.0f * one).margin(0.001));
    audio->PlayEffect(ToneId); // A third restarts the oldest: still two, and no click
    const auto left = Side(Render(*audio, 4800), 0);
    CHECK(left.back() == Catch::Approx(2.0f * one).margin(0.001));
    CHECK(LargestJump(left) < one * 1.05f); // The new one's own start; the oldest one left softly under it
    audio->StopEffect(ToneId);
    CHECK_FALSE(audio->IsEffectPlaying(ToneId));
    (void)Render(*audio, 960);

    audio->PlayEffect(OtherId);
    audio->PlayEffect(OtherId); // Dropped
    CHECK(Side(Render(*audio, 960), 0).back() == Catch::Approx(one).margin(0.001));
    CHECK(audio->StartVoice(OtherId, {}) == VoiceId{});
    // A voice held by a handle is outside the count.
    CHECK(audio->StartVoice(OtherId, {.owned = true}) != VoiceId{});
}

TEST_CASE("Engine.AudioGraph.VolumeOptionsAreWhereTheyWere", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(),
                             {.effects = {{ToneId, "flat.wav", {.volume = 0.5f}}, {OtherId, "flat.wav", {.bus = SoundBus::Ui}}}});
    const VoiceId world = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    const auto level = [&] { return Side(Render(*audio, 4800), 0).back(); };
    CHECK(level() == Catch::Approx(0.25f * Middle).margin(0.001));
    audio->SetEffectVolume(ToneId, 1.0f);
    CHECK(level() == Catch::Approx(0.5f * Middle).margin(0.001));
    audio->SetEffectVolume(0.5f);
    CHECK(level() == Catch::Approx(0.25f * Middle).margin(0.001));
    audio->SetMasterVolume(0.5f);
    CHECK(level() == Catch::Approx(0.125f * Middle).margin(0.001));
    audio->SetMuted(true);
    CHECK(level() == 0.0f);
    audio->SetMuted(false);
    audio->SetMasterVolume(1.0f);
    audio->SetEffectVolume(1.0f);
    CHECK(level() == Catch::Approx(0.5f * Middle).margin(0.001));
    audio->StopVoice(world, 0.0f);
}

TEST_CASE("Engine.AudioGraph.ABusFadesAndMufflesWhatGoesThroughIt", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    WriteWav(files.temp.Path() / "high.wav", Rate, 1, Sine(4800, Rate, 4800));
    auto audio = OpenOffline(files.temp.Path(),
                             {.effects = {{ToneId, "flat.wav"}, {OtherId, "flat.wav", {.bus = SoundBus::Ui}},
                                          {SoundId{3}, "high.wav"}}});
    (void)audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)audio->StartVoice(OtherId, {.loop = true, .owned = true});
    CHECK(Side(Render(*audio, 480), 0).back() == Catch::Approx(1.0f * Middle).margin(0.001));
    audio->FadeBus(SoundBus::World, 0.0f, 0.1f);
    CHECK(Side(Render(*audio, 9600), 0).back() == Catch::Approx(0.5f * Middle).margin(0.001)); // Ui alone remains
    audio->FadeBus(SoundBus::World, 1.0f, 0.0f);
    CHECK(Side(Render(*audio, 1920), 0).back() == Catch::Approx(1.0f * Middle).margin(0.001));
    audio->StopEffect(OtherId);
    audio->StopEffect(ToneId);
    (void)Render(*audio, 960);
    (void)audio->StartVoice(SoundId{3}, {.loop = true, .owned = true});
    const float clear = Rms(Side(Render(*audio, 4800), 0));
    audio->MuffleBus(SoundBus::World, 1.0f, 0.0f);
    (void)Render(*audio, 4800);
    const float muffled = Rms(Side(Render(*audio, 4800), 0));
    CHECK(20.0f * std::log10(muffled / clear) < -40.0f);
}

TEST_CASE("Engine.AudioGraph.ABusKeepsFadingWithNothingPlaying", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    audio->FadeBus(SoundBus::World, 0.0f, 0.1f);
    (void)Render(*audio, 9600);
    (void)audio->StartVoice(ToneId, {.loop = true, .owned = true});
    CHECK(Peak(Side(Render(*audio, 4800), 0)) == 0.0f);
}

TEST_CASE("Engine.AudioGraph.MuffleOnAVoiceLeavesTheOthersClear", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "high.wav", Rate, 1, Sine(4800, Rate, 4800));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "high.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.muffle = 1.0f, .loop = true, .owned = true});
    (void)Render(*audio, 4800);
    const float muffled = Rms(Side(Render(*audio, 4800), 0));
    audio->MuffleVoice(voice, 0.0f, 0.0f);
    (void)Render(*audio, 4800);
    const float clear = Rms(Side(Render(*audio, 4800), 0));
    CHECK(clear == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.01));
    CHECK(20.0f * std::log10(muffled / clear) < -40.0f);
}

TEST_CASE("Engine.AudioGraph.PitchChangesTheFrequencyLiveWithoutAClick", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "tone.wav", Rate, 1, Sine(1000, Rate, 4800));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "tone.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    std::vector<float> left;
    float pitch = 1.0f;
    for (int i = 0; i < 100; ++i) {
        audio->SetVoicePitch(voice, pitch);
        const auto block = Side(Render(*audio, 480), 0);
        left.insert(left.end(), block.begin(), block.end());
        pitch += 0.01f;
    }
    CHECK(LargestJump(std::span(left).subspan(480)) < CleanJump(2000, 0.5f * Middle) * 1.05f);
    int rising = 0;
    for (std::size_t i = left.size() - 4800; i < left.size(); ++i)
        if (left[i - 1] < 0.0f && left[i] >= 0.0f) ++rising;
    CHECK(rising * 10 == Catch::Approx(1950).margin(60)); // The last tenth of a second: pitch from 1.9 to 2
}

TEST_CASE("Engine.AudioGraph.StartAtBeginsPartWayAndWrapsAround", "[engine][audio]") {
    Files files;
    std::vector<float> ramp(4800);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float>(i) / 4800.0f;
    WriteWav(files.temp.Path() / "ramp.wav", Rate, 1, ramp);
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "ramp.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.startAt = 0.05f, .loop = true, .owned = true});
    CHECK(Side(Render(*audio, 480), 0)[100] / Middle == Catch::Approx(0.5f + 100.0f / 4800.0f).margin(0.01));
    audio->StopVoice(voice, 0.0f);
    (void)Render(*audio, 960);
    (void)audio->StartVoice(ToneId, {.startAt = 0.15f, .loop = true, .owned = true}); // Past the end of 0.1 s
    CHECK(Side(Render(*audio, 480), 0)[100] / Middle == Catch::Approx(0.5f + 100.0f / 4800.0f).margin(0.01));
}

TEST_CASE("Engine.AudioGraph.APausedVoiceHoldsItsPlace", "[engine][audio]") {
    Files files;
    std::vector<float> ramp(Rate);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float>(i) / Rate;
    WriteWav(files.temp.Path() / "ramp.wav", Rate, 1, ramp);
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "ramp.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 4800);
    audio->PauseVoice(voice, true);
    const auto held = Side(Render(*audio, 4800), 0);
    CHECK(LargestJump(held) < 0.05f); // Softly
    CHECK(held.back() == 0.0f);
    CHECK(audio->IsVoiceAlive(voice));
    const auto silent = Side(Render(*audio, 48000), 0);
    CHECK(Peak(silent) == 0.0f);
    audio->PauseVoice(voice, false);
    const auto left = Side(Render(*audio, 4800), 0);
    // A second of pause cost it nothing: it is still near the tenth of a second where it stopped.
    CHECK(left.back() / Middle == Catch::Approx(0.2f).margin(0.03));
}

TEST_CASE("Engine.AudioGraph.AVoiceStartedPausedMakesNoSound", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true, .paused = true});
    CHECK(Peak(Side(Render(*audio, 4800), 0)) == 0.0f);
    audio->PauseVoice(voice, false);
    CHECK(Side(Render(*audio, 4800), 0).back() == Catch::Approx(0.5f * Middle).margin(0.001));
}

TEST_CASE("Engine.AudioGraph.VoicesGivenOneFrameStartInStep", "[engine][audio]") {
    Files files;
    const auto wave = Sine(441, 44100, 44100);
    auto inverted = wave;
    for (float& sample : inverted)
        sample = -sample;
    WriteWav(files.temp.Path() / "a.wav", 44100, 1, wave);
    WriteWav(files.temp.Path() / "b.wav", 44100, 1, inverted);
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "a.wav"}, {OtherId, "b.wav"}}});
    const std::uint64_t frame = audio->ClockFrames() + 2048;
    (void)audio->StartVoice(ToneId, {.startFrame = frame, .loop = true, .owned = true});
    // Started a block later on the clock, but told the same frame: they cancel to nothing, sample by sample.
    (void)Render(*audio, 480);
    (void)audio->StartVoice(OtherId, {.startFrame = frame, .loop = true, .owned = true});
    CHECK(Peak(Side(Render(*audio, Rate * 2), 0)) < 0.0005f);
}

TEST_CASE("Engine.AudioGraph.SoundsOfALongFileAreStreamed", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "long.wav", Rate, 2, std::vector<float>(static_cast<std::size_t>(Rate) * 2 * 30, 0.25f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "long.wav", {.streamed = true}}}});
    (void)audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 4800);
    CHECK(Side(Render(*audio, 4800), 0).back() == Catch::Approx(0.25f * Middle).margin(0.001));
}

TEST_CASE("Engine.AudioGraph.MusicPlaysSeeksAndEnds", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "track.wav", Rate, 1, std::vector<float>(Rate, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.music = {{TrackA, "track.wav"}}});
    CHECK_FALSE(audio->CurrentMusic());
    CHECK(audio->MusicLength() == 0.0f);
    audio->PlayMusic(TrackA, {.loop = false});
    CHECK(audio->CurrentMusic() == TrackA);
    CHECK(audio->IsMusicPlaying());
    CHECK(audio->MusicLength() == Catch::Approx(1.0f));
    CHECK(Side(Render(*audio, 4800), 0).back() == Catch::Approx(0.5f * Middle).margin(0.001));
    CHECK(audio->MusicPosition() == Catch::Approx(0.1f).margin(0.02));
    audio->SeekMusic(0.5f);
    (void)Render(*audio, 480);
    CHECK(audio->MusicPosition() == Catch::Approx(0.51f).margin(0.02));
    audio->PauseMusic();
    (void)Render(*audio, 4800);
    CHECK_FALSE(audio->IsMusicPlaying());
    CHECK(audio->CurrentMusic() == TrackA);
    const float at = audio->MusicPosition();
    (void)Render(*audio, 4800);
    CHECK(audio->MusicPosition() == Catch::Approx(at).margin(0.001));
    audio->ResumeMusic();
    (void)Render(*audio, 480);
    CHECK(audio->IsMusicPlaying());
    (void)Render(*audio, Rate);
    CHECK_FALSE(audio->IsMusicPlaying()); // Ended by itself; the selection stays
    CHECK(audio->CurrentMusic() == TrackA);
    audio->PlayMusic(TrackA, {.loop = true}); // And it can start again
    CHECK(Side(Render(*audio, 4800), 0).back() == Catch::Approx(0.5f * Middle).margin(0.001));
    audio->StopMusic();
    CHECK_FALSE(audio->CurrentMusic());
    (void)Render(*audio, 960);
    CHECK(Side(Render(*audio, 480), 0).back() == 0.0f);
}

TEST_CASE("Engine.AudioGraph.MusicVolumeGroupsAreSeparate", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "track.wav", Rate, 1, std::vector<float>(Rate, 0.5f));
    auto audio = OpenOffline(files.temp.Path(),
                             {.music = {{TrackA, "track.wav"}, {TrackB, "track.wav", MusicVolumeGroup::Menu}}});
    audio->SetMusicVolume(0.5f);
    audio->SetMusicVolume(0.25f, MusicVolumeGroup::Menu);
    audio->PlayMusic(TrackA);
    CHECK(Side(Render(*audio, 1920), 0).back() == Catch::Approx(0.25f * Middle).margin(0.001));
    audio->PlayMusic(TrackB);
    CHECK(Side(Render(*audio, 1920), 0).back() == Catch::Approx(0.125f * Middle).margin(0.001));
    // Under a bus's level as well, whichever group it is.
    audio->FadeBus(SoundBus::Music, 0.0f, 0.0f);
    CHECK(Side(Render(*audio, 1920), 0).back() == 0.0f);
}

TEST_CASE("Engine.AudioGraph.TwoTracksCrossAtConstantPower", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "a.wav", Rate, 1, Sine(400, Rate, Rate));
    WriteWav(files.temp.Path() / "b.wav", Rate, 1, Sine(770, Rate, Rate));
    auto audio = OpenOffline(files.temp.Path(), {.music = {{TrackA, "a.wav"}, {TrackB, "b.wav"}}});
    audio->PlayMusic(TrackA);
    const float alone = Rms(Side(Render(*audio, 9600), 0));
    audio->PlayMusic(TrackB, {.fadeSeconds = 1.0f});
    CHECK(audio->CurrentMusic() == TrackB);
    const auto cross = Side(Render(*audio, Rate), 0);
    for (int part = 0; part < 10; ++part) {
        const auto piece = std::span(cross).subspan(static_cast<std::size_t>(part) * 4800, 4800);
        CAPTURE(part);
        CHECK(20.0f * std::log10(Rms(piece) / alone) == Catch::Approx(0.0f).margin(0.5));
    }
    const float after = Rms(Side(Render(*audio, 9600), 0));
    CHECK(after == Catch::Approx(alone).margin(0.005)); // Only one is left, and it is a different frequency
}

TEST_CASE("Engine.AudioGraph.OtherFormatsDecodeLoopAndStream", "[engine][audio]") {
    const char* name = GENERATE("tone.ogg", "tone.mp3", "tone.flac");
    CAPTURE(name);
    const std::filesystem::path root = ENGINE_AUDIO_TEST_FILES;
    const DecodedAudio decoded = DecodeAudioFile(root / name);
    CHECK(decoded.sampleRate == 44100);
    CHECK(decoded.channels == 1);
    CHECK(static_cast<int>(decoded.Frames()) >= 44100);
    CHECK(Peak(decoded.samples) == Catch::Approx(0.5f).margin(0.05));

    auto audio = OpenOffline(root, {.effects = {{ToneId, name}}, .music = {{TrackA, name}}});
    (void)audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 4800);
    CHECK(Rms(Side(Render(*audio, 4800), 0)) == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.02));
    audio->StopEffect(ToneId);
    (void)Render(*audio, 960);
    audio->PlayMusic(TrackA);
    CHECK(audio->MusicLength() == Catch::Approx(1.0f).margin(0.1));
    (void)Render(*audio, 9600);
    CHECK(audio->IsMusicPlaying());
    CHECK(audio->MusicPosition() == Catch::Approx(0.2f).margin(0.05));
    CHECK(Rms(Side(Render(*audio, 4800), 0)) == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.02));
}

TEST_CASE("Engine.AudioGraph.FlacAndVorbisLoopWithoutASeam", "[engine][audio]") {
    const char* name = GENERATE("tone.flac", "tone.ogg");
    const bool streamed = GENERATE(false, true);
    CAPTURE(name, streamed);
    auto audio = OpenOffline(ENGINE_AUDIO_TEST_FILES, {.effects = {{ToneId, name, {.streamed = streamed}}}});
    (void)audio->StartVoice(ToneId, {.loop = true, .owned = true});
    std::vector<float> left;
    for (int i = 0; i < 40; ++i) { // Over three seams
        const auto block = Side(Render(*audio, 4800), 0);
        left.insert(left.end(), block.begin(), block.end());
        if (streamed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const float clean = CleanJump(441.0f, 0.5f * Middle);
    CHECK(LargestJump(std::span(left).subspan(4800)) < clean * 1.15f); // Vorbis is lossy, so a little above a pure sine
}

TEST_CASE("Engine.AudioGraph.DecodingReportsBadFiles", "[engine][audio]") {
    Files files;
    CHECK_THROWS_WITH(DecodeAudioFile(files.temp.Path() / "missing.wav"), Catch::Matchers::ContainsSubstring("not found"));
    std::ofstream(files.temp.Path() / "bad.wav") << "not a sound";
    CHECK_THROWS_WITH(DecodeAudioFile(files.temp.Path() / "bad.wav"), Catch::Matchers::ContainsSubstring("Could not decode"));
}

TEST_CASE("Engine.AudioGraph.APathWithAnyLettersLoads", "[engine][audio]") {
    Files files;
    const auto folder = files.temp.Path() / std::filesystem::path(u8"Sesler üğşサウンド");
    std::filesystem::create_directories(folder);
    WriteWav(folder / "tone.wav", Rate, 1, Sine(1000, Rate, 4800));
    CHECK(DecodeAudioFile(folder / "tone.wav").Frames() == 4800);
    auto audio = OpenOffline(folder, {.effects = {{ToneId, "tone.wav"}}});
    audio->PlayEffect(ToneId);
    CHECK(Peak(Side(Render(*audio, 480), 0)) > 0.1f);
}

TEST_CASE("Engine.AudioGraph.LoadingFailureIsKeptAndRethrown", "[engine][audio]") {
    Files files;
    AudioSystem audio(files.temp.Path(), {}, AudioOutput::Offline);
    audio.BeginLoading({.effects = {{ToneId, "missing.wav"}}});
    for (int i = 0; i < 5000; ++i) {
        try {
            if (audio.FinishLoading()) break;
        } catch (const std::runtime_error&) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK_THROWS_WITH(audio.FinishLoading(), Catch::Matchers::ContainsSubstring("missing.wav"));
    CHECK_THROWS_WITH(audio.PlayEffect(ToneId), Catch::Matchers::ContainsSubstring("missing.wav"));
    CHECK_FALSE(audio.HasEffect(ToneId));
}

TEST_CASE("Engine.AudioGraph.ResourcesLoadAndUnloadWhileRunning", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {});
    CHECK_THROWS_AS(audio->PlayEffect(ToneId), std::out_of_range);
    audio->LoadEffect(ToneId, "flat.wav");
    CHECK(audio->HasEffect(ToneId));
    const VoiceId voice = audio->StartVoice(ToneId, {.loop = true, .owned = true});
    (void)Render(*audio, 960);
    // A failed replacement leaves the old one playing.
    CHECK_THROWS(audio->LoadEffect(ToneId, "missing.wav"));
    CHECK(audio->IsVoiceAlive(voice));
    audio->UnloadEffect(ToneId);
    CHECK_FALSE(audio->HasEffect(ToneId));
    CHECK_FALSE(audio->IsVoiceAlive(voice));
    audio->SetVoicePitch(voice, 2.0f); // Safe: the voice is gone with its effect
    CHECK(Side(Render(*audio, 960), 0).back() == 0.0f);

    CHECK_THROWS_AS(audio->PlayMusic(TrackA), std::out_of_range);
    audio->LoadMusic(TrackA, "flat.wav");
    audio->PlayMusic(TrackA);
    (void)Render(*audio, 480);
    audio->UnloadMusic(TrackA);
    CHECK_FALSE(audio->CurrentMusic());
    CHECK_FALSE(audio->HasMusic(TrackA));
}

TEST_CASE("Engine.AudioGraph.NothingPlaysBeforeLoading", "[engine][audio]") {
    Files files;
    AudioSystem audio(files.temp.Path(), {}, AudioOutput::Offline);
    CHECK_THROWS_AS(audio.PlayEffect(ToneId), std::logic_error);
    CHECK_THROWS_AS((void)audio.StartVoice(ToneId, {}), std::logic_error);
    CHECK_THROWS_AS(audio.FadeBus(SoundBus::World, 0.0f, 0.0f), std::logic_error);
    CHECK_FALSE(audio.IsVoiceAlive(VoiceId{1}));
    CHECK(audio.ClockFrames() == 0);
    audio.Update();
    audio.SetEffectVolume(0.5f); // Volumes may come early
    CHECK(audio.EffectVolume() == 0.5f);
}

TEST_CASE("Engine.AudioGraph.BadValuesThrow", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(480, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    const float nan = std::nanf("");
    CHECK_THROWS_AS((void)audio->StartVoice(ToneId, {.volume = 2.0f}), std::invalid_argument);
    CHECK_THROWS_AS((void)audio->StartVoice(ToneId, {.pitch = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS((void)audio->StartVoice(ToneId, {.pan = nan}), std::invalid_argument);
    CHECK_THROWS_AS((void)audio->StartVoice(ToneId, {.muffle = -0.1f}), std::invalid_argument);
    CHECK_THROWS_AS((void)audio->StartVoice(ToneId, {.fadeIn = -1.0f}), std::invalid_argument);
    const VoiceId voice = audio->StartVoice(ToneId, {.owned = true});
    CHECK_THROWS_AS(audio->SetVoicePlacement(voice, 2.0f, 0.0f), std::invalid_argument);
    CHECK_THROWS_AS(audio->FadeVoice(voice, 0.5f, nan), std::invalid_argument);
    CHECK_THROWS_AS(audio->MuffleVoice(voice, 1.5f, 0.0f), std::invalid_argument);
    CHECK_THROWS_AS(audio->FadeBus(static_cast<SoundBus>(7), 0.0f, 0.0f), std::invalid_argument);
    std::array<float, 3> odd{};
    CHECK_THROWS_AS(audio->Render(odd), std::invalid_argument);
}

TEST_CASE("Engine.AudioGraph.OnlyOfflineCanRender", "[engine][audio]") {
    // A device may not exist here, so the refusal is asked of the system before loading.
    Files files;
    AudioSystem audio(files.temp.Path());
    std::array<float, 2> frame{};
    CHECK_THROWS_AS(audio.Render(frame), std::logic_error);
}

TEST_CASE("Engine.AudioGraph.ACaptureWritesWhatWasPlayed", "[engine][audio]") {
    Files files;
    WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 0.5f));
    auto audio = OpenOffline(files.temp.Path(), {.effects = {{ToneId, "flat.wav"}}});
    (void)audio->StartVoice(ToneId, {.pan = 1.0f, .loop = true, .owned = true});
    const auto wav = files.temp.Path() / "capture.wav";
    audio->StartCapture(wav, 0.1f);
    CHECK(audio->IsCapturing());
    (void)Render(*audio, 9600);
    CHECK_FALSE(audio->IsCapturing());
    const DecodedAudio captured = DecodeAudioFile(wav);
    CHECK(captured.sampleRate == Rate);
    CHECK(captured.channels == 2);
    CHECK(captured.Frames() == 4800);
    CHECK(captured.samples[captured.samples.size() - 2] == Catch::Approx(0.0f).margin(0.0001));
    CHECK(captured.samples[captured.samples.size() - 1] == Catch::Approx(0.5f).margin(0.001));
    CHECK_THROWS_AS(audio->StartCapture(files.temp.Path() / "no" / "such" / "folder.wav", 1.0f), std::runtime_error);
    CHECK_THROWS_AS(audio->StartCapture(wav, 0.0f), std::invalid_argument);
}

// The whole way, through the player and its handles, with a table.
namespace {
enum class Live { Tone, Flat, High, Count };
inline constexpr std::array LiveTable{
    SoundRow{Live::Tone, {"tone.wav"}, {.loop = true}},
    SoundRow{Live::Flat, {"flat.wav"}, {.volume = 0.5f, .loop = true, .falloff = {.fullWithin = 100.0f, .silentBeyond = 1100.0f, .curve = Curve::Linear}}},
    SoundRow{Live::High, {"high.wav"}, {.loop = true, .bus = SoundBus::Music}},
};
static_assert(RowsInIdOrder(LiveTable));

struct Bench {
    Bench() {
        WriteWav(files.temp.Path() / "tone.wav", Rate, 1, Sine(400, Rate, Rate));
        WriteWav(files.temp.Path() / "flat.wav", Rate, 1, std::vector<float>(4800, 1.0f));
        WriteWav(files.temp.Path() / "high.wav", Rate, 1, Sine(770, Rate, Rate));
        audio = OpenOffline(files.temp.Path(), CatalogWithSounds({}, tables));
        output = std::make_unique<AudioSoundOutput>(*audio);
        player = std::make_unique<SoundPlayer>(tables, *output);
    }
    std::vector<float> Render(int frames) {
        auto out = ::Render(*audio, frames);
        player->Update(static_cast<float>(frames) / Rate);
        return out;
    }
    Files files;
    SoundTables tables = LiveTable;
    std::unique_ptr<AudioSystem> audio;
    std::unique_ptr<AudioSoundOutput> output;
    std::unique_ptr<SoundPlayer> player;
};
} // namespace

TEST_CASE("Engine.LiveSound.PositionPansAndFadesWithDistance", "[engine][audio]") {
    Bench bench;
    bench.player->SetListener({.position = {1000.0f, 0.0f}, .panWidth = 500.0f, .maxPan = 1.0f});
    LiveSound sound = bench.player->Start(Live::Flat, {.at = Float2{1000.0f, 0.0f}});
    auto out = bench.Render(4800);
    const float rowLevel = 0.5f * Middle;
    CHECK(out[out.size() - 2] == Catch::Approx(rowLevel).margin(0.001)); // On the listener: both ears
    CHECK(out[out.size() - 1] == Catch::Approx(rowLevel).margin(0.001));

    sound.SetPosition({500.0f, 0.0f}); // To the left, at the pan's whole width
    out = bench.Render(4800);
    const float falloff = (1100.0f - 500.0f) / 1000.0f;
    CHECK(out[out.size() - 2] == Catch::Approx(rowLevel * falloff * std::numbers::sqrt2_v<float>).margin(0.002));
    CHECK(out[out.size() - 1] == Catch::Approx(0.0f).margin(0.001));

    // Moving the listener moves the sound, with no call on the handle.
    bench.player->SetListener({.position = {0.0f, 0.0f}, .panWidth = 500.0f, .maxPan = 1.0f});
    out = bench.Render(4800);
    CHECK(out[out.size() - 2] == Catch::Approx(0.0f).margin(0.002));
    CHECK(out[out.size() - 1] > 0.1f);

    sound.SetPosition({5000.0f, 0.0f}); // Beyond hearing
    out = bench.Render(4800);
    CHECK(Peak(std::span(out).subspan(4800)) == 0.0f);
    CHECK(sound.Playing()); // Still going, for when it comes back

    sound.SetPan(-1.0f); // Out of the world again: no falloff
    out = bench.Render(4800);
    CHECK(out[out.size() - 2] == Catch::Approx(rowLevel * std::numbers::sqrt2_v<float>).margin(0.002));
}

TEST_CASE("Engine.LiveSound.GoingAwayFadesItOut", "[engine][audio]") {
    Bench bench;
    {
        LiveSound sound = bench.player->Start(Live::Flat);
        CHECK(sound.Playing());
        (void)bench.Render(4800);
    }
    const auto out = Side(bench.Render(9600), 0);
    CHECK(LargestJump(out) < 0.02f);
    CHECK(out.back() == 0.0f);
    CHECK_FALSE(bench.audio->IsEffectPlaying(static_cast<SoundId>(SoundTables::FirstGeneratedId + 1)));
}

TEST_CASE("Engine.LiveSound.AHeartbeatSpeedsUpAndGetsLouder", "[engine][audio]") {
    Bench bench;
    LiveSound beat = bench.player->Start(Live::Tone, {.volume = 0.1f});
    float previous = Rms(Side(bench.Render(4800), 0));
    for (float closeness : {0.25f, 0.5f, 0.75f, 1.0f}) { // As a view would, from the distance to something
        beat.SetVolume(0.1f + 0.9f * closeness);
        beat.SetPitch(1.0f + closeness);
        (void)bench.Render(2400);
        const auto left = Side(bench.Render(4800), 0);
        CHECK(Rms(left) > previous);
        previous = Rms(left);
        int rising = 0;
        for (std::size_t i = 1; i < left.size(); ++i)
            if (left[i - 1] < 0.0f && left[i] >= 0.0f) ++rising;
        CHECK(rising * 10 == Catch::Approx(400.0f * (1.0f + closeness)).margin(20));
    }
}

TEST_CASE("Engine.LiveSound.ThePlayerPausesABusAndLetsItGo", "[engine][audio]") {
    Bench bench;
    LiveSound world = bench.player->Start(Live::Flat);
    LiveSound music = bench.player->Start(Live::High);
    (void)bench.Render(4800);
    bench.player->SetBusPaused(SoundBus::World, true);
    (void)bench.Render(4800);
    // The music plays on by itself: it is what is left.
    auto left = Side(bench.Render(4800), 0);
    CHECK(Rms(left) == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.01));
    CHECK(world.Playing());
    LiveSound late = bench.player->Start(Live::Flat); // Begun while paused: waits as well
    left = Side(bench.Render(4800), 0);
    CHECK(Rms(left) == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.01));
    bench.player->SetBusPaused(SoundBus::World, false);
    (void)bench.Render(960);
    const auto both = Side(bench.Render(4800), 0);
    CHECK(Peak(both) > 0.5f * Middle + 0.4f * Middle); // Flat twice at 0.5 and the tone
}

TEST_CASE("Engine.SoundLayers.CrossFadeHoldsTheLevelAndSilenceIsSilent", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(*bench.player, 0.5f);
    layers.Set(Live::Tone, 1.0f, 0.0f);
    (void)bench.Render(4800);
    const float alone = Rms(Side(bench.Render(4800), 0));
    CHECK(alone == Catch::Approx(0.5f / std::numbers::sqrt2_v<float> * Middle).margin(0.01));

    layers.Mix({{Live::High, 1.0f}}); // Tone out, the other in, over the default half second
    const auto cross = Side(bench.Render(24000), 0);
    for (int part = 0; part < 5; ++part) {
        CAPTURE(part);
        CHECK(20.0f * std::log10(Rms(std::span(cross).subspan(static_cast<std::size_t>(part) * 4800, 4800)) / alone) ==
              Catch::Approx(0.0f).margin(0.6));
    }
    CHECK(layers.Volume(Live::Tone) == 0.0f);
    CHECK(layers.Volume(Live::High) == 1.0f);
    (void)bench.Render(4800);

    layers.Silence(0.1f);
    (void)bench.Render(9600);
    CHECK(Peak(Side(bench.Render(4800), 0)) == 0.0f);
    CHECK(layers.Silenced());
    CHECK(layers.Volume(Live::High) == 1.0f); // It remembers
    layers.Restore(0.1f);
    (void)bench.Render(9600);
    CHECK(Rms(Side(bench.Render(4800), 0)) == Catch::Approx(alone).margin(0.01));

    layers.Stop(0.1f);
    (void)bench.Render(9600);
    CHECK(Peak(Side(bench.Render(480), 0)) == 0.0f);
    CHECK(layers.Volume(Live::High) == 0.0f);
}

TEST_CASE("Engine.SoundLayers.PreparedLayersStayInStep", "[engine][audio]") {
    enum class Stem { Up, Down, Count };
    static constexpr std::array StemTable{SoundRow{Stem::Up, {"up.wav"}, {.loop = true}},
                                          SoundRow{Stem::Down, {"down.wav"}, {.loop = true}}};
    Files files;
    const auto wave = Sine(441, 44100, 44100);
    auto inverted = wave;
    for (float& sample : inverted)
        sample = -sample;
    WriteWav(files.temp.Path() / "up.wav", 44100, 1, wave);
    WriteWav(files.temp.Path() / "down.wav", 44100, 1, inverted);
    const SoundTables tables = StemTable;
    auto audio = OpenOffline(files.temp.Path(), CatalogWithSounds({}, tables));
    AudioSoundOutput output(*audio);
    SoundPlayer player(tables, output);
    SoundLayers layers(player);
    layers.Prepare({Stem::Up, Stem::Down});
    layers.Set(Stem::Up, 1.0f, 0.0f);
    (void)Render(*audio, 9600);
    CHECK(Rms(Side(Render(*audio, 4800), 0)) > 0.2f); // One alone is heard
    layers.Set(Stem::Down, 1.0f, 0.0f);
    (void)Render(*audio, 1920);
    CHECK(Peak(Side(Render(*audio, 48000), 0)) < 0.0005f); // Both at once cancel, so they are sample-aligned
    CHECK_THROWS_AS(layers.Prepare({Stem::Up}), std::logic_error);
}
