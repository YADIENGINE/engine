#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>
#include "Graphics/Backgrounds/Clouds.h"
#include "Graphics/Backgrounds/Mountains.h"
#include "Graphics/Backgrounds/Nebula.h"
#include "Graphics/Backgrounds/Patterns.h"
#include "Graphics/Backgrounds/Sea.h"
#include "Graphics/Backgrounds/Sky.h"
#include "Graphics/Backgrounds/Skyline.h"
#include "Graphics/Backgrounds/StarField.h"
#include "Graphics/Backgrounds/SynthwaveHorizon.h"
#include "Tests/PaintHarness.h"

namespace {
using BackgroundDraw = std::function<void(Painter&, const FloatRect&, float)>;

// Every item of every background set, each with its own look.
std::vector<BackgroundDraw> AllBackgrounds() {
    std::vector<BackgroundDraw> all;
    for (const SkyKind kind : {SkyKind::Day, SkyKind::Sunset, SkyKind::Night}) {
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawSky(p, b, kind, t); });
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawClouds(p, b, kind, t); });
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawMountains(p, b, kind, t); });
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawSea(p, b, kind, t); });
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawSkyline(p, b, kind, t); });
    }
    for (const StarFieldKind kind : {StarFieldKind::Still, StarFieldKind::Drift, StarFieldKind::Warp})
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawStarField(p, b, kind, t); });
    for (const NebulaKind kind : {NebulaKind::Violet, NebulaKind::Teal, NebulaKind::Ember})
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawNebula(p, b, kind, t); });
    for (const SynthwaveKind kind : {SynthwaveKind::Magenta, SynthwaveKind::Cyan, SynthwaveKind::Ember})
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawSynthwaveHorizon(p, b, kind, t); });
    for (const int item : {0, 1, 2, 3}) {
        const auto kind = static_cast<PatternKind>(item);
        all.push_back([kind](Painter& p, const FloatRect& b, float t) { DrawPattern(p, b, kind, t); });
    }
    return all;
}

std::vector<PaintVertex> Vertices(const BackgroundDraw& draw, const FloatRect& box, float time, float scale = 2.0f) {
    PaintHarness h({400.0f, 300.0f}, scale);
    h.Begin();
    draw(h.paint, box, time);
    h.End();
    std::vector<PaintVertex> vertices;
    const auto draws = h.Draws();
    for (std::size_t i = 0; i < draws.size(); ++i) {
        const auto drawn = h.Drawn(i);
        vertices.insert(vertices.end(), drawn.begin(), drawn.end());
    }
    return vertices;
}

bool SameVertices(const std::vector<PaintVertex>& a, const std::vector<PaintVertex>& b) {
    return a.size() == b.size() && std::ranges::equal(a, b, [](const PaintVertex& x, const PaintVertex& y) {
               return x.x == y.x && x.y == y.y && x.color == y.color;
           });
}
} // namespace

TEST_CASE("Engine.Backgrounds.EveryItemDrawsAtAnyShapeAndSize", "[engine][paint]") {
    // Wide, tall and square, down to a small thumbnail. A Debug build throws on any rejected Painter call.
    constexpr std::array Boxes{FloatRect{0.0f, 0.0f, 400.0f, 225.0f}, FloatRect{20.0f, 10.0f, 90.0f, 280.0f},
                               FloatRect{30.0f, 40.0f, 48.0f, 48.0f}, FloatRect{0.0f, 100.0f, 400.0f, 30.0f}};
    const auto all = AllBackgrounds();
    for (std::size_t index = 0; index < all.size(); ++index)
        for (const FloatRect& box : Boxes) {
            CAPTURE(index, box.width, box.height);
            const auto vertices = Vertices(all[index], box, 2.5f);
            CHECK_FALSE(vertices.empty());
            CHECK(std::ranges::all_of(vertices, [](const PaintVertex& vertex) {
                return std::isfinite(vertex.x) && std::isfinite(vertex.y);
            }));
        }
}

TEST_CASE("Engine.Backgrounds.AnEmptyBoxDrawsNothing", "[engine][paint]") {
    for (const auto& draw : AllBackgrounds()) {
        CHECK(Vertices(draw, {10.0f, 10.0f, 0.0f, 50.0f}, 1.0f).empty());
        CHECK(Vertices(draw, {10.0f, 10.0f, 50.0f, 0.0f}, 1.0f).empty());
    }
}

