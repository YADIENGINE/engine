#include "Editor/Internals/Pixelator/PixelatorDiff.h"
#include <algorithm>
#include "Editor/Internals/Pixelator/PixelatorLayout.h"

namespace {
const std::vector<Color4>& PaletteOf(const PixelatorAsset& asset, std::size_t frame) {
    static const std::vector<Color4> None;
    return frame < asset.frames.size() ? asset.palettes[asset.frames[frame].palette] : None;
}

void ComparePixels(const PixelatorImage& before, const PixelatorImage& after, PixelatorFrameChange& change) {
    // `before` pixel (x, y) is at (x, y) + offset in `after`.
    const Vec2 offset = PixelatorPivotOffset(after, before);
    const int left = std::min(0, offset.x);
    const int top = std::min(0, offset.y);
    const int right = std::max(after.width, before.width + offset.x);
    const int bottom = std::max(after.height, before.height + offset.y);
    const auto inside = [](const PixelatorImage& image, int x, int y) {
        return x >= 0 && y >= 0 && x < image.width && y < image.height;
    };
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x) {
            const int bx = x - offset.x;
            const int by = y - offset.y;
            const Ink inkAfter = inside(after, x, y) ? after.InkAt(x, y) : ClearInk;
            const Ink inkBefore = inside(before, bx, by) ? before.InkAt(bx, by) : ClearInk;
            if (inkAfter != inkBefore) {
                change.pixels.push_back({x, y});
            } else if (inkAfter != ClearInk && after.PartAt(x, y) != before.PartAt(bx, by)) {
                ++change.partsOnly;
            }
        }
}
} // namespace

std::size_t PixelatorDiff::ChangedPixels() const {
    std::size_t count = 0;
    for (const auto& frame : frames)
        count += frame.pixels.size();
    return count;
}

const PixelatorFrameChange* PixelatorDiff::Frame(std::size_t frame) const {
    const auto found = std::ranges::find(frames, frame, &PixelatorFrameChange::frame);
    return found == frames.end() ? nullptr : &*found;
}

PixelatorDiff DiffPixelator(const PixelatorAsset& before, const PixelatorAsset& after) {
    PixelatorDiff diff;
    diff.framesBefore = before.frames.size();
    diff.framesAfter = after.frames.size();
    const auto& oldPalette = PaletteOf(before, 0);
    const auto& newPalette = PaletteOf(after, 0);
    for (std::size_t ink = 1; ink < std::max(oldPalette.size(), newPalette.size()); ++ink) {
        const Color4 was = ink < oldPalette.size() ? oldPalette[ink] : Color4{0, 0, 0, 0};
        const Color4 is = ink < newPalette.size() ? newPalette[ink] : Color4{0, 0, 0, 0};
        if (!(was == is)) diff.palette.push_back({static_cast<int>(ink), was, is});
    }
    static const PixelatorImage Empty;
    for (std::size_t frame = 0; frame < std::max(diff.framesBefore, diff.framesAfter); ++frame) {
        PixelatorFrameChange change;
        change.frame = frame;
        change.added = frame >= diff.framesBefore;
        change.removed = frame >= diff.framesAfter;
        const auto& was = change.added ? Empty : before.images[before.frames[frame].image];
        const auto& is = change.removed ? Empty : after.images[after.frames[frame].image];
        change.sizeBefore = {was.width, was.height};
        change.sizeAfter = {is.width, is.height};
        change.pivotBefore = was.pivot;
        change.pivotAfter = is.pivot;
        if (!change.added) change.secondsBefore = before.frames[frame].seconds;
        if (!change.removed) change.secondsAfter = after.frames[frame].seconds;
        ComparePixels(was, is, change);
        const bool changed = change.added || change.removed || !change.pixels.empty() || change.partsOnly > 0 ||
                             !(change.sizeBefore == change.sizeAfter) || !(change.pivotBefore == change.pivotAfter) ||
                             change.secondsBefore != change.secondsAfter;
        if (changed) diff.frames.push_back(std::move(change));
    }
    return diff;
}

std::vector<Color4> PixelatorDiffColors(const PixelatorImage& image, std::span<const Color4> palette,
                                        const PixelatorFrameChange* change) {
    std::vector<Color4> colors(image.inks.size(), Color4{0, 0, 0, 0});
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const Ink ink = image.inks[index];
        if (ink == ClearInk || ink >= palette.size()) continue;
        colors[index] = palette[ink];
        colors[index].a = static_cast<std::uint8_t>(colors[index].a * PixelatorDiffDimAlpha / 255);
    }
    if (!change) return colors;
    for (const Vec2 pixel : change->pixels) {
        if (pixel.x < 0 || pixel.y < 0 || pixel.x >= image.width || pixel.y >= image.height) continue;
        const auto index = static_cast<std::size_t>(pixel.y * image.width + pixel.x);
        const Ink ink = image.inks[index];
        colors[index] = ink == ClearInk ? PixelatorDiffRemoved : ink < palette.size() ? palette[ink] : Color4{0, 0, 0, 0};
    }
    return colors;
}
