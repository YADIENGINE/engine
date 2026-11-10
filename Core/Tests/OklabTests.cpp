#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "Utilities/Color/Oklab.h"

namespace {
void CheckOklab(Color4 color, float l, float a, float b) {
    const auto lab = ToOklab(color);
    CHECK(lab.l == Catch::Approx(l).margin(2e-3));
    CHECK(lab.a == Catch::Approx(a).margin(2e-3));
    CHECK(lab.b == Catch::Approx(b).margin(2e-3));
}
} // namespace

TEST_CASE("Engine.Color.OklabMatchesReferenceValues", "[engine]") {
    // sRGB primaries and white in Oklab, as the CSS Color 4 reference gives them.
    CheckOklab({255, 255, 255, 255}, 1.0f, 0.0f, 0.0f);
    CheckOklab({0, 0, 0, 255}, 0.0f, 0.0f, 0.0f);
    CheckOklab({255, 0, 0, 255}, 0.627955f, 0.224863f, 0.125846f);
    CheckOklab({0, 255, 0, 255}, 0.866440f, -0.233888f, 0.179498f);
    CheckOklab({0, 0, 255, 255}, 0.452014f, -0.032457f, -0.311528f);
    // A grey has no hue, and lightness rises with value.
    const auto grey = ToOklab({128, 128, 128, 255});
    CHECK(grey.a == Catch::Approx(0.0f).margin(1e-4));
    CHECK(grey.b == Catch::Approx(0.0f).margin(1e-4));
    CHECK(grey.l == Catch::Approx(0.5999f).margin(2e-3));
    CHECK(ToOklab({129, 128, 128, 255}).l > grey.l);

    for (const Color4 color : {Color4{20, 18, 28, 255}, Color4{240, 160, 48, 255}, Color4{77, 134, 168, 255}})
        CHECK(FromOklab(ToOklab(color)) == color);
    CHECK(FromOklab(ToOklab({10, 20, 30, 255}), 7).a == 7);
    CHECK(FromOklab({2.0f, 0.0f, 0.0f}) == Color4{255, 255, 255, 255}); // Clamped
    CHECK(OklabDistance(ToOklab({255, 0, 0, 255}), ToOklab({255, 0, 0, 255})) == 0.0f);
    CHECK(OklabDistance({0.5f, 0.0f, 0.0f}, {0.5f, 0.3f, 0.4f}) == Catch::Approx(0.5f));
}
