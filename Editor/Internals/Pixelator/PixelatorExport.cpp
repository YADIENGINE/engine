#include "Editor/Internals/Pixelator/PixelatorExport.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <stdexcept>
#include "Editor/Internals/Pixelator/MsfGif.h"
#include "Editor/Internals/Pixelator/PixelatorLayout.h"
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr int GifMinDelay = 2;      // Hundredths of a second; players show shorter delays as 10
constexpr int GifAlphaThreshold = 128;
constexpr int GifMaxBitDepth = 16;

void CheckScale(int scale) {
    if (scale < 1 || scale > PixelatorExportMaxScale)
        throw std::invalid_argument(std::format("The export scale is 1 to {}", PixelatorExportMaxScale));
}

ViewportPixels Blank(int width, int height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("The graphic has no pixels to export");
    if (width > PixelatorExportMaxSide || height > PixelatorExportMaxSide)
        throw std::invalid_argument(std::format("The export would be {} x {} pixels; at most {} a side: use a smaller "
                                                "scale or a grid",
                                                width, height, PixelatorExportMaxSide));
    ViewportPixels pixels;
    pixels.width = width;
    pixels.height = height;
    pixels.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, std::byte{0});
    return pixels;
}

// Draws a frame with its top-left box corner at `at` (picture pixels).
void Place(ViewportPixels& picture, const PixelatorAsset& asset, std::size_t frame, const PixelatorFrameBox& box,
           Vec2 at, int scale) {
    const auto& shown = asset.frames[frame];
    const auto& image = asset.images[shown.image];
    const auto& palette = asset.palettes[shown.palette];
    const Vec2 corner = box.pivot - image.pivot; // The sprite's corner inside the box
    for (int py = 0; py < image.height; ++py)
        for (int px = 0; px < image.width; ++px) {
            const Ink ink = image.InkAt(px, py);
            if (ink == ClearInk || ink >= palette.size()) continue;
            const Color4 color = palette[ink];
            for (int y = 0; y < scale; ++y)
                for (int x = 0; x < scale; ++x) {
                    const int tx = at.x + (corner.x + px) * scale + x;
                    const int ty = at.y + (corner.y + py) * scale + y;
                    auto* out = &picture.rgba[(static_cast<std::size_t>(ty) * static_cast<std::size_t>(picture.width) +
                                               static_cast<std::size_t>(tx)) *
                                              4];
                    out[0] = std::byte{color.r};
                    out[1] = std::byte{color.g};
                    out[2] = std::byte{color.b};
                    out[3] = std::byte{color.a};
                }
        }
}

std::filesystem::path Utf8Path(const std::string& text) { return std::u8string(text.begin(), text.end()); }

std::string ItemOrGraphic(const PixelatorAsset& asset) {
    return asset.items.empty() ? PixelatorExportFolderName(asset) : asset.items[asset.item].name;
}

std::string SafeName(std::string text) {
    for (auto& character : text)
        if (character == '/' || character == '\\' || character == ':' || character == '*' || character == '?' ||
            character == '"' || character == '<' || character == '>' || character == '|')
            character = '_';
    return text.empty() ? std::string("Graphic") : text;
}
} // namespace

const char* PixelatorExportKindName(PixelatorExportKind kind) {
    switch (kind) {
    case PixelatorExportKind::Frame: return "frame";
    case PixelatorExportKind::Strip: return "strip";
    case PixelatorExportKind::Grid : return "grid";
    case PixelatorExportKind::Gif  : return "gif";
    }
    return "frame";
}

PixelatorExportKind PixelatorExportKindOf(const std::string& name) {
    for (const auto kind : {PixelatorExportKind::Frame, PixelatorExportKind::Strip, PixelatorExportKind::Grid,
                            PixelatorExportKind::Gif})
        if (name == PixelatorExportKindName(kind)) return kind;
    throw std::invalid_argument("An export is frame, strip, grid or gif");
}

ViewportPixels ComposePixelatorFrame(const PixelatorAsset& asset, std::size_t frame, int scale) {
    CheckScale(scale);
    if (frame >= asset.frames.size())
        throw std::invalid_argument(std::format("Frame {} of {} does not exist", frame, asset.frames.size()));
    const auto box = PixelatorFramesBox(asset);
    auto picture = Blank(box.size.x * scale, box.size.y * scale);
    Place(picture, asset, frame, box, {}, scale);
    return picture;
}

int PixelatorSheetColumns(std::size_t frames, int columns) {
    if (columns > 0) return static_cast<int>(std::min<std::size_t>(static_cast<std::size_t>(columns), frames));
    int square = 1;
    while (static_cast<std::size_t>(square) * static_cast<std::size_t>(square) < frames)
        ++square;
    return square;
}

