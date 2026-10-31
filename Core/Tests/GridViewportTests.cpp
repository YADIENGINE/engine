#include <catch2/catch_approx.hpp>
#include <limits>
#include <stdexcept>
#include "Tests/Support/Diagnostics.h"
#include "Menus/GridViewport.h"

namespace {
constexpr GridViewportSettings Settings{.bounds = {10, 20, 70, 45},
                                        .columns = 3,
                                        .visibleRows = 2,
                                        .tileWidth = 20,
                                        .tileHeight = 20,
                                        .gapX = 5,
                                        .gapY = 5,
                                        .scrollSpeed = 10.0f};
} // namespace

TEST_CASE("Engine.GridViewport.FollowsSelectionOnlyOutsideVisibleRows", "[engine][graphics]") {
    GridViewport viewport(Settings);
    viewport.Update(10, 3, 0.05f);
    CHECK(viewport.ScrollRows() == 0.0f);
    viewport.Update(10, 6, 0.05f);
    CHECK(viewport.ScrollRows() == Catch::Approx(0.5f));
    CHECK(viewport.TileArea(6).y == 58);
    CHECK(viewport.TileArea(0).y == 8);
    viewport.Update(10, 3, 0.05f);
    CHECK(viewport.ScrollRows() == Catch::Approx(0.5f));
    viewport.Update(10, 6, 0.1f);
    CHECK(viewport.ScrollRows() == 1.0f);
    viewport.Update(10, 0, 0.05f);
    CHECK(viewport.ScrollRows() == Catch::Approx(0.5f));
}

TEST_CASE("Engine.GridViewport.ResetsClampsAndHandlesChangingCounts", "[engine][graphics]") {
    GridViewport viewport(Settings);
    CHECK(viewport.RowCount(10) == 4);
    CHECK(viewport.RowCount(9) == 3);
    CHECK(viewport.RowCount(0) == 0);
    CHECK(viewport.RowCount(-1) == 0);
    viewport.Reset(10, 9);
    CHECK(viewport.ScrollRows() == 2.0f);
    CHECK(viewport.TileArea(9).y == 45);
    viewport.Reset(10, 100);
    CHECK(viewport.ScrollRows() == 2.0f);
    viewport.Update(4, 3, 0.0f);
    CHECK(viewport.ScrollRows() == 0.0f);
    viewport.Reset(10, -1);
    CHECK(viewport.ScrollRows() == 0.0f);
    viewport.Reset(10, 9);
    viewport.Update(0, -1, 0.1f);
    CHECK(viewport.ScrollRows() == 0.0f);
    CHECK_FALSE(viewport.TileAt({10, 20}, 0));
    viewport.Reset(-1, 9);
    CHECK(viewport.ScrollRows() == 0.0f);
}

TEST_CASE("Engine.GridViewport.HitsCurrentCountAndRejectsGapsAndEdges", "[engine][graphics]") {
    GridViewport viewport(Settings);
    viewport.Reset(3, 0);
    CHECK(viewport.TileAt({10, 20}, 3) == 0);
    CHECK(viewport.TileAt({79, 39}, 3) == 2);
    CHECK_FALSE(viewport.TileAt({30, 20}, 3));
    CHECK_FALSE(viewport.TileAt({10, 40}, 3));
    CHECK_FALSE(viewport.TileAt({80, 20}, 3));
    CHECK_FALSE(viewport.TileAt({10, 65}, 10));
    CHECK_FALSE(viewport.TileAt({10, 19}, 10));
    CHECK_FALSE(viewport.TileAt({10, 45}, 3));
    CHECK(viewport.TileAt({10, 45}, 4) == 3);
    CHECK_FALSE(viewport.TileAt({35, 45}, 4));
    viewport.Update(10, 6, 0.05f);
    CHECK(viewport.TileAt({10, 20}, 10) == 0);
    CHECK_FALSE(viewport.TileAt({10, 28}, 10));
    CHECK_FALSE(viewport.TileAt({10, 57}, 10));
    CHECK(viewport.TileAt({10, 58}, 10) == 6);
    CHECK_FALSE(viewport.TileAt({10, 58}, 6));
}

TEST_CASE("Engine.GridViewport.ConfiguresIndependentGeometryAndImmediateScrolling", "[engine][graphics]") {
    GridViewport viewport({.bounds = {100, 200, 51, 41},
                           .columns = 2,
                           .visibleRows = 2,
                           .tileWidth = 24,
                           .tileHeight = 18,
                           .gapX = 3,
                           .gapY = 5});
    const FloatRect tile = viewport.TileArea(3);
    CHECK(tile.x == 127);
    CHECK(tile.y == 223);
    CHECK(tile.width == 24);
    CHECK(tile.height == 18);
    viewport.Update(7, 6, 0.001f);
    CHECK(viewport.ScrollRows() == 2.0f);
    CHECK(viewport.TileArea(6).y == 223);
    CHECK_FALSE(viewport.TileAt({127, 223}, 7));
}

TEST_CASE("Engine.GridViewport.RejectsInvalidSettingsAndSkipsInvalidAnimationDeltas", "[engine][graphics]") {
    GridViewport viewport(Settings);
    for (float delta : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        viewport.Update(10, 9, delta);
        CHECK(viewport.ScrollRows() == 0.0f);
    }
    auto invalid = Settings;
    invalid.columns = 0;
    CHECK_THROWS_AS(GridViewport(invalid), std::invalid_argument);
    invalid = Settings;
    invalid.tileHeight = 0;
    CHECK_THROWS_AS(GridViewport(invalid), std::invalid_argument);
    invalid = Settings;
    invalid.gapX = -1;
    CHECK_THROWS_AS(GridViewport(invalid), std::invalid_argument);
    invalid = Settings;
    invalid.scrollSpeed = std::numeric_limits<float>::infinity();
    CHECK_THROWS_AS(GridViewport(invalid), std::invalid_argument);
    invalid = Settings;
    invalid.tileWidth = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(GridViewport(invalid), std::invalid_argument);
}

TEST_CASE("Engine.GridViewport.KeepsFloatSizesAndConvertsIntegerPointerInput", "[engine][menus]") {
    GridViewport viewport({.bounds = {10.25f, 20.0f, 50.5f, 40.5f},
                           .columns = 2,
                           .visibleRows = 2,
                           .tileWidth = 20.5f,
                           .tileHeight = 18.25f,
                           .gapX = 4.25f,
                           .gapY = 2.0f});
    const FloatRect tile = viewport.TileArea(1);
    CHECK(tile.x == Catch::Approx(35.0f));
    CHECK(tile.width == Catch::Approx(20.5f));
    CHECK(tile.height == Catch::Approx(18.25f));
    CHECK(viewport.TileAt({35, 20}, 4) == 1);
    CHECK_FALSE(viewport.TileAt({31, 20}, 4));
    CHECK_FALSE(viewport.TileAt({10, 20}, 4));
}
