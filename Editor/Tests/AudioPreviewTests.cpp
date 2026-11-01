#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Editor/Internals/Session/AudioAnalysis.h"
#include "Editor/Internals/Session/AudioPreview.h"
#include "Editor/Internals/Session/AudioUsage.h"
#include "Editor/Internals/Session/AudioWaveformCache.h"
#include "Tests/Support/TempDirectory.h"

namespace {
void Write(const std::filesystem::path& file, const std::string& text = "x") {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}
// Polls until the background decode settles; a hung decoder fails the test instead of blocking it.
WaveformEntry Settled(AudioWaveformCache& cache, const std::filesystem::path& file) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    auto entry = cache.Request(file);
    while (entry.state == WaveformState::Decoding && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        entry = cache.Request(file);
    }
    return entry;
}
} // namespace

TEST_CASE("Editor.AudioPreview.NeighborsAreTheFolderSoundsByName", "[engine-editor]") {
    TempDirectory temp;
    const auto folder = temp.Path() / "Audio";
    Write(folder / "b.ogg");
    Write(folder / "a.wav");
    Write(folder / "c.MP3");
    Write(folder / "notes.txt");
    Write(folder / "Nested" / "d.ogg");

    const auto sounds = SoundsBeside(folder / "b.ogg");
    REQUIRE(sounds.size() == 3);
    CHECK(sounds[0].filename() == "a.wav");
    CHECK(sounds[2].filename() == "c.MP3");
    CHECK(NeighborSound(folder / "b.ogg", -1) == folder / "a.wav");
    CHECK(NeighborSound(folder / "b.ogg", 1) == folder / "c.MP3");
    CHECK_FALSE(NeighborSound(folder / "a.wav", -1));
    CHECK_FALSE(NeighborSound(folder / "c.MP3", 1));
    CHECK_FALSE(NeighborSound(folder / "gone.ogg", 1));
}

TEST_CASE("Editor.AudioPreview.FormatsTimesAndSizes", "[engine-editor]") {
    CHECK(FormatAudioTime(0.0f) == "0:00.0");
    CHECK(FormatAudioTime(7.46f) == "0:07.4");
    CHECK(FormatAudioTime(65.9f) == "1:05");
    CHECK(FormatAudioTime(-1.0f) == "0:00.0");
    CHECK(FormatFileSize(512) == "512 B");
    CHECK(FormatFileSize(1536) == "1.5 KB");
    CHECK(FormatFileSize(3 * 1024 * 1024) == "3.0 MB");
}

TEST_CASE("Editor.AudioPreview.WaveformSummarizesEveryChannel", "[engine-editor]") {
    // Two stereo frames per bucket; the right channel carries the louder sample in the second bucket.
    const std::vector<float> samples{0.5f, 0.0f, -0.5f, 0.0f, 0.1f, -0.9f, 0.1f, 0.1f};
    const auto waveform = SummarizeWaveform(samples, 2, 4, 2);
    REQUIRE(waveform.peaks.size() == 2);
    CHECK(waveform.peaks[0] == Catch::Approx(0.5f));
    CHECK(waveform.peaks[1] == Catch::Approx(0.9f));
    CHECK(waveform.rms[0] == Catch::Approx(std::sqrt(0.5f / 4.0f)));
    CHECK(waveform.rms[1] < waveform.peaks[1]);
    CHECK(waveform.seconds == Catch::Approx(1.0f));
    CHECK(waveform.channels == 2);

    CHECK(SummarizeWaveform(samples, 2, 4, 100).peaks.size() == 4); // One bucket per frame at most
    CHECK(SummarizeWaveform({}, 2, 4, 8).peaks.empty());
    CHECK(SummarizeWaveform(samples, 0, 4, 8).peaks.empty());
    const std::vector<float> loud{2.0f, -3.0f};
    CHECK(SummarizeWaveform(loud, 1, 2, 2).peaks[1] == 1.0f); // Clipped samples stay in range
}

TEST_CASE("Editor.AudioPreview.WaveformCacheDecodesInTheBackgroundOncePerVersion", "[engine-editor]") {
    TempDirectory temp;
    const auto file = temp.Path() / "hit.wav";
    Write(file);
    std::atomic<int> decodes = 0;
    AudioWaveformCache cache([&](const std::filesystem::path&) {
        ++decodes;
        return DecodedAudio{8, 1, std::vector<float>(16, 0.25f)};
    });

    const auto ready = Settled(cache, file);
    REQUIRE(ready.state == WaveformState::Ready);
    CHECK(ready.waveform->peaks.front() == Catch::Approx(0.25f));
    CHECK(cache.Request(file).waveform == ready.waveform);
    CHECK(decodes == 1);

    Write(file, "saved again"); // A new size is a new version
    CHECK(Settled(cache, file).state == WaveformState::Ready);
    CHECK(decodes == 2);
}

