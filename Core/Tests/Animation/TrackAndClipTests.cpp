#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <vector>
#include "Animation/Clip.h"
#include "Animation/ClipSet.h"
#include "Animation/Curve.h"
#include "Animation/PoseFields.h"
#include "Animation/Track.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

using Catch::Approx;

namespace {
enum class Frame { Stand, StepLeft, StepRight };
struct TestPose {
    float arm = 0.5f;
    Float2 offset{};
    Color4 tint{10, 20, 30, 255};
    int frame = 0;
    Frame mode = Frame::Stand;
    static constexpr auto Fields() {
        return PoseFields(PoseField("arm", &TestPose::arm), PoseField("offset", &TestPose::offset),
                          PoseField("tint", &TestPose::tint), PoseField("frame", &TestPose::frame),
                          PoseField("mode", &TestPose::mode));
    }
};
struct FieldNames {
    std::vector<std::string> seen;
    void Field(std::string_view name, const auto&) { seen.emplace_back(name); }
};
std::vector<std::string> Met(const StepClip<TestPose>& clip, int before, int after, bool includeAfter = false) {
    std::vector<std::string> names;
    clip.ForEachCrossed(before, after, [&](std::string_view name) { names.emplace_back(name); }, includeAfter);
    return names;
}
} // namespace

