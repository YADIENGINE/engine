#include <catch2/catch_approx.hpp>
#include <stdexcept>
#include "Tests/Support/Diagnostics.h"
#include "Utilities/UiTransform.h"

TEST_CASE("Engine.UiTransform.IdentityAndCenteredAspectFit", "[engine][ui]") {
    const UiTransform identity({10, 20, 100, 50}, {10, 20, 100, 50});
    CHECK(identity.Scale() == 1.0f);
    CHECK(identity.Offset().x == 0.0f);
    CHECK(identity.Offset().y == 0.0f);
    CHECK(identity.Map(Vec2{13, 27}) == Vec2{13, 27});
    CHECK(identity.Unmap(Vec2{13, 27}) == Vec2{13, 27});

    const UiTransform wide({10, 20, 100, 50}, {200, 300, 300, 100});
    CHECK(wide.Scale() == 2.0f);
    CHECK(wide.Offset().x == 230.0f);
    CHECK(wide.Offset().y == 260.0f);
    CHECK(wide.Map(Vec2{10, 20}) == Vec2{250, 300});
    CHECK(wide.Map(Vec2{110, 70}) == Vec2{450, 400});
    CHECK(wide.Unmap(Vec2{350, 350}) == Vec2{60, 45});

    const UiTransform tall({0, 0, 100, 50}, {10, 20, 50, 100});
    CHECK(tall.Scale() == 0.5f);
    CHECK(tall.Map(Vec2{0, 0}) == Vec2{10, 58});
    CHECK(tall.Map(Vec2{100, 50}) == Vec2{60, 83});
}

TEST_CASE("Engine.UiTransform.OddCentersFractionalInverseAndRounding", "[engine][ui]") {
    const UiTransform odd({11, -7, 5, 3}, {100, 200, 9, 9});
    CHECK(odd.Scale() == Catch::Approx(1.8f));
    const Float2 center = odd.Map(Float2{13.0f, -6.0f});
    CHECK(center.x == Catch::Approx(104.0f));
    CHECK(center.y == Catch::Approx(204.0f));
    const Float2 mapped = odd.Map(Float2{11.25f, -5.75f});
    CHECK(mapped.x == Catch::Approx(100.85f));
    CHECK(mapped.y == Catch::Approx(204.45f));
    const Float2 local = odd.Unmap(mapped);
    CHECK(local.x == Catch::Approx(11.25f));
    CHECK(local.y == Catch::Approx(-5.75f));

    const UiTransform halves({0, 0, 4, 4}, {-1, -1, 2, 2});
    CHECK(halves.Map(Vec2{1, 3}) == Vec2{-1, 1});
    const UiTransform doubled({0, 0, 2, 2}, {0, 0, 4, 4});
    CHECK(doubled.Unmap(Vec2{-1, 1}) == Vec2{-1, 1});
}

TEST_CASE("Engine.UiTransform.RejectsEmptyOrNegativeRectangles", "[engine][ui]") {
    constexpr Rect Valid{0, 0, 10, 20};
    for (const Rect invalid : {Rect{0, 0, 0, 20}, Rect{0, 0, 10, 0}, Rect{0, 0, -1, 20}, Rect{0, 0, 10, -1}}) {
        CHECK_THROWS_AS(UiTransform(invalid, Valid), std::invalid_argument);
        CHECK_THROWS_AS(UiTransform(Valid, invalid), std::invalid_argument);
    }
}
