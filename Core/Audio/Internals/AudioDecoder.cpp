#include "Audio/Internals/AudioDecoder.h"
#include <miniaudio.h>
#define STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
[[nodiscard]] ma_result OpenDecoder(const std::filesystem::path& file, ma_decoder& decoder) {
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0); // The file's channels and rate
#ifdef _WIN32
    return ma_decoder_init_file_w(file.c_str(), &config, &decoder);
#else
    return ma_decoder_init_file(file.c_str(), &config, &decoder);
#endif
}
} // namespace

float AudioFileSeconds(const std::filesystem::path& file) {
    // miniaudio reads Vorbis in stb_vorbis's push mode, which cannot tell the length; its pull mode reads it at once.
    if (file.extension() == ".ogg") {
        std::FILE* opened = nullptr;
#ifdef _WIN32
        if (_wfopen_s(&opened, file.c_str(), L"rb") != 0) return 0.0f;
#else
        opened = std::fopen(file.c_str(), "rb");
#endif
        if (opened == nullptr) return 0.0f;
        int error = 0;
        stb_vorbis* vorbis = stb_vorbis_open_file(opened, 1, &error, nullptr); // Closes the file with the decoder
        if (vorbis == nullptr) {
            std::fclose(opened);
            return 0.0f;
        }
        const float seconds = stb_vorbis_stream_length_in_seconds(vorbis);
        stb_vorbis_close(vorbis);
        return seconds;
    }
    ma_decoder decoder;
    if (OpenDecoder(file, decoder) != MA_SUCCESS) return 0.0f;
    ma_uint64 frames = 0;
    const ma_result result = ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    const ma_uint32 rate = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);
    return result == MA_SUCCESS && rate > 0 ? static_cast<float>(static_cast<double>(frames) / rate) : 0.0f;
}

DecodedAudio DecodeAudioSamples(const std::filesystem::path& file) {
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Audio file not found: " + file.string());
    ma_decoder decoder;
    if (OpenDecoder(file, decoder) != MA_SUCCESS) throw std::runtime_error("Could not decode audio: " + file.string());

    DecodedAudio decoded{static_cast<int>(decoder.outputSampleRate), static_cast<int>(decoder.outputChannels), {}};
    const std::size_t channels = decoder.outputChannels;
    // In pieces, since not every format knows its length before it has been read.
    constexpr std::size_t PieceFrames = 65536;
    try {
        for (;;) {
            const std::size_t before = decoded.samples.size();
            decoded.samples.resize(before + PieceFrames * channels);
            ma_uint64 read = 0;
            const ma_result result =
                ma_decoder_read_pcm_frames(&decoder, decoded.samples.data() + before, PieceFrames, &read);
            decoded.samples.resize(before + static_cast<std::size_t>(read) * channels);
            if (result == MA_AT_END || read == 0) break;
            if (result != MA_SUCCESS) throw std::runtime_error("Could not decode audio: " + file.string());
        }
    } catch (...) {
        ma_decoder_uninit(&decoder);
        throw;
    }
    ma_decoder_uninit(&decoder);
    if (decoded.samples.empty()) throw std::runtime_error("Could not decode audio: " + file.string());
    return decoded;
}
