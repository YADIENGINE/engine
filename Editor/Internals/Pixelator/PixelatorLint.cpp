#include "Editor/Internals/Pixelator/PixelatorLint.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <map>
#include <set>
#include <utility>
#include "Utilities/Color/Oklab.h"

namespace {
constexpr std::array<Vec2, 4> Sides{Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, 1}, Vec2{0, -1}};
constexpr std::size_t MaxRampPixels = 64; // Pixels listed for a ramp finding

class Grid {
public:
    explicit Grid(const PixelatorImage& image)
        : _image(image) {}
    [[nodiscard]] bool Inside(int x, int y) const { return x >= 0 && y >= 0 && x < _image.width && y < _image.height; }
    // Ink 0 outside the sprite.
    [[nodiscard]] Ink At(int x, int y) const { return Inside(x, y) ? _image.InkAt(x, y) : ClearInk; }
    [[nodiscard]] int Width() const { return _image.width; }
    [[nodiscard]] int Height() const { return _image.height; }
    // Drawn, and beside a transparent pixel inside the sprite.
    [[nodiscard]] bool Edge(int x, int y) const {
        if (At(x, y) == ClearInk) return false;
        return std::ranges::any_of(Sides, [&](Vec2 side) {
            return Inside(x + side.x, y + side.y) && At(x + side.x, y + side.y) == ClearInk;
        });
    }

private:
    const PixelatorImage& _image;
};

std::string Pixel(Vec2 at) { return std::format("{}, {}", at.x, at.y); }

// What one sprite's rules produce, capped per rule.
class Findings {
public:
    Findings(std::vector<PixelatorFinding>& out, const PixelatorLintOptions& options)
        : _out(out)
        , _options(options) {}
    void Add(PixelatorFinding finding) {
        const auto key = std::pair{finding.rule, finding.image.value_or(SIZE_MAX)};
        const int count = ++_counts[key];
        if (count <= _options.maxPerRule) {
            _out.push_back(std::move(finding));
            _last[key] = _out.size() - 1;
        } else {
            auto& last = _out[_last[key]];
            const auto more = count - _options.maxPerRule;
            const auto cut = last.text.find(" (and ");
            last.text = last.text.substr(0, cut) + std::format(" (and {} more like it)", more);
        }
    }

private:
    std::vector<PixelatorFinding>& _out;
    const PixelatorLintOptions& _options;
    std::map<std::pair<std::string, std::size_t>, int> _counts;
    std::map<std::pair<std::string, std::size_t>, std::size_t> _last;
};

struct Sprite {
    std::size_t image = 0;
    const PixelatorImage* pixels = nullptr;
    std::vector<std::size_t> frames;
    const std::vector<Color4>* palette = nullptr;
};

void StrayPixels(const Sprite& sprite, const std::vector<OklabColor>& lab, const PixelatorAsset& asset,
                 const PixelatorLintOptions& options, Findings& findings) {
    const Grid grid(*sprite.pixels);
    std::map<std::pair<Ink, Ink>, std::vector<Vec2>> lone; // (ink, surround) → pixels
    for (int y = 1; y + 1 < grid.Height(); ++y)
        for (int x = 1; x + 1 < grid.Width(); ++x) {
            const Ink ink = grid.At(x, y);
            if (ink == ClearInk) continue;
            const Ink around = grid.At(x + 1, y);
            if (around == ClearInk || around == ink) continue;
            if (!std::ranges::all_of(Sides, [&](Vec2 side) { return grid.At(x + side.x, y + side.y) == around; }))
                continue;
            bool alone = true;
            for (int dy = -1; dy <= 1 && alone; ++dy)
                for (int dx = -1; dx <= 1 && alone; ++dx)
                    if ((dx != 0 || dy != 0) && grid.At(x + dx, y + dy) == ink) alone = false;
            if (!alone || ink >= lab.size() || around >= lab.size()) continue;
            if (OklabDistance(lab[ink], lab[around]) >= options.strayContrast) continue; // A deliberate detail
            lone[{ink, around}].push_back({x, y});
        }
    for (const auto& [inks, pixels] : lone) {
        if (static_cast<int>(pixels.size()) > options.strayRepeats) continue; // A texture
        for (const Vec2 at : pixels)
            findings.Add({"stray-pixel", PixelatorLintSeverity::Warning, sprite.image, sprite.frames, {at},
                          {inks.first, inks.second},
                          std::format("Lone pixel of ink {} inside ink {} at {}: a single pixel this close in tone "
                                      "reads as noise; remove it or make it part of a shape",
                                      PixelatorInkLabel(asset, inks.first), PixelatorInkLabel(asset, inks.second),
                                      Pixel(at))});
    }
}