TEST_CASE("Engine.Backgrounds.TheSameTimeDrawsTheSameFrame", "[engine][paint]") {
    constexpr FloatRect Box{0.0f, 0.0f, 400.0f, 225.0f};
    for (const auto& draw : AllBackgrounds()) CHECK(SameVertices(Vertices(draw, Box, 4.0f), Vertices(draw, Box, 4.0f)));
}

TEST_CASE("Engine.Backgrounds.AnimatedItemsMoveWithTime", "[engine][paint]") {
    constexpr FloatRect Box{0.0f, 0.0f, 400.0f, 225.0f};
    const auto moves = [&](const BackgroundDraw& draw) {
        return !SameVertices(Vertices(draw, Box, 1.0f), Vertices(draw, Box, 2.0f));
    };
    CHECK(moves([](Painter& p, const FloatRect& b, float t) { DrawStarField(p, b, StarFieldKind::Drift, t); }));
    CHECK(moves([](Painter& p, const FloatRect& b, float t) { DrawSea(p, b, SkyKind::Day, t); }));
    CHECK(moves([](Painter& p, const FloatRect& b, float t) { DrawClouds(p, b, SkyKind::Day, t); }));
    CHECK(moves([](Painter& p, const FloatRect& b, float t) { DrawSynthwaveHorizon(p, b, SynthwaveKind::Cyan, t); }));
    CHECK(moves([](Painter& p, const FloatRect& b, float t) { DrawPattern(p, b, PatternKind::DotWave, t); }));
}

TEST_CASE("Engine.Backgrounds.ScenerySkipsItsSkyWhenLaidOverAnother", "[engine][paint]") {
    constexpr FloatRect Box{0.0f, 0.0f, 400.0f, 225.0f};
    const auto count = [&](const BackgroundDraw& draw) { return Vertices(draw, Box, 1.0f).size(); };
    const BackgroundDraw sky = [](Painter& p, const FloatRect& b, float t) { DrawSky(p, b, SkyKind::Night, t); };
    CHECK(count([](Painter& p, const FloatRect& b, float t) { DrawClouds(p, b, SkyKind::Night, t, {.sky = false}); }) +
              count(sky) ==
          count([](Painter& p, const FloatRect& b, float t) { DrawClouds(p, b, SkyKind::Night, t); }));
    const BackgroundDraw ridges = [](Painter& p, const FloatRect& b, float t) {
        DrawMountains(p, b, SkyKind::Night, t, {.sky = false});
    };
    CHECK(count(ridges) + count(sky) ==
          count([](Painter& p, const FloatRect& b, float t) { DrawMountains(p, b, SkyKind::Night, t); }));
}

TEST_CASE("Engine.Backgrounds.ALookChangesOnlyWhatItNames", "[engine][paint]") {
    constexpr FloatRect Box{0.0f, 0.0f, 400.0f, 225.0f};
    constexpr Color4 Named{1, 200, 3, 255};
    const auto shows = [&](const BackgroundDraw& draw) {
        const auto vertices = Vertices(draw, Box, 1.0f);
        // Opaque, so premultiplying leaves it as it is.
        return std::ranges::any_of(vertices, [](const PaintVertex& vertex) {
            return vertex.color.r == Named.r && vertex.color.g == Named.g && vertex.color.b == Named.b &&
                   vertex.color.a == Named.a;
        });
    };
    CHECK_FALSE(shows([](Painter& p, const FloatRect& b, float t) { DrawPattern(p, b, PatternKind::Dots, t); }));
    CHECK(shows([](Painter& p, const FloatRect& b, float t) {
        DrawPattern(p, b, PatternKind::Dots, t, {.mark = Named});
    }));
    // A colour left out keeps the kind's own, so the two pictures still share their background.
    const BackgroundDraw own = [](Painter& p, const FloatRect& b, float t) { DrawPattern(p, b, PatternKind::Dots, t); };
    const BackgroundDraw changed = [](Painter& p, const FloatRect& b, float t) {
        DrawPattern(p, b, PatternKind::Dots, t, {.mark = Named});
    };
    const auto first = Vertices(own, Box, 1.0f);
    const auto second = Vertices(changed, Box, 1.0f);
    REQUIRE(first.size() == second.size());
    CHECK(first.front().color == second.front().color);
}
