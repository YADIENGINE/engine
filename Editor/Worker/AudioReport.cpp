#include "Editor/Worker/AudioReport.h"
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include "Audio/AudioFiles.h"
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Editor/Internals/Session/AudioAnalysis.h"

namespace {
constexpr int ReportVersion = 1;
constexpr int MaxImageSide = 8192;

std::filesystem::path PathOf(const nlohmann::json& text) {
    const auto value = text.get<std::string>();
    return std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size());
}
std::string TextOf(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

nlohmann::json FileReport(const std::filesystem::path& file, float silenceDb) {
    try {
        const auto decoded = DecodeAudioFile(file);
        const auto facts = AnalyzeAudio(decoded.samples, decoded.channels, decoded.sampleRate, silenceDb);
        return {{"file", TextOf(file)},
                {"seconds", facts.seconds},
                {"sampleRate", facts.sampleRate},
                {"channels", facts.channels},
                {"frames", facts.frames},
                {"peakDb", facts.peakDb},
                {"rmsDb", facts.rmsDb},
                {"leadingSilence", facts.leadingSilence},
                {"trailingSilence", facts.trailingSilence},
                {"clipped", facts.clipped}};
    } catch (const std::exception& error) {
        return {{"file", TextOf(file)}, {"error", error.what()}};
    }
}

nlohmann::json WaveformReport(const nlohmann::json& request) {
    const auto file = PathOf(request.at("file"));
    const auto png = PathOf(request.at("path"));
    const int width = request.value("width", 0);
    const int height = request.value("height", 0);
    if (width < 1 || height < 1 || width > MaxImageSide || height > MaxImageSide)
        throw std::invalid_argument("Waveform size must be between 1 and " + std::to_string(MaxImageSide));
    const auto decoded = DecodeAudioFile(file);
    const auto waveform =
        SummarizeWaveform(decoded.samples, decoded.channels, decoded.sampleRate, static_cast<std::size_t>(width));
    auto image = DrawWaveformImage(waveform, width, height);
    std::filesystem::create_directories(png.parent_path());
    const ::Image pixels{image.rgba.data(), image.width, image.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if (!ExportImage(pixels, png.string().c_str())) throw std::runtime_error("Could not write " + png.string());
    return {{"file", TextOf(file)},   {"path", TextOf(png)},         {"width", image.width},
            {"height", image.height}, {"seconds", waveform.seconds}, {"tickSeconds", image.tickSeconds}};
}
} // namespace

void WriteAudioReport(const std::filesystem::path& request, const AudioCatalog& catalog) {
    SetTraceLogLevel(LOG_WARNING); // The answer is the file; decoder notes would only clutter the tool's output
    std::ifstream input(request, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read the audio report request: " + request.string());
    const auto json = nlohmann::json::parse(input);
    const float silenceDb = json.value("silenceDb", DefaultSilenceDb);
    nlohmann::json answer{{"version", ReportVersion}, {"catalog", EncodeAudioCatalog(catalog)}};
    auto files = nlohmann::json::array();
    for (const auto& file : json.value("files", nlohmann::json::array()))
        files.push_back(FileReport(PathOf(file), silenceDb));
    answer["files"] = std::move(files);
    if (json.contains("waveform")) answer["waveform"] = WaveformReport(json.at("waveform"));
    const auto out = PathOf(json.at("out"));
    std::ofstream output(out, std::ios::binary);
    output << answer.dump(2);
    if (!output) throw std::runtime_error("Cannot write the audio report: " + out.string());
}
