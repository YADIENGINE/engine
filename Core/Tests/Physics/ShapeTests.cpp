#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "Physics/Collision/Shape.h"

TEST_CASE("Engine.Physics.Shape.KeepsWhatItWasMadeOf", "[engine][physics]") {
    const auto circle = Shape::Circle(5.0f, {1.0f, 2.0f});
    CHECK(circle.Kind() == ShapeKind::Circle);
    CHECK(circle.Radius() == 5.0f);
    CHECK(circle.Points()[0] == Float2{1.0f, 2.0f});

    const auto box = Shape::Box(3.0f, 2.0f, {10.0f, 0.0f});
    REQUIRE(box.Points().size() == 4);
    CHECK(box.Points()[0] == Float2{7.0f, -2.0f});
    CHECK(box.Points()[2] == Float2{13.0f, 2.0f});
    CHECK(box.Radius() == 0.0f);

    CHECK(Shape::Capsule({0.0f, 0.0f}, {4.0f, 0.0f}, 1.0f).Points().size() == 2);
    CHECK(Shape::Segment({0.0f, 0.0f}, {4.0f, 0.0f}).Radius() == 0.0f);
}
TEST_CASE("Engine.Physics.Shape.RefusesSizesThatMeanNothing", "[engine][physics]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(Shape::Circle(0.0f), std::invalid_argument);
    CHECK_THROWS_AS(Shape::Circle(-1.0f), std::invalid_argument);
    CHECK_THROWS_AS(Shape::Circle(nan), std::invalid_argument);
    CHECK_THROWS_AS(Shape::Box(0.0f, 1.0f), std::invalid_argument);
    CHECK_THROWS_AS(Shape::Capsule({0.0f, 0.0f}, {0.0f, 0.0f}, 1.0f), std::invalid_argument);
    CHECK_THROWS_AS(Shape::Segment({1.0f, 1.0f}, {1.0f, 1.0f}), std::invalid_argument);
}
TEST_CASE("Engine.Physics.Shape.PolygonsAreConvexInEitherDirection", "[engine][physics]") {
    constexpr std::array<Float2, 4> clockwise{Float2{0, 0}, Float2{10, 0}, Float2{10, 10}, Float2{0, 10}};
    constexpr std::array<Float2, 4> counter{Float2{0, 10}, Float2{10, 10}, Float2{10, 0}, Float2{0, 0}};
    CHECK(Shape::Polygon(clockwise).Points().size() == 4);
    CHECK(Shape::Polygon(counter).Points().size() == 4);

    constexpr std::array<Float2, 4> concave{Float2{0, 0}, Float2{10, 0}, Float2{2, 2}, Float2{0, 10}};
    CHECK_THROWS_AS(Shape::Polygon(concave), std::invalid_argument);
    constexpr std::array<Float2, 3> line{Float2{0, 0}, Float2{5, 0}, Float2{10, 0}};
    CHECK_THROWS_AS(Shape::Polygon(line), std::invalid_argument);
    constexpr std::array<Float2, 2> two{Float2{0, 0}, Float2{5, 0}};
    CHECK_THROWS_AS(Shape::Polygon(two), std::invalid_argument);
    std::array<Float2, 9> nine{};
    for (std::size_t i = 0; i < nine.size(); ++i)
        nine[i] = Float2::FromAngle(static_cast<float>(i) * 0.7f, 10.0f);
    CHECK_THROWS_AS(Shape::Polygon(nine), std::invalid_argument);
}
TEST_CASE("Engine.Physics.Shape.RefusesAPolygonTooSmallToCollide", "[engine][physics]") {
    constexpr std::array<Float2, 3> tiny{Float2{0, 0}, Float2{0.001f, 0}, Float2{0, 0.001f}};
    CHECK_THROWS_AS(Shape::Polygon(tiny), std::invalid_argument);
}