TEST_CASE("Editor.AudioPreview.WaveformCacheReportsDecodeFailures", "[engine-editor]") {
    TempDirectory temp;
    const auto file = temp.Path() / "broken.ogg";
    Write(file);
    std::atomic<int> decodes = 0;
    AudioWaveformCache cache([&](const std::filesystem::path&) -> DecodedAudio {
        ++decodes;
        throw std::runtime_error("Could not decode audio");
    });

    const auto failed = Settled(cache, file);
    CHECK(failed.state == WaveformState::Failed);
    CHECK(failed.error == "Could not decode audio");
    CHECK_FALSE(failed.waveform);
    CHECK(cache.Request(file).state == WaveformState::Failed); // Not retried until the file changes
    CHECK(decodes == 1);
}

TEST_CASE("Editor.AudioPreview.WorkerReportsWhichFilesTheCatalogPlays", "[engine-editor]") {
    const AudioCatalog catalog{
        .effects = {{SoundId{3}, "Audio/Hit.wav"}, {SoundId{4}, "Audio/Hit.wav", {}, "HitAgain"}},
        .music = {{MusicId{0}, "Audio/Music/Menu.ogg", MusicVolumeGroup::Menu, "Menu"}}};
    const AudioUsage usage{true, DecodeAudioCatalog(EncodeAudioCatalog(catalog))};
    REQUIRE(usage.uses.size() == 3);

    const auto hit = AudioUsesOf(usage, "Audio/hit.WAV"); // Windows paths ignore case
    REQUIRE(hit.size() == 2);
    CHECK(AudioUseLabels(hit) == "Effect #3, Effect HitAgain"); // The numeric id stands in for a missing name
    CHECK(AudioUseLabels(AudioUsesOf(usage, "Audio/Music/Menu.ogg")) == "Music Menu");
    CHECK(AudioUsesOf(usage, "Audio/Unused.ogg").empty());
    CHECK(AudioUsesOf(usage, "Audio/Music/../Hit.wav").size() == 2);
}

TEST_CASE("Editor.AudioPreview.AnalysisReportsLevelsSilenceAndClipping", "[engine-editor]") {
    // Mono at 10 Hz: 0.3 s of silence, 0.5 s of sound with one clipped sample, then 0.2 s of silence.
    std::vector<float> samples(10, 0.0f);
    for (std::size_t i = 3; i < 8; ++i)
        samples[i] = 0.5f;
    samples[5] = -1.0f;
    const auto facts = AnalyzeAudio(samples, 1, 10);
    CHECK(facts.seconds == Catch::Approx(1.0f));
    CHECK(facts.frames == 10);
    CHECK(facts.peakDb == Catch::Approx(0.0f));
    CHECK(facts.rmsDb < facts.peakDb);
    CHECK(facts.leadingSilence == Catch::Approx(0.3f));
    CHECK(facts.trailingSilence == Catch::Approx(0.2f));
    CHECK(facts.clipped == 1);

    const std::vector<float> quiet(8, 0.0001f); // Below the -60 dBFS silence level
    const auto silent = AnalyzeAudio(quiet, 2, 4);
    CHECK(silent.leadingSilence == Catch::Approx(silent.seconds));
    CHECK(silent.trailingSilence == 0.0f);
    CHECK(AnalyzeAudio(quiet, 2, 4, -100.0f).leadingSilence == 0.0f);
    CHECK(AnalyzeAudio({}, 1, 10).peakDb == AudioFloorDb);
    CHECK(AudioDecibels(0.0f) == AudioFloorDb);
    CHECK(AudioDecibels(0.5f) == Catch::Approx(-6.0206f));
}

TEST_CASE("Editor.AudioPreview.WaveformImageMarksTicksAndLoudColumns", "[engine-editor]") {
    CHECK(WaveformTickSeconds(1.0f) == Catch::Approx(0.1f));
    CHECK(WaveformTickSeconds(30.0f) == Catch::Approx(2.0f));
    CHECK(WaveformTickSeconds(1.0e6f) > 0.0f);

    AudioWaveform waveform{{0.0f, 1.0f}, {0.0f, 0.5f}, 10, 1, 1.0f};
    const auto image = DrawWaveformImage(waveform, 4, 9);
    REQUIRE(image.rgba.size() == 4u * 9u * 4u);
    const auto pixel = [&](int x, int y) {
        const auto at = (static_cast<std::size_t>(y) * 4 + static_cast<std::size_t>(x)) * 4;
        return std::vector<std::uint8_t>(image.rgba.begin() + static_cast<std::ptrdiff_t>(at),
                                         image.rgba.begin() + static_cast<std::ptrdiff_t>(at) + 4);
    };
    CHECK(pixel(0, 0) == pixel(1, 0));       // The quiet half stays empty at the top
    CHECK_FALSE(pixel(3, 0) == pixel(1, 0)); // The loud half reaches the edge
    CHECK(DrawWaveformImage({}, 0, 0).width == 1);
}
