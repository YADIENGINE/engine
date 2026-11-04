#include "Animation/Tween.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include "Animation/TweenSet.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

using Catch::Approx;

namespace {
enum class Part { Title, Glow, Footer };
struct Enemy {
    SecondsTween<float> scale{1.0f};
    SecondsTween<float> alpha{1.0f};
};
} // namespace

static_assert(std::is_trivially_copyable_v<StepTween<float>>); // No allocation, and cheap to keep in a vector
static_assert(std::is_trivially_copyable_v<StepTween<Float2>>);

TEST_CASE("Engine.Tween.GoesToItsTargetAndEndsExactly", "[engine][animation]") {
    SecondsTween<float> value{2.0f};
    CHECK_FALSE(value.Active());
    value.To(4.0f, 1.0f);
    CHECK(value.Active());
    CHECK(value.Value() == 2.0f);
    CHECK_FALSE(value.Update(0.5f));
    CHECK(value.Value() == Approx(3.0f));
    CHECK_FALSE(value.JustFinished());
    CHECK(value.Update(0.6f));
    CHECK(value.JustFinished());
    CHECK(value.Value() == 4.0f);
    CHECK_FALSE(value.Active());
    CHECK_FALSE(value.Update(0.1f));
    CHECK_FALSE(value.JustFinished());
    CHECK(value.Value() == 4.0f);
}
TEST_CASE("Engine.Tween.ReadsLikeThePlainValue", "[engine][animation]") {
    SecondsTween<float> scale{2.0f};
    CHECK(10.0f * scale == 20.0f);
    const float plain = scale;
    CHECK(plain == 2.0f);
    SecondsTween<Float2> offset{Float2{1.0f, 2.0f}};
    const Float2 moved = Float2{1.0f, 1.0f} + offset;
    CHECK(moved.y == 3.0f);
    CHECK(Mix(Float2{0.0f, 0.0f}, offset, 0.5f).x == Approx(0.5f));
}
TEST_CASE("Engine.Tween.CurveShapesTheWay", "[engine][animation]") {
    SecondsTween<float> value{0.0f};
    value.To(1.0f, 1.0f, Curve::OutQuad);
    value.Update(0.5f);
    CHECK(value.Value() == Approx(0.75f));
}
TEST_CASE("Engine.Tween.ChainsAndWaits", "[engine][animation]") {
    StepTween<float> value{1.0f};
    value.To(3.0f, 2).Wait(2).Then(1.0f, 2);
    CHECK_FALSE(value.Step());
    CHECK(value.Value() == 2.0f);
    CHECK_FALSE(value.Step());
    CHECK(value.Value() == 3.0f);
    CHECK_FALSE(value.Step());
    CHECK_FALSE(value.Step());
    CHECK(value.Value() == 3.0f); // Waiting
    CHECK_FALSE(value.Step());
    CHECK(value.Value() == 2.0f);
    CHECK(value.Step());
    CHECK(value.Value() == 1.0f);
}
TEST_CASE("Engine.Tween.ALongFrameCrossesSeveralParts", "[engine][animation]") {
    SecondsTween<float> value{0.0f};
    value.To(1.0f, 0.1f).Then(2.0f, 0.1f).Then(10.0f, 1.0f);
    CHECK_FALSE(value.Update(0.7f));
    CHECK(value.Value() == Approx(2.0f + 8.0f * 0.5f));
    CHECK(value.Update(9.0f));
    CHECK(value.Value() == 10.0f);
}
TEST_CASE("Engine.Tween.ANewToContinuesFromTheCurrentValue", "[engine][animation]") {
    StepTween<float> value{0.0f};
    value.To(10.0f, 10).Then(0.0f, 10);
    for (int i = 0; i < 5; ++i)
        value.Step();
    value.To(-5.0f, 10);
    CHECK(value.Value() == 5.0f); // No jump
    value.Step();
    CHECK(value.Value() == Approx(4.0f));
    for (int i = 0; i < 9; ++i)
        value.Step();
    CHECK(value.Value() == -5.0f);
    CHECK_FALSE(value.Active()); // The old chain's second part is gone
}
TEST_CASE("Engine.Tween.ThenAfterAnEndStartsANewChain", "[engine][animation]") {
    StepTween<float> value{0.0f};
    value.To(1.0f, 1);
    value.Step();
    value.Then(3.0f, 2);
    CHECK(value.Active());
    value.Step();
    CHECK(value.Value() == 2.0f);
}
TEST_CASE("Engine.Tween.ThenOnAnIdleTweenStartsFromItsValue", "[engine][animation]") {
    StepTween<float> value{5.0f};
    value.Then(7.0f, 2);
    value.Step();
    CHECK(value.Value() == 6.0f);
}
TEST_CASE("Engine.Tween.LoopNeverFinishes", "[engine][animation]") {
    StepTween<float> value{1.0f};
    value.To(2.0f, 2).Then(1.0f, 2).Loop();
    for (int i = 0; i < 100; ++i)
        CHECK_FALSE(value.Step());
    CHECK(value.Active());
    CHECK(value.Value() == 1.0f); // 100 is a whole number of rounds
    value.Step();
    CHECK(value.Value() == Approx(1.5f));
    CHECK_THROWS(value.Then(3.0f, 1));
    value.To(9.0f, 1); // To starts over, loop and all
    CHECK(value.Step());
    CHECK(value.Value() == 9.0f);
}
TEST_CASE("Engine.Tween.LoopOnSecondsCrossesRounds", "[engine][animation]") {
    SecondsTween<float> value{0.0f};
    value.To(1.0f, 0.5f).Wait(0.5f).Loop();
    CHECK_FALSE(value.Update(2.25f));
    CHECK(value.Value() == Approx(1.0f)); // Each round starts where the last ended: it is already at its target
}
TEST_CASE("Engine.Tween.StopAndSnap", "[engine][animation]") {
    StepTween<float> value{0.0f};
    value.To(10.0f, 10);
    value.Step();
    value.Stop();
    CHECK_FALSE(value.Active());
    CHECK(value.Value() == 1.0f);
    CHECK_FALSE(value.Step());
    CHECK(value.Value() == 1.0f);
    value.To(10.0f, 10);
    value.Snap(7.0f);
    CHECK_FALSE(value.Active());
    CHECK(value.Value() == 7.0f);
    CHECK(value.Value(0.0f) == 7.0f); // A jump: nothing is drawn across it
}
TEST_CASE("Engine.Tween.RefusesInvalidUse", "[engine][animation]") {
    SecondsTween<float> value;
    CHECK_THROWS(value.To(1.0f, -1.0f));
    CHECK_THROWS(value.To(1.0f, std::numeric_limits<float>::quiet_NaN()));
    CHECK_THROWS(value.To(1.0f, std::numeric_limits<float>::infinity()));
    CHECK_THROWS(value.Wait(-1.0f));
    value.To(1.0f, 1.0f).Then(2.0f, 1.0f).Then(3.0f, 1.0f).Then(4.0f, 1.0f);
    CHECK_THROWS(value.Then(5.0f, 1.0f)); // Longer sequences are what a clip is for
    SecondsTween<float> empty;
    CHECK_THROWS(empty.Loop());
    empty.To(1.0f, 0.0f);
    CHECK_THROWS(empty.Loop()); // It would never advance
}
TEST_CASE("Engine.Tween.NoTimeMovesNothing", "[engine][animation]") {
    SecondsTween<float> value{0.0f};
    value.To(1.0f, 1.0f);
    CHECK_FALSE(value.Update(0.0f));
    CHECK_FALSE(value.Update(-1.0f));
    CHECK(value.Value() == 0.0f);
    value.To(5.0f, 0.0f); // No duration: arrives at the next update
    CHECK(value.Update(0.001f));
    CHECK(value.Value() == 5.0f);
}
TEST_CASE("Engine.Tween.BetweenStepsFollowsStepSmoothed", "[engine][animation]") {
    StepTween<float> value{0.0f};
    value.To(4.0f, 4);
    value.Step();
    value.Step();
    CHECK(value.Value(0.0f) == Approx(1.0f));
    CHECK(value.Value(0.5f) == Approx(1.5f));
    CHECK(value.Value(1.0f) == 2.0f);
    value.Step();
    value.Step();
    CHECK(value.Value(0.5f) == Approx(3.5f));
    value.Step();
    CHECK(value.Value(0.5f) == 4.0f); // Standing still afterwards
}
TEST_CASE("Engine.Tween.WorksForOtherTypes", "[engine][animation]") {
    SecondsTween<Color4> color{Color4{0, 0, 0, 255}};
    color.To(Color4{200, 0, 0, 255}, 1.0f);
    color.Update(0.5f);
    CHECK(color.Value().r == 100);
    StepTween<int> frame{0};
    frame.To(10, 2);
    frame.Step();
    CHECK(frame.Value() == 10); // Whole values change at the middle
    frame = StepTween<int>{0};
    frame.Wait(1).Then(3, 0);
    CHECK(frame.Step());
    CHECK(frame.Value() == 3);
}
TEST_CASE("Engine.UpdateAll.AdvancesSeveralInOneLine", "[engine][animation]") {
    SecondsTween<float> a{0.0f};
    SecondsTween<Float2> b{Float2{}};
    a.To(1.0f, 1.0f);
    b.To(Float2{2.0f, 0.0f}, 1.0f);
    UpdateAll(0.5f, a, b);
    CHECK(a.Value() == Approx(0.5f));
    CHECK(b.Value().x == Approx(1.0f));
    StepTween<float> c{0.0f};
    c.To(2.0f, 2);
    UpdateAll(1, c);
    CHECK(c.Value() == 1.0f);
}
TEST_CASE("Engine.UpdateAll.ReachesFieldsOfAListThatChanges", "[engine][animation]") {
    std::vector<Enemy> enemies(2);
    enemies[0].scale.To(2.0f, 1.0f);
    enemies[1].alpha.To(0.0f, 1.0f);
    UpdateAll(0.5f, enemies, &Enemy::scale, &Enemy::alpha);
    CHECK(enemies[0].scale.Value() == Approx(1.5f));
    CHECK(enemies[1].alpha.Value() == Approx(0.5f));
    enemies.resize(1000); // Moves them: nothing here points at them
    UpdateAll(0.5f, enemies, &Enemy::scale);
    CHECK(enemies[0].scale.Value() == 2.0f);
    CHECK(enemies[0].scale.JustFinished());
    CHECK(enemies[999].scale.Value() == 1.0f);
}
TEST_CASE("Engine.TweenSet.StartsFromItsFallback", "[engine][animation]") {
    SecondsTweenSet<Part> tweens{1.0f};
    CHECK(tweens[Part::Title] == 1.0f); // Not seen yet
    CHECK_FALSE(tweens.Contains(Part::Title));
    tweens.To(Part::Title, 3.0f, 1.0f).Then(1.0f, 1.0f);
    CHECK(tweens.Contains(Part::Title));
    CHECK(tweens.Active(Part::Title));
    tweens.Update(0.5f);
    CHECK(tweens[Part::Title] == Approx(2.0f));
    CHECK(tweens[Part::Glow] == 1.0f);
}
TEST_CASE("Engine.TweenSet.ReportsWhatFinishedInKeyOrder", "[engine][animation]") {
    SecondsTweenSet<Part> tweens;
    tweens.To(Part::Footer, 1.0f, 0.5f);
    tweens.To(Part::Title, 1.0f, 0.5f);
    tweens.To(Part::Glow, 1.0f, 2.0f);
    std::vector<Part> finished;
    tweens.Update(1.0f, [&](Part part) { finished.push_back(part); });
    CHECK(finished == std::vector<Part>{Part::Title, Part::Footer});
    CHECK(tweens[Part::Title] == 1.0f); // It keeps its value
    finished.clear();
    tweens.Update(0.1f, [&](Part part) { finished.push_back(part); });
    CHECK(finished.empty());
}
TEST_CASE("Engine.TweenSet.ACallbackMayChangeTheSet", "[engine][animation]") {
    SecondsTweenSet<int> tweens;
    for (int id = 0; id < 5; ++id)
        tweens.To(id, 1.0f, 0.5f);
    tweens.Update(1.0f, [&](int id) {
        tweens.Remove(id);
        tweens.Remove(id + 1);
        tweens.To(100 + id, 1.0f, 1.0f);
    });
    CHECK_FALSE(tweens.Contains(0));
    CHECK(tweens.Contains(100));
    CHECK(tweens.Count() == 5);
}
TEST_CASE("Engine.TweenSet.SetRemoveAndClear", "[engine][animation]") {
    SecondsTweenSet<int> tweens{1.0f};
    tweens.Set(7, 5.0f);
    CHECK(tweens[7] == 5.0f);
    CHECK_FALSE(tweens.Active(7));
    tweens.Remove(7);
    CHECK(tweens[7] == 1.0f);
    tweens.Remove(7); // A gone key is no error
    tweens.Set(8, 2.0f);
    tweens.Clear();
    CHECK(tweens.Count() == 0);
}
TEST_CASE("Engine.TweenSet.OnStepsDrawsBetweenThem", "[engine][animation]") {
    StepTweenSet<Part, Float2> tweens{Float2{1.0f, 1.0f}};
    tweens.To(Part::Title, Float2{3.0f, 1.0f}, 2);
    int finished = 0;
    tweens.Step([&](Part) { ++finished; });
    CHECK(tweens.Value(Part::Title, 0.5f).x == Approx(1.5f));
    CHECK(tweens.Value(Part::Glow, 0.5f).x == 1.0f);
    tweens.Step([&](Part) { ++finished; });
    CHECK(finished == 1);
}