// The ink most of the edge is drawn in, when the sprite has an outline.
std::optional<Ink> OutlineInk(const Grid& grid, const PixelatorLintOptions& options) {
    std::array<int, MaxInks> counts{};
    int edge = 0;
    for (int y = 0; y < grid.Height(); ++y)
        for (int x = 0; x < grid.Width(); ++x)
            if (grid.Edge(x, y)) {
                ++counts[grid.At(x, y)];
                ++edge;
            }
    if (edge < options.outlineMinEdge) return std::nullopt;
    const auto most = std::ranges::max_element(counts);
    if (static_cast<float>(*most) < options.outlineShare * static_cast<float>(edge)) return std::nullopt;
    return static_cast<Ink>(most - counts.begin());
}

void OutlineDoubles(const Sprite& sprite, Ink outline, Findings& findings) {
    const Grid grid(*sprite.pixels);
    std::vector<bool> reported(sprite.pixels->inks.size(), false);
    const auto wasReported = [&](int x, int y) {
        return grid.Inside(x, y) && reported[static_cast<std::size_t>(y * grid.Width() + x)];
    };
    for (int y = 0; y < grid.Height(); ++y)
        for (int x = 0; x < grid.Width(); ++x) {
            if (grid.At(x, y) != outline || !grid.Edge(x, y)) continue;
            for (const Vec2 bend : {Vec2{1, 1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{-1, -1}}) {
                // An L: arms beside and above/below in the outline ink, nothing drawn inside the bend, and nothing
                // but the outside on the pixel's other two sides, so removing it opens no gap. One pixel per L.
                if (grid.At(x + bend.x, y) != outline || grid.At(x, y + bend.y) != outline) continue;
                if (!grid.Inside(x + bend.x, y + bend.y) || grid.At(x + bend.x, y + bend.y) != ClearInk) continue;
                if (grid.At(x - bend.x, y) != ClearInk || grid.At(x, y - bend.y) != ClearInk) continue;
                if (wasReported(x + bend.x, y) || wasReported(x, y + bend.y)) break;
                reported[static_cast<std::size_t>(y * grid.Width() + x)] = true;
                findings.Add({"outline-double", PixelatorLintSeverity::Warning, sprite.image, sprite.frames, {{x, y}},
                              {outline},
                              std::format("Double on the outline at {}: this corner pixel thickens the line to two "
                                          "pixels; without it the step stays one pixel wide",
                                          Pixel({x, y}))});
                break;
            }
        }
}

void OutlineGaps(const Sprite& sprite, Ink outline, const PixelatorAsset& asset, const PixelatorLintOptions& options,
                 Findings& findings) {
    const Grid grid(*sprite.pixels);
    const int width = grid.Width();
    const int height = grid.Height();
    const auto index = [&](int x, int y) { return static_cast<std::size_t>(y * width + x); };
    // Size of each drawn pixel's 8-connected group: tiny groups are particles, not gaps.
    std::vector<int> group(static_cast<std::size_t>(width * height), -1);
    std::vector<int> sizes;
    std::vector<Vec2> stack;
    // The 8-connected pixels around (x0, y0) that are `member`, marked `id` in `marks`.
    const auto flood = [&](int x0, int y0, auto&& member, std::vector<int>& marks, int id) {
        std::vector<Vec2> members;
        stack.assign(1, {x0, y0});
        marks[index(x0, y0)] = id;
        while (!stack.empty()) {
            const Vec2 at = stack.back();
            stack.pop_back();
            members.push_back(at);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = at.x + dx, ny = at.y + dy;
                    if (!grid.Inside(nx, ny) || marks[index(nx, ny)] != -1 || !member(nx, ny)) continue;
                    marks[index(nx, ny)] = id;
                    stack.push_back({nx, ny});
                }
        }
        return members;
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (grid.At(x, y) != ClearInk && group[index(x, y)] == -1)
                sizes.push_back(static_cast<int>(
                    flood(
                        x, y, [&](int nx, int ny) { return grid.At(nx, ny) != ClearInk; }, group,
                        static_cast<int>(sizes.size()))
                        .size()));
    const auto gap = [&](int x, int y) {
        return grid.Edge(x, y) && grid.At(x, y) != outline &&
               sizes[static_cast<std::size_t>(group[index(x, y)])] > options.particleSize;
    };
    std::vector<int> cluster(static_cast<std::size_t>(width * height), -1);
    int clusters = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            if (cluster[index(x, y)] != -1 || !gap(x, y)) continue;
            auto pixels = flood(x, y, gap, cluster, clusters++);
            std::ranges::sort(pixels, [](Vec2 a, Vec2 b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
            std::set<int> inks;
            for (const Vec2 at : pixels)
                inks.insert(grid.At(at.x, at.y));
            std::string named;
            for (const int ink : inks)
                named += (named.empty() ? "" : ", ") + PixelatorInkLabel(asset, ink);
            const auto count = pixels.size();
            findings.Add({"outline-gap", PixelatorLintSeverity::Warning, sprite.image, sprite.frames, pixels,
                          {inks.begin(), inks.end()},
                          std::format("Outline gap at {}: {} pixel{} of ink {} touch the outside where the rest of the "
                                      "edge is ink {}",
                                      Pixel(pixels.front()), count, count == 1 ? "" : "s", named,
                                      PixelatorInkLabel(asset, outline))});
        }
}

// Where two inks touch, across every sprite of the item: pixel edges and a few of the pixels.
struct Contact {
    int edges = 0;
    std::vector<std::pair<std::size_t, Vec2>> pixels; // (image, pixel)
};

std::map<std::pair<Ink, Ink>, Contact> Contacts(const std::vector<Sprite>& sprites) {
    std::map<std::pair<Ink, Ink>, Contact> contacts;
    for (const auto& sprite : sprites) {
        const Grid grid(*sprite.pixels);
        for (int y = 0; y < grid.Height(); ++y)
            for (int x = 0; x < grid.Width(); ++x) {
                const Ink ink = grid.At(x, y);
                if (ink == ClearInk) continue;
                for (const Vec2 side : {Vec2{1, 0}, Vec2{0, 1}}) {
                    const Ink other = grid.At(x + side.x, y + side.y);
                    if (other == ClearInk || other == ink) continue;
                    auto& contact = contacts[{std::min(ink, other), std::max(ink, other)}];
                    ++contact.edges;
                    if (contact.pixels.size() < MaxRampPixels) contact.pixels.push_back({sprite.image, {x, y}});
                }
            }
    }
    return contacts;
}

void RampContrast(const std::vector<Sprite>& sprites, const std::vector<OklabColor>& lab,
                  const std::array<bool, MaxInks>& used, const PixelatorAsset& asset,
                  const PixelatorLintOptions& options, Findings& findings) {
    const auto contacts = Contacts(sprites);
    const auto where = [&](Ink a, Ink b) {
        std::vector<Vec2> pixels;
        std::optional<std::size_t> image;
        if (const auto found = contacts.find({std::min(a, b), std::max(a, b)}); found != contacts.end())
            for (const auto& [shown, pixel] : found->second.pixels)
                if (!image || *image == shown) {
                    image = shown;
                    pixels.push_back(pixel);
                }
        return std::pair{image, pixels};
    };
    const auto framesOf = [&](std::optional<std::size_t> image) {
        for (const auto& sprite : sprites)
            if (image && sprite.image == *image) return sprite.frames;
        return std::vector<std::size_t>{};
    };
    std::set<std::pair<Ink, Ink>> reported;
    if (!asset.ramps.empty()) {
        for (const auto& ramp : asset.ramps) {
            std::vector<Ink> steps;
            for (const Ink ink : {ramp.deep, ramp.shade, ramp.base, ramp.lit, ramp.shine})
                if (ink != ClearInk && ink < lab.size() && used[ink] && (steps.empty() || steps.back() != ink))
                    steps.push_back(ink);
            for (std::size_t step = 1; step < steps.size(); ++step) {
                const Ink darker = steps[step - 1];
                const Ink lighter = steps[step];
                if (!reported.insert({darker, lighter}).second) continue;
                const float rise = lab[lighter].l - lab[darker].l;
                if (rise >= options.rampStep) continue;
                const auto [image, pixels] = where(darker, lighter);
                const bool reversed = rise < 0.0f;
                findings.Add({"ramp-contrast", PixelatorLintSeverity::Warning, image, framesOf(image), pixels,
                              {darker, lighter},
                              reversed ? std::format("Ramp out of order: ink {} is darker than ink {} below it "
                                                     "(Oklab lightness {:.3f} < {:.3f}); shading reads backwards",
                                                     PixelatorInkLabel(asset, lighter), PixelatorInkLabel(asset, darker),
                                                     lab[lighter].l, lab[darker].l)
                                       : std::format("Ramp step too small: inks {} and {} differ by {:.3f} in Oklab "
                                                     "lightness (at least {:.3f} reads as a step); spread the tones "
                                                     "or merge them",
                                                     PixelatorInkLabel(asset, darker), PixelatorInkLabel(asset, lighter),
                                                     rise, options.rampStep)});
            }
        }
        return;
    }
    for (const auto& [inks, contact] : contacts) {
        if (contact.edges < options.similarEdges) continue;
        const auto [a, b] = inks;
        if (a >= lab.size() || b >= lab.size()) continue;
        const float lightness = std::abs(lab[a].l - lab[b].l);
        const float hue = std::hypot(lab[a].a - lab[b].a, lab[a].b - lab[b].b);
        if (lightness >= options.similarLightness || hue >= options.similarHue) continue;
        const auto [image, pixels] = where(a, b);
        findings.Add({"ramp-contrast", PixelatorLintSeverity::Warning, image, framesOf(image), pixels, {a, b},
                      std::format("Inks {} and {} touch along {} pixel edges but look almost the same (Oklab "
                                  "lightness {:.3f} apart); make them a visible step or one ink",
                                  PixelatorInkLabel(asset, a), PixelatorInkLabel(asset, b), contact.edges,
                                  lightness)});
    }
}
} // namespace

const char* PixelatorSeverityName(PixelatorLintSeverity severity) {
    return severity == PixelatorLintSeverity::Warning ? "warning" : "info";
}

std::size_t PixelatorWarnings(const std::vector<PixelatorFinding>& findings) {
    return static_cast<std::size_t>(std::ranges::count(findings, PixelatorLintSeverity::Warning,
                                                       &PixelatorFinding::severity));
}

int PixelatorColorBudget(int width, int height) {
    const int side = std::max(width, height);
    return side <= 16 ? 8 : side <= 32 ? 16 : side <= 64 ? 24 : side <= 128 ? 32 : 48;
}

std::vector<PixelatorFinding> LintPixelator(const PixelatorAsset& asset, const PixelatorLintOptions& options) {
    std::vector<PixelatorFinding> out;
    if (!asset.error.empty() || asset.frames.empty()) return out;
    Findings findings(out, options);
    std::vector<Sprite> sprites;
    for (std::size_t frame = 0; frame < asset.frames.size(); ++frame) {
        const auto& shown = asset.frames[frame];
        auto found = std::ranges::find(sprites, shown.image, &Sprite::image);
        if (found == sprites.end()) {
            sprites.push_back({shown.image, &asset.images[shown.image], {}, &asset.palettes[shown.palette]});
            found = sprites.end() - 1;
        }
        found->frames.push_back(frame);
    }
    const auto& palette = asset.palettes[asset.frames.front().palette];
    std::vector<OklabColor> lab;
    for (const Color4 color : palette)
        lab.push_back(ToOklab(color));
    std::array<bool, MaxInks> used{};
    for (const auto& sprite : sprites)
        for (const Ink ink : sprite.pixels->inks)
            used[ink] = true;

    for (const auto& sprite : sprites) {
        if (sprite.pixels->Empty()) continue;
        StrayPixels(sprite, lab, asset, options, findings);
        if (const auto outline = OutlineInk(Grid(*sprite.pixels), options)) {
            OutlineDoubles(sprite, *outline, findings);
            OutlineGaps(sprite, *outline, asset, options, findings);
        }
        std::array<bool, MaxInks> inSprite{};
        for (const Ink ink : sprite.pixels->inks)
            inSprite[ink] = true;
        std::set<std::uint32_t> colors;
        for (int ink = 1; ink < MaxInks; ++ink) {
            if (!inSprite[static_cast<std::size_t>(ink)] || static_cast<std::size_t>(ink) >= sprite.palette->size())
                continue;
            const Color4 color = (*sprite.palette)[static_cast<std::size_t>(ink)];
            colors.insert(static_cast<std::uint32_t>(color.r) << 24 | static_cast<std::uint32_t>(color.g) << 16 |
                          static_cast<std::uint32_t>(color.b) << 8 | color.a);
        }
        const int budget = PixelatorColorBudget(sprite.pixels->width, sprite.pixels->height);
        if (static_cast<int>(colors.size()) > budget)
            findings.Add({"color-count", PixelatorLintSeverity::Info, sprite.image, sprite.frames, {}, {},
                          std::format("{} colours in a {} x {} sprite; about {} keep a sprite this size readable",
                                      colors.size(), sprite.pixels->width, sprite.pixels->height, budget)});
    }
    RampContrast(sprites, lab, used, asset, options, findings);
    std::vector<int> unused;
    for (std::size_t ink = 1; ink < palette.size(); ++ink)
        if (!used[ink] && palette[ink].a > 0) unused.push_back(static_cast<int>(ink));
    if (!unused.empty()) {
        std::string named;
        for (const int ink : unused)
            named += (named.empty() ? "" : ", ") + PixelatorInkLabel(asset, ink);
        findings.Add({"unused-ink", PixelatorLintSeverity::Info, std::nullopt, {}, {}, unused,
                      std::format("Palette ink{} {} {} not used by any frame", unused.size() == 1 ? "" : "s", named,
                                  unused.size() == 1 ? "is" : "are")});
    }
    return out;
}