TEST_CASE("Engine.Curve.MatchesTheFunctionsAndEndsExactly", "[engine][animation]") {
    CHECK(Ease(Curve::OutBack, 0.3f) == EaseOutBack(0.3f));
    CHECK(Ease(Curve::InOutSine, 0.3f) == EaseInOutSine(0.3f));
    CHECK(Ease(Curve::OutBounce, 0.3f) == EaseOutBounce(0.3f));
    for (int value = 0; value <= static_cast<int>(Curve::Hold); ++value) {
        const auto curve = static_cast<Curve>(value);
        INFO(value);
        CHECK(Ease(curve, 1.0f) == Approx(1.0f));
        CHECK(Ease(curve, 0.0f) == Approx(0.0f).margin(1e-6));
    }
    CHECK(Ease(Curve::Hold, 0.99f) == 0.0f);
}
TEST_CASE("Engine.Track.RefusesInvalidKeys", "[engine][animation]") {
    using Keys = std::vector<TrackKey<float, int>>;
    CHECK_THROWS(StepTrack<float>(Keys{}));
    CHECK_THROWS(StepTrack<float>{{-1, 0.0f}});
    CHECK_THROWS(StepTrack<float>{{10, 0.0f}, {5, 1.0f}});
    CHECK_THROWS(SecondsTrack<float>{{std::numeric_limits<float>::quiet_NaN(), 0.0f}});
    CHECK_THROWS(SecondsTrack<float>{{std::numeric_limits<float>::infinity(), 0.0f}});
    CHECK_NOTHROW(StepTrack<float>{{5, 0.0f}, {5, 1.0f}});
}
TEST_CASE("Engine.Track.PassesThroughItsKeys", "[engine][animation]") {
    const StepTrack<float> track{{10, 1.0f}, {20, 3.0f}, {40, -1.0f, Curve::OutQuad}};
    CHECK(track.Length() == 40);
    CHECK(track.At(0) == 1.0f); // Before the first key
    CHECK(track.At(10) == 1.0f);
    CHECK(track.At(15) == Approx(2.0f));
    CHECK(track.At(20) == 3.0f);
    CHECK(track.At(30) == Approx(3.0f - 4.0f * EaseOutQuad(0.5f))); // The curve belongs to the way to its key
    CHECK(track.At(40) == -1.0f);
    CHECK(track.At(99) == -1.0f);
}
TEST_CASE("Engine.Track.BetweenStepsFollowsStepSmoothed", "[engine][animation]") {
    const StepTrack<float> track{{0, 0.0f}, {10, 10.0f}};
    CHECK(track.At(5, 0.0f) == track.At(4));
    CHECK(track.At(5, 1.0f) == track.At(5));
    CHECK(track.At(5, 0.5f) == Approx(4.5f));
    CHECK(track.At(0, 0.5f) == 0.0f); // Nothing has run yet
}
TEST_CASE("Engine.Track.KeysAtOnePositionJump", "[engine][animation]") {
    const StepTrack<float> track{{0, 0.0f}, {10, 1.0f}, {10, 5.0f}, {20, 6.0f}};
    CHECK(track.At(9) == Approx(0.9f));
    CHECK(track.At(10) == 5.0f);
    CHECK(track.At(15) == Approx(5.5f));
}
TEST_CASE("Engine.Track.HoldCurveAndWholeValuesHold", "[engine][animation]") {
    const StepTrack<float> held{{0, 1.0f}, {10, 2.0f, Curve::Hold}};
    CHECK(held.At(9) == 1.0f);
    CHECK(held.At(10) == 2.0f);
    const StepTrack<int> frames{{0, 0}, {5, 1}, {10, 2}};
    CHECK(frames.At(4) == 0);
    CHECK(frames.At(5) == 1);
    CHECK(frames.At(9) == 1);
    CHECK(frames.At(10) == 2);
    const StepTrack<Frame> modes{{0, Frame::Stand}, {5, Frame::StepLeft}};
    CHECK(modes.At(4) == Frame::Stand);
    CHECK(modes.At(5) == Frame::StepLeft);
}
TEST_CASE("Engine.Track.WorksForEveryMixableType", "[engine][animation]") {
    const SecondsTrack<Float2> path{{0.0f, Float2{0.0f, 0.0f}}, {1.0f, Float2{10.0f, 20.0f}}};
    CHECK(path.At(0.5f).x == Approx(5.0f));
    CHECK(path.At(0.5f).y == Approx(10.0f));
    const SecondsTrack<Color4> color{{0.0f, Color4{0, 0, 0, 255}}, {1.0f, Color4{200, 100, 0, 255}}};
    CHECK(color.At(0.5f).r == 100);
    CHECK(color.At(1.0f).g == 100);
    const SecondsTrack<double> precise{{0.0f, 0.0}, {2.0f, 1.0}};
    CHECK(precise.At(1.0f) == Approx(0.5));
}
TEST_CASE("Engine.Pose.MixesAndVisitsEveryField", "[engine][animation]") {
    const TestPose a{.arm = 0.0f, .offset = {0.0f, 2.0f}, .tint = {0, 0, 0, 255}, .frame = 1, .mode = Frame::Stand};
    const TestPose b{.arm = 1.0f, .offset = {4.0f, 2.0f}, .tint = {100, 0, 0, 255}, .frame = 3, .mode = Frame::StepRight};
    const TestPose early = MixPose(a, b, 0.25f);
    CHECK(early.arm == Approx(0.25f));
    CHECK(early.offset.x == Approx(1.0f));
    CHECK(early.tint.r == 25);
    CHECK(early.frame == 1); // Whole values change at the middle of a fade
    CHECK(early.mode == Frame::Stand);
    const TestPose late = MixPose(a, b, 0.75f);
    CHECK(late.frame == 3);
    CHECK(late.mode == Frame::StepRight);

    FieldNames names;
    VisitPose(names, a);
    CHECK(names.seen == std::vector<std::string>{"arm", "offset", "tint", "frame", "mode"});
}
TEST_CASE("Engine.Clip.SamplesItsTracksOverTheRestPose", "[engine][animation]") {
    const auto clip = StepClip<TestPose>{}
                          .Track(&TestPose::offset, {{0, Float2{0.0f, 0.0f}}, {10, Float2{10.0f, 0.0f}}})
                          .Track(&TestPose::frame, {{0, 0}, {5, 7}});
    CHECK(clip.Length() == 10);
    const TestPose pose = clip.Sample(5);
    CHECK(pose.offset.x == Approx(5.0f));
    CHECK(pose.frame == 7);
    CHECK(pose.arm == 0.5f); // No track: the pose's own default
    CHECK(clip.Sample(6, 0.5f).offset.x == Approx(5.5f));
}
TEST_CASE("Engine.Clip.ALaterTrackForOneFieldWins", "[engine][animation]") {
    const auto clip = StepClip<TestPose>{}.Track(&TestPose::arm, {{0, 1.0f}}).Track(&TestPose::arm, {{0, 2.0f}});
    CHECK(clip.Sample(0).arm == 2.0f);
}
TEST_CASE("Engine.Clip.MomentsAreKeptInOrderAndChecked", "[engine][animation]") {
    auto clip = StepClip<TestPose>{}.Moment("late", 20).Moment("early", 5).Moment("alsoLate", 20);
    CHECK(clip.Length() == 20);
    CHECK(Met(clip, 0, 100) == std::vector<std::string>{"early", "late", "alsoLate"}); // As declared at one position
    CHECK(Met(clip, 5, 6) == std::vector<std::string>{"early"});
    CHECK(Met(clip, 6, 20).empty());
    CHECK(Met(clip, 6, 20, true) == std::vector<std::string>{"late", "alsoLate"});
    CHECK_THROWS(clip.Moment("early", 1)); // Twice
    CHECK_THROWS(clip.Moment("", 1));
    CHECK_THROWS(clip.Moment("negative", -1));
    CHECK(clip.Moments().size() == 3); // A refused moment leaves the clip as it was
}
TEST_CASE("Engine.Clip.LengthCannotCutItsContent", "[engine][animation]") {
    auto clip = StepClip<TestPose>{}.Track(&TestPose::arm, {{0, 0.0f}, {10, 1.0f}});
    CHECK_THROWS(clip.Length(5));
    CHECK_NOTHROW(clip.Length(30));
    CHECK(clip.Length() == 30);
    CHECK_THROWS(clip.Moment("beyond", 31));
    CHECK_THROWS(clip.Track(&TestPose::frame, {{40, 1}}));
    CHECK_THROWS(clip.Length(-1));
}
TEST_CASE("Engine.Clip.ALoopingClipWraps", "[engine][animation]") {
    const auto clip = StepClip<TestPose>{}.Track(&TestPose::arm, {{0, 0.0f}, {10, 1.0f}}).Looping();
    CHECK(clip.IsLooping());
    CHECK(clip.Sample(13).arm == Approx(0.3f));
    CHECK(clip.Sample(9, 0.5f).arm == Approx(0.85f));
}
TEST_CASE("Engine.ClipSet.RefusesInvalidDataAndSharesOnCopy", "[engine][animation]") {
    const auto idle = StepClip<TestPose>{}.Track(&TestPose::arm, {{0, 0.0f}, {10, 1.0f}}).Looping();
    using Entries = std::vector<ClipSetEntry<TestPose, int>>;
    CHECK_THROWS(StepClipSet<TestPose>(Entries{}));
    CHECK_THROWS(StepClipSet<TestPose>{{"", idle}});
    CHECK_THROWS(StepClipSet<TestPose>{{"idle", idle}, {"idle", idle}});
    CHECK_THROWS(StepClipSet<TestPose>{{"empty", StepClip<TestPose>{}.Looping()}});

    const StepClipSet<TestPose> clips{{"idle", idle}, {"point", StepClip<TestPose>{}.Track(&TestPose::frame, {{0, 4}})}};
    CHECK(clips.Count() == 2);
    CHECK(clips.Contains("point"));
    CHECK_FALSE(clips.Contains("nope"));
    CHECK(clips.Find("point") == 1);
    CHECK_THROWS(clips.Find("nope"));
    CHECK(clips.Name(0) == "idle");
    const auto copy = clips;
    CHECK(&copy.Get(0) == &clips.Get(0));
}
