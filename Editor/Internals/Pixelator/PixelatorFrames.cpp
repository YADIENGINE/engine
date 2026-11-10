#include "Editor/Internals/Pixelator/PixelatorFrames.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <map>
#include <stdexcept>
#include <string>

namespace {
constexpr std::size_t MaxFrames = 1024; // As the "pixels" message accepts
constexpr std::size_t MaxNames = 256;

std::vector<std::string> Numbered(const std::vector<std::string>& names) {
    std::vector<std::string> byNumber;
    if (names.empty()) return byNumber;
    byNumber.reserve(std::min(names.size(), MaxNames - 1) + 1);
    byNumber.emplace_back(); // Ink 0 / no part
    for (std::size_t index = 0; index < names.size() && index + 1 < MaxNames; ++index)
        byNumber.push_back(names[index]);
    return byNumber;
}
} // namespace

std::string PixelatorInkLabel(const PixelatorAsset& asset, int ink) {
    const auto& name = asset.InkName(ink);
    return name.empty() ? std::to_string(ink) : std::format("{} {}", ink, name);
}

PixelatorImage PixelatorImageOf(const PixelSprite& sprite) {
    PixelatorImage image;
    if (sprite.Empty()) return image;
    image.width = sprite.Width();
    image.height = sprite.Height();
    image.pivot = sprite.Pivot();
    image.inks.assign(sprite.Inks().begin(), sprite.Inks().end());
    image.parts.assign(sprite.Parts().begin(), sprite.Parts().end());
    return image;
}

PixelatorAsset PixelatorFramesOf(const GraphicPixels& pixels, std::size_t maxBytes) {
    if (pixels.frames.size() > MaxFrames)
        throw std::invalid_argument(std::format("{} frames; the Pixelator shows at most {}", pixels.frames.size(),
                                                MaxFrames));
    PixelatorAsset result;
    const auto colors = pixels.palette.Colors();
    result.palettes.emplace_back(colors.begin(), colors.end());
    result.loops = pixels.loops;
    result.inkNames = Numbered(pixels.inkNames);
    result.partNames = Numbered(pixels.partNames);
    result.ramps = pixels.ramps;
    std::map<std::uint64_t, std::size_t> imageOfSprite; // Sprite ids already seen
    std::size_t bytes = 0;
    for (std::size_t index = 0; index < pixels.frames.size(); ++index) {
        const auto& frame = pixels.frames[index];
        if (!std::isfinite(frame.seconds) || frame.seconds <= 0.0f)
            throw std::invalid_argument(std::format("Frame {} lasts {} s; frames last longer than 0 s", index,
                                                    frame.seconds));
        std::size_t image = 0;
        if (const auto known = imageOfSprite.find(frame.sprite.Id());
            known != imageOfSprite.end() && frame.sprite.Id() != 0) {
            image = known->second;
        } else {
            auto made = PixelatorImageOf(frame.sprite);
            const auto same = std::ranges::find(result.images, made);
            if (same != result.images.end()) {
                image = static_cast<std::size_t>(same - result.images.begin());
            } else {
                const std::size_t size = made.inks.size() + made.parts.size();
                if (!result.images.empty() && bytes + size > maxBytes) {
                    result.capped = true;
                    break;
                }
                bytes += size;
                image = result.images.size();
                result.images.push_back(std::move(made));
            }
            imageOfSprite[frame.sprite.Id()] = image;
        }
        result.frames.push_back({image, 0, frame.seconds});
    }
    return result;
}
