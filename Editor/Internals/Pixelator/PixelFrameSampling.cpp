#include "Editor/Internals/Pixelator/PixelFrameSampling.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace {
bool SameStep(const PixelatorFrame& a, const PixelatorFrame& b, float tolerance) {
    return a.image == b.image && a.palette == b.palette && std::abs(a.seconds - b.seconds) <= tolerance;
}

// The shortest cycle the frames repeat, when there is one: every frame after the first `period` equals the one
// `period` before it, except that the last one (cut by the window) may be shorter. The repeat must cover at least two
// frames and a quarter of the cycle, so a long cycle is found with less than two passes in the window. 0 when none.
std::size_t CyclePeriod(const std::vector<PixelatorFrame>& frames, float tolerance) {
    constexpr float MinRepeatShare = 0.25f;
    const std::size_t count = frames.size();
    for (std::size_t period = 1; period + 2 <= count; ++period) {
        float cycle = 0.0f;
        for (std::size_t index = 0; index < period; ++index)
            cycle += frames[index].seconds;
        float repeated = 0.0f;
        for (std::size_t index = period; index < count; ++index)
            repeated += frames[index].seconds;
        if (repeated < cycle * MinRepeatShare) continue;
        bool repeats = true;
        for (std::size_t index = period; index + 1 < count && repeats; ++index)
            repeats = SameStep(frames[index], frames[index - period], tolerance);
        const auto& last = frames[count - 1];
        const auto& counterpart = frames[count - 1 - period];
        repeats = repeats && last.image == counterpart.image && last.palette == counterpart.palette &&
                  last.seconds <= counterpart.seconds + tolerance;
        if (repeats) return period;
    }
    return 0;
}
} // namespace

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

PixelatorAsset SamplePixelFrames(const std::function<GraphicPixels(float time)>& pixels,
                                 const PixelSamplingLimits& limits) {
    if (limits.rate <= 0 || !(limits.window > 0.0f) || limits.maxFrames == 0)
        throw std::invalid_argument("Pixel sampling needs a positive rate, window and frame limit");
    PixelatorAsset result;
    const float step = 1.0f / static_cast<float>(limits.rate);
    const auto samples = std::max(1, static_cast<int>(std::lround(limits.window * static_cast<float>(limits.rate))));
    const auto started = std::chrono::steady_clock::now();
    std::map<std::uint64_t, std::size_t> imageOfSprite; // Sprite ids already seen
    std::size_t bytes = 0;
    int sampled = 0;
    for (int sample = 0; sample < samples; ++sample) {
        if (sample > 0 &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() > limits.maxWallSeconds) {
            result.capped = true;
            break;
        }
        const GraphicPixels shown = pixels(static_cast<float>(sample) * step);
        std::size_t image = 0;
        if (const auto known = imageOfSprite.find(shown.sprite.Id());
            known != imageOfSprite.end() && shown.sprite.Id() != 0) {
            image = known->second;
        } else {
            auto made = PixelatorImageOf(shown.sprite);
            const auto same = std::ranges::find(result.images, made);
            if (same != result.images.end()) {
                image = static_cast<std::size_t>(same - result.images.begin());
            } else {
                const std::size_t size = made.inks.size() + made.parts.size();
                if (!result.images.empty() && bytes + size > limits.maxBytes) {
                    result.capped = true;
                    break;
                }
                bytes += size;
                image = result.images.size();
                result.images.push_back(std::move(made));
            }
            imageOfSprite[shown.sprite.Id()] = image;
        }
        const auto colors = shown.palette.Colors();
        auto palette = static_cast<std::size_t>(
            std::ranges::find_if(result.palettes,
                                 [&](const std::vector<Color4>& known) { return std::ranges::equal(known, colors); }) -
            result.palettes.begin());
        if (palette == result.palettes.size()) result.palettes.emplace_back(colors.begin(), colors.end());
        if (!result.frames.empty() && result.frames.back().image == image && result.frames.back().palette == palette) {
            result.frames.back().seconds += step;
        } else {
            if (result.frames.size() == limits.maxFrames) {
                result.capped = true;
                break;
            }
            result.frames.push_back({image, palette, step});
        }
        ++sampled;
    }
    result.window = static_cast<float>(sampled) * step;
    if (!result.capped && result.frames.size() > 2)
        if (const auto period = CyclePeriod(result.frames, step * 1.5f)) {
            result.frames.resize(period);
            result.looped = true;
        }
    // Drop sprites and palettes only frames past the kept cycle used.
    std::vector<std::size_t> imageIndex(result.images.size(), result.images.size());
    std::vector<std::size_t> paletteIndex(result.palettes.size(), result.palettes.size());
    std::vector<PixelatorImage> images;
    std::vector<std::vector<Color4>> palettes;
    for (auto& frame : result.frames) {
        if (imageIndex[frame.image] == result.images.size()) {
            imageIndex[frame.image] = images.size();
            images.push_back(std::move(result.images[frame.image]));
        }
        if (paletteIndex[frame.palette] == result.palettes.size()) {
            paletteIndex[frame.palette] = palettes.size();
            palettes.push_back(std::move(result.palettes[frame.palette]));
        }
        frame.image = imageIndex[frame.image];
        frame.palette = paletteIndex[frame.palette];
    }
    result.images = std::move(images);
    result.palettes = std::move(palettes);
    return result;
}
