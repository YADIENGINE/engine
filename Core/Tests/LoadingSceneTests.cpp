#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include "Input/Input.h"
#include "Runtime/Internals/EngineLoadingScene.h"
#include "Scene/LoadingScene.h"
#include "Tests/PaintHarness.h"

namespace {
constexpr float Step = 0.01f;

class PlainLoading final : public LoadingScene {
public:
    [[nodiscard]] bool SeesLoaded() const { return Loaded(); }
};

DeviceInput KeyPress(InputKey key) {
    DeviceInput input;
    input.keysPressed[static_cast<std::size_t>(key)] = true;
    return input;
}

// A screen that is entered and shut down around the test's own ticks.
class Screen {
public:
    Screen()
        : scene({1280, 720}, ProjectSettings::LoadingScreen::Default) {
        scene.Activate();
    }
    ~Screen() { scene.Shutdown(); }
    // Ticks until the screen finishes, or for at most `limit` seconds; returns the seconds that passed.
    float Run(float limit) {
        float seconds = 0.0f;
        while (seconds < limit && !scene.Finished()) {
            scene.Tick(Step);
            seconds += Step;
        }
        return seconds;
    }
    EngineLoadingScene scene;
};
} // namespace

TEST_CASE("Engine.LoadingScene.RunsOnRealTimeAndLearnsFromTheHostThatSoundsAreReady", "[engine][scene]") {
    PlainLoading scene;
    CHECK(scene.Clock() == TimeDomain::Real);
    CHECK_FALSE(scene.SeesLoaded());
    CHECK(scene.Finished());
    scene.MarkLoaded();
    CHECK(scene.SeesLoaded());
}

TEST_CASE("Engine.EngineLoadingScene.EveryLookDrawsWithoutAFontInAnyCanvasShape", "[engine][scene]") {
    for (const Vec2 canvas : {Vec2{1280, 720}, Vec2{720, 1280}, Vec2{320, 320}}) {
        for (std::size_t look = 0; look < ProjectSettings::LoadingScreenNames.size(); ++look) {
            CAPTURE(ProjectSettings::LoadingScreenNames[look], canvas.x, canvas.y);
            EngineLoadingScene scene(canvas, static_cast<ProjectSettings::LoadingScreen>(look));
            scene.Activate();
            for (const float seconds : {0.1f, 1.0f, 25.0f}) {
                scene.Tick(seconds);
                PaintHarness harness({static_cast<float>(canvas.x), static_cast<float>(canvas.y)}, 1.0f);
                harness.Begin();
                REQUIRE_FALSE(harness.paint.HasFont());
                CHECK_NOTHROW(scene.Render(harness.paint));
                harness.End();
                CHECK_FALSE(harness.Draws().empty());
            }
            scene.Shutdown();
        }
    }
}

TEST_CASE("Engine.EngineLoadingScene.LastsTwoSecondsWhenTheSoundsAreAlreadyReady", "[engine][scene]") {
    Screen screen;
    screen.scene.MarkLoaded();
    CHECK(screen.Run(5.0f) == Catch::Approx(2.0f).margin(0.05f));
}

TEST_CASE("Engine.EngineLoadingScene.WaitsInItsHoldWhileTheSoundsLoadThenFades", "[engine][scene]") {
    Screen screen;
    CHECK(screen.Run(10.0f) == Catch::Approx(10.0f).margin(0.05f));
    CHECK_FALSE(screen.scene.Finished());
    CHECK(screen.scene.Visibility() == Catch::Approx(1.0f));
    screen.scene.MarkLoaded();
    CHECK(screen.Run(5.0f) == Catch::Approx(0.4f).margin(0.05f));
    CHECK(screen.scene.Finished());
    CHECK(screen.scene.Visibility() == Catch::Approx(0.0f));
}

TEST_CASE("Engine.EngineLoadingScene.IgnoresAPressBeforeTheSoundsAreReady", "[engine][scene]") {
    Screen screen;
    screen.scene.ReadInput(KeyPress(InputKey::Space));
    screen.scene.MarkLoaded();
    CHECK(screen.Run(5.0f) == Catch::Approx(2.0f).margin(0.05f));
}

TEST_CASE("Engine.EngineLoadingScene.AnyPressAfterTheSoundsAreReadyFadesInAFifthOfASecond", "[engine][scene]") {
    const auto skipped = [](const DeviceInput& input) {
        Screen screen;
        screen.scene.MarkLoaded();
        (void)screen.Run(0.8f);
        screen.scene.ReadInput(input);
        CHECK(screen.Run(5.0f) == Catch::Approx(0.2f).margin(0.05f));
    };
    SECTION("a key") { skipped(KeyPress(InputKey::Escape)); }
    SECTION("a mouse button") { skipped(KeyPress(InputKey::MouseRight)); }
    SECTION("a touch") {
        DeviceInput input;
        input.pressPoints.push_back({10, 10});
        skipped(input);
    }
}

TEST_CASE("Engine.EngineLoadingScene.ASkipDuringTheFadeInDarkensFromWhereTheFadeInReached", "[engine][scene]") {
    Screen screen;
    screen.scene.MarkLoaded();
    (void)screen.Run(0.2f);
    const float reached = screen.scene.Visibility();
    CHECK(reached == Catch::Approx(0.5f).margin(0.05f));
    screen.scene.ReadInput(KeyPress(InputKey::Enter));
    CHECK(screen.scene.Visibility() == Catch::Approx(reached));
    screen.scene.Tick(0.1f);
    CHECK(screen.scene.Visibility() == Catch::Approx(reached * 0.5f).margin(0.01f));
}