ViewportPixels ComposePixelatorSheet(const PixelatorAsset& asset, int columns, int scale) {
    CheckScale(scale);
    if (asset.frames.empty()) throw std::invalid_argument("The graphic has no frames to export");
    columns = PixelatorSheetColumns(asset.frames.size(), columns);
    const int rows = static_cast<int>((asset.frames.size() + static_cast<std::size_t>(columns) - 1) /
                                      static_cast<std::size_t>(columns));
    const auto box = PixelatorFramesBox(asset);
    const Vec2 cell = box.size * scale;
    auto picture = Blank(cell.x * columns, cell.y * rows);
    for (std::size_t frame = 0; frame < asset.frames.size(); ++frame) {
        const int column = static_cast<int>(frame % static_cast<std::size_t>(columns));
        const int row = static_cast<int>(frame / static_cast<std::size_t>(columns));
        Place(picture, asset, frame, box, {column * cell.x, row * cell.y}, scale);
    }
    return picture;
}

std::vector<int> PixelatorGifDelays(const PixelatorAsset& asset) {
    std::vector<int> delays;
    double elapsed = 0.0;
    long long shown = 0; // Hundredths already given out
    for (const auto& frame : asset.frames) {
        elapsed += frame.seconds;
        const auto end = std::llround(elapsed * 100.0);
        const int delay = std::max(GifMinDelay, static_cast<int>(end - shown));
        delays.push_back(delay);
        shown += delay;
    }
    return delays;
}

std::vector<std::byte> EncodePixelatorGif(const PixelatorAsset& asset, int scale) {
    CheckScale(scale);
    if (asset.frames.empty()) throw std::invalid_argument("The graphic has no frames to export");
    const auto box = PixelatorFramesBox(asset);
    const auto delays = PixelatorGifDelays(asset);
    const int width = box.size.x * scale;
    const int height = box.size.y * scale;
    (void)Blank(width, height); // Size checks
    MsfGifState state{};
    msf_gif_alpha_threshold = GifAlphaThreshold;
    msf_gif_bgra_flag = 0;
    if (!msf_gif_begin(&state, width, height)) throw std::runtime_error("GIF encoding failed");
    for (std::size_t frame = 0; frame < asset.frames.size(); ++frame) {
        auto picture = Blank(width, height);
        Place(picture, asset, frame, box, {}, scale);
        if (!msf_gif_frame(&state, reinterpret_cast<std::uint8_t*>(picture.rgba.data()), delays[frame],
                           GifMaxBitDepth, width * 4))
            break; // msf_gif_end reports the failure
    }
    const MsfGifResult result = msf_gif_end(&state);
    if (!result.data) throw std::runtime_error("GIF encoding failed (out of memory)");
    const auto* bytes = static_cast<const std::byte*>(result.data);
    std::vector<std::byte> gif(bytes, bytes + result.dataSize);
    msf_gif_free(result);
    return gif;
}

std::string PixelatorExportFolderName(const PixelatorAsset& asset) {
    auto name = asset.id.substr(0, asset.id.find('#'));
    if (const auto slash = name.find_last_of("/:"); slash != std::string::npos) name = name.substr(slash + 1);
    return SafeName(name);
}

std::string PixelatorExportFileName(const PixelatorAsset& asset, const PixelatorExportOptions& options) {
    const auto base = SafeName(ItemOrGraphic(asset));
    switch (options.kind) {
    case PixelatorExportKind::Frame: return std::format("{}-frame{}.png", base, options.frame + 1);
    case PixelatorExportKind::Strip: return base + "-strip.png";
    case PixelatorExportKind::Grid : return base + "-grid.png";
    case PixelatorExportKind::Gif  : return base + ".gif";
    }
    return base + ".png";
}

std::filesystem::path PixelatorExportFolder(const std::filesystem::path& projectRoot, const PixelatorAsset& asset) {
    return projectRoot / "Exports" / Utf8Path(PixelatorExportFolderName(asset));
}

std::filesystem::path WritePixelatorExport(const std::filesystem::path& projectRoot, const PixelatorAsset& asset,
                                           const PixelatorExportOptions& options,
                                           const std::function<std::vector<std::byte>(const ViewportPixels&)>& encodePng) {
    if (projectRoot.empty()) throw std::runtime_error("Exports go to the project's Exports folder; no project is open");
    if (!asset.error.empty()) throw std::runtime_error(asset.error);
    std::vector<std::byte> bytes;
    switch (options.kind) {
    case PixelatorExportKind::Frame: bytes = encodePng(ComposePixelatorFrame(asset, options.frame, options.scale)); break;
    case PixelatorExportKind::Strip:
        bytes = encodePng(ComposePixelatorSheet(asset, static_cast<int>(asset.frames.size()), options.scale));
        break;
    case PixelatorExportKind::Grid: bytes = encodePng(ComposePixelatorSheet(asset, options.columns, options.scale)); break;
    case PixelatorExportKind::Gif : bytes = EncodePixelatorGif(asset, options.scale); break;
    }
    const auto folder = PixelatorExportFolder(projectRoot, asset);
    std::filesystem::create_directories(folder);
    const auto file = folder / Utf8Path(PixelatorExportFileName(asset, options));
    if (!WriteBinaryFileAtomic(file, {reinterpret_cast<const char*>(bytes.data()), bytes.size()}))
        throw std::runtime_error("Could not write " + file.generic_string());
    return file;
}
