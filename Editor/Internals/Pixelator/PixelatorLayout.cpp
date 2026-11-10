#include "Editor/Internals/Pixelator/PixelatorLayout.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace {
std::uint8_t Halfway(std::uint8_t from, std::uint8_t to) { return static_cast<std::uint8_t>((from + to + 1) / 2); }
} // namespace

std::vector<PixelatorGhost> PixelatorOnionGhosts(std::size_t frameCount, std::size_t current, bool loops,
                                                 const PixelatorOnionOptions& options) {
    std::vector<PixelatorGhost> ghosts;
    if (frameCount < 2 || current >= frameCount) return ghosts;
    const int before = std::clamp(options.before, 0, PixelatorOnionMaxFrames);
    const int after = std::clamp(options.after, 0, PixelatorOnionMaxFrames);
    const float opacity = std::clamp(options.opacity, 0.0f, 1.0f);
    const auto count = static_cast<long long>(frameCount);
    std::vector<std::size_t> shown{current};
    const auto add = [&](int distance) {
        long long frame = static_cast<long long>(current) + distance;
        if (loops)
            frame = ((frame % count) + count) % count;
        else if (frame < 0 || frame >= count)
            return;
        const auto index = static_cast<std::size_t>(frame);
        if (std::ranges::find(shown, index) != shown.end()) return;
        shown.push_back(index);
        PixelatorGhost ghost{index, distance, opacity / static_cast<float>(std::abs(distance)), std::nullopt};
        if (options.tinted) ghost.tint = distance < 0 ? PixelatorOnionBefore : PixelatorOnionAfter;
        ghosts.push_back(ghost);
    };
    // Nearest first, so a short loop gives a frame to its nearer side, then reversed to draw the farthest first.
    for (int step = 1; step <= std::max(before, after); ++step) {
        if (step <= before) add(-step);
        if (step <= after) add(step);
    }
    std::ranges::reverse(ghosts);
    return ghosts;
}

std::vector<Color4> PixelatorGhostColors(const PixelatorImage& image, std::span<const Color4> palette,
                                         const PixelatorGhost& ghost) {
    std::vector<Color4> colors(image.inks.size(), Color4{0, 0, 0, 0});
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const Ink ink = image.inks[index];
        if (ink == ClearInk || ink >= palette.size()) continue;
        Color4 color = palette[ink];
        if (ghost.tint) color = {Halfway(color.r, ghost.tint->r), Halfway(color.g, ghost.tint->g),
                                 Halfway(color.b, ghost.tint->b), color.a};
        color.a = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.a) * ghost.opacity));
        colors[index] = color;
    }
    return colors;
}

Vec2 PixelatorPivotOffset(const PixelatorImage& image, const PixelatorImage& other) { return image.pivot - other.pivot; }

PixelatorFrameBox PixelatorFramesBox(const PixelatorAsset& asset) {
    int left = INT_MAX, top = INT_MAX, right = INT_MIN, bottom = INT_MIN;
    for (const auto& frame : asset.frames) {
        const auto& image = asset.images[frame.image];
        if (image.Empty()) continue;
        // In pivot coordinates: the pivot is (0, 0).
        left = std::min(left, -image.pivot.x);
        top = std::min(top, -image.pivot.y);
        right = std::max(right, image.width - image.pivot.x);
        bottom = std::max(bottom, image.height - image.pivot.y);
    }
    if (left == INT_MAX) return {};
    return {{right - left, bottom - top}, {-left, -top}};
}

std::vector<PixelatorPane> PixelatorPanes(Float2 area, int count, float gap) {
    count = std::clamp(count, 1, PixelatorMaxPanes);
    int columns = count;
    int rows = 1;
    if (count == 4) {
        columns = 2;
        rows = 2;
    } else if (area.y > area.x) {
        columns = 1;
        rows = count;
    }
    const float width = std::floor(std::max(1.0f, (area.x - gap * static_cast<float>(columns - 1)) /
                                                      static_cast<float>(columns)));
    const float height =
        std::floor(std::max(1.0f, (area.y - gap * static_cast<float>(rows - 1)) / static_cast<float>(rows)));
    std::vector<PixelatorPane> panes;
    for (int index = 0; index < count; ++index) {
        const int column = index % columns;
        const int row = index / columns;
        panes.push_back({{std::floor(static_cast<float>(column) * (width + gap)),
                          std::floor(static_cast<float>(row) * (height + gap))},
                         {width, height}});
    }
    return panes;
}

Vec2 PixelatorTiledPixel(Vec2 pixel, Vec2 sprite) {
    if (sprite.x <= 0 || sprite.y <= 0) return {};
    return {((pixel.x % sprite.x) + sprite.x) % sprite.x, ((pixel.y % sprite.y) + sprite.y) % sprite.y};
}
