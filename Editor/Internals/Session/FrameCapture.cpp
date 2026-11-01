#include "Editor/Internals/Session/FrameCapture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace {
constexpr int MaxCaptureSide = 4096;
constexpr int Channels = 4;
std::byte Channel(const PreviewPixels& frame, int x, int y, int channel) {
    return frame
        .rgba[(static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)) *
                  Channels +
              static_cast<std::size_t>(channel)];
}
} // namespace

PreviewPixels CaptureFrame(const PreviewPixels& frame, Vec2 canvas, const FrameCaptureOptions& options) {
    if (frame.width <= 0 || frame.height <= 0 || canvas.x <= 0 || canvas.y <= 0 ||
        frame.rgba.size() != static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * Channels)
        throw std::runtime_error("No usable preview frame");
    if (!std::isfinite(options.scale) || options.scale < FrameCaptureMinScale || options.scale > FrameCaptureMaxScale)
        throw std::runtime_error(
            std::format("Scale must be between {} and {}", FrameCaptureMinScale, FrameCaptureMaxScale));
    const FloatRect area =
        options.crop.value_or(FloatRect{0.0f, 0.0f, static_cast<float>(canvas.x), static_cast<float>(canvas.y)});
    if (area.width <= 0.0f || area.height <= 0.0f || area.x < 0.0f || area.y < 0.0f ||
        area.Right() > static_cast<float>(canvas.x) || area.Bottom() > static_cast<float>(canvas.y))
        throw std::runtime_error(std::format("Crop must lie inside the {}x{} canvas", canvas.x, canvas.y));
    // Canvas units onto frame pixels (the worker renders the canvas at its own size, but do not rely on it)
    const double toPixelX = static_cast<double>(frame.width) / canvas.x;
    const double toPixelY = static_cast<double>(frame.height) / canvas.y;
    const double left = area.x * toPixelX, top = area.y * toPixelY;
    const double sourceWidth = area.width * toPixelX, sourceHeight = area.height * toPixelY;
    PreviewPixels result;
    result.width = std::clamp(static_cast<int>(std::lround(sourceWidth * options.scale)), 1, MaxCaptureSide);
    result.height = std::clamp(static_cast<int>(std::lround(sourceHeight * options.scale)), 1, MaxCaptureSide);
    result.sequence = frame.sequence;
    result.capturedAt = frame.capturedAt;
    result.rgba.resize(static_cast<std::size_t>(result.width) * static_cast<std::size_t>(result.height) * Channels);
    const double stepX = sourceWidth / result.width, stepY = sourceHeight / result.height;
    for (int y = 0; y < result.height; ++y) {
        // Each output pixel averages the source pixels its footprint covers (at least the one under its center)
        const int y0 = std::clamp(static_cast<int>(std::floor(top + y * stepY)), 0, frame.height - 1);
        const int y1 = std::clamp(static_cast<int>(std::ceil(top + (y + 1) * stepY)), y0 + 1, frame.height);
        for (int x = 0; x < result.width; ++x) {
            const int x0 = std::clamp(static_cast<int>(std::floor(left + x * stepX)), 0, frame.width - 1);
            const int x1 = std::clamp(static_cast<int>(std::ceil(left + (x + 1) * stepX)), x0 + 1, frame.width);
            std::array<unsigned, Channels> sum{};
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = x0; sx < x1; ++sx)
                    for (int c = 0; c < Channels; ++c)
                        sum[static_cast<std::size_t>(c)] += std::to_integer<unsigned>(Channel(frame, sx, sy, c));
            const auto count = static_cast<unsigned>((y1 - y0) * (x1 - x0));
            const auto at =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(result.width) + static_cast<std::size_t>(x)) *
                Channels;
            for (int c = 0; c < Channels; ++c)
                result.rgba[at + static_cast<std::size_t>(c)] =
                    static_cast<std::byte>((sum[static_cast<std::size_t>(c)] + count / 2) / count);
        }
    }
    return result;
}
std::filesystem::path NewCapturePath(const std::filesystem::path& directory) {
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const std::string stamp = std::format("{:%Y%m%d-%H%M%S}", now);
    for (int attempt = 0;; ++attempt) {
        auto path = directory / (attempt ? std::format("{}-{}.png", stamp, attempt) : stamp + ".png");
        if (!std::filesystem::exists(path)) return path;
    }
}
void PruneCaptures(const std::filesystem::path& directory, std::size_t keep) {
    std::error_code error;
    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file(error) || entry.path().extension() != ".png") continue;
        files.emplace_back(entry.last_write_time(error), entry.path());
    }
    if (files.size() <= keep) return;
    std::ranges::sort(files, std::greater{}); // Newest first
    for (std::size_t i = keep; i < files.size(); ++i)
        std::filesystem::remove(files[i].second, error);
}
