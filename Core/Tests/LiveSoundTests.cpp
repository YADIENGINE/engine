#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include "Audio/LiveSound.h"
#include "Audio/SoundLayers.h"
#include "Audio/SoundPlayer.h"
#include "Audio/SoundSpace.h"
#include "Audio/SoundTables.h"
#include "Camera/CameraView.h"
#include "Runtime/GameSettings.h"
#include "Runtime/Internals/GameAssetManifest.h"
#include "Tests/Support/FakeSoundOutput.h"

namespace {
enum class Sound { Hit, Hum, Rain, Wind, Music, Count };
inline constexpr std::array TestSounds{
    SoundRow{Sound::Hit, {"Audio/hit.wav"}, {.pitch = {0.5f, 2.0f}, .cooldown = 0.5f}},
    SoundRow{Sound::Hum, {"Audio/hum1.wav", "Audio/hum2.wav"}, {.loop = true, .falloff = {.fullWithin = 100.0f, .silentBeyond = 500.0f, .curve = Curve::Linear}}},
    SoundRow{Sound::Rain, {"Audio/Ambience/rain.ogg"}, {.loop = true, .streamed = true}},
    SoundRow{Sound::Wind, {"Audio/Ambience/wind.ogg"}, {.loop = true, .streamed = true}},
    SoundRow{Sound::Music, {"Audio/Stems/low.ogg"}, {.loop = true, .streamed = true, .bus = SoundBus::Music}},
};
static_assert(RowsInIdOrder(TestSounds));
static_assert(TestSounds[1].options.loop && !TestSounds[0].options.loop);
static_assert(TestSounds[4].options.bus == SoundBus::Music && TestSounds[4].options.streamed);

// A row that is invalid does not compile: this asks the compiler whether making the row is a constant expression.
template <class Make> concept CompilesAsConstant = requires { typename std::bool_constant<(Make{}(), true)>; };
static_assert(CompilesAsConstant<decltype([] { return SoundRow{Sound::Hit, {"a.wav"}, {.loop = true}}; })>);
static_assert(!CompilesAsConstant<decltype([] {
    return SoundRow{Sound::Hit, {"a.wav"}, {.falloff = {.fullWithin = 10.0f, .silentBeyond = 5.0f}}};
})>);
static_assert(!CompilesAsConstant<decltype([] {
    return SoundRow{Sound::Hit, {"a.wav"}, {.falloff = {.fullWithin = -1.0f}}};
})>);
static_assert(!CompilesAsConstant<decltype([] {
    return SoundRow{Sound::Hit, {"a.wav"}, {.bus = static_cast<SoundBus>(9)}};
})>);

struct Bench {
    FakeSoundOutput output;
    SoundTables tables = TestSounds;
    SoundPlayer player{tables, output};
};
} // namespace

// Pan and distance.

TEST_CASE("Engine.SoundSpace.TheFalloffIsFullThenFallsThenSilent", "[engine][audio]") {
    const SoundFalloff linear{.fullWithin = 100.0f, .silentBeyond = 300.0f, .curve = Curve::Linear};
    CHECK(FalloffGain(linear, 0.0f) == 1.0f);
    CHECK(FalloffGain(linear, 100.0f) == 1.0f);
    CHECK(FalloffGain(linear, 200.0f) == Catch::Approx(0.5f));
    CHECK(FalloffGain(linear, 300.0f) == 0.0f);
    CHECK(FalloffGain(linear, 1e9f) == 0.0f);
    const SoundFalloff natural{.fullWithin = 100.0f, .silentBeyond = 300.0f};
    CHECK(FalloffGain(natural, 200.0f) == Catch::Approx(0.25f)); // InQuad, read from far to near: falls fast at first
    float previous = 2.0f;
    for (float distance = 0.0f; distance <= 400.0f; distance += 5.0f) {
        const float gain = FalloffGain(natural, distance);
        CHECK(gain <= previous);
        previous = gain;
    }
}

TEST_CASE("Engine.SoundSpace.ASoundOnTheLeftPansLeft", "[engine][audio]") {
    const SoundListener listener{.position = {100.0f, 100.0f}, .panWidth = 200.0f, .maxPan = 0.8f};
    CHECK(PlaceSound(listener, {100.0f, 100.0f}).pan == 0.0f);
    CHECK(PlaceSound(listener, {0.0f, 100.0f}).pan == Catch::Approx(-0.4f));
    CHECK(PlaceSound(listener, {200.0f, 100.0f}).pan == Catch::Approx(0.4f));
    CHECK(PlaceSound(listener, {-5000.0f, 100.0f}).pan == Catch::Approx(-0.8f)); // Never fully out of one ear
    CHECK(PlaceSound(listener, {100.0f, 5000.0f}).pan == 0.0f); // Straight ahead or behind is the middle
}

TEST_CASE("Engine.SoundSpace.PanAgreesWithWhereACameraShowsTheSound", "[engine][audio]") {
    for (const float rotation : {0.0f, 0.5f, -1.2f, 2.5f}) {
        CameraView view;
        view.target = {300.0f, -200.0f};
        view.offset = {640.0f, 360.0f};
        view.rotation = rotation;
        const SoundListener listener{.position = view.target, .rotation = view.rotation, .panWidth = 100.0f, .maxPan = 1.0f};
        for (const Float2 source : {Float2{310.0f, -200.0f}, Float2{300.0f, -170.0f}, Float2{260.0f, -240.0f}}) {
            const float onScreen = view.ToScreen(source).x - view.offset.x;
            CAPTURE(rotation, source.x, source.y);
            CHECK(PlaceSound(listener, source).pan == Catch::Approx(onScreen / 100.0f).margin(0.0001));
        }
    }
}

TEST_CASE("Engine.SoundSpace.DistanceScaleHearsFurther", "[engine][audio]") {
    const SoundFalloff falloff{.fullWithin = 100.0f, .silentBeyond = 300.0f, .curve = Curve::Linear};
    const SoundListener plain{.distanceScale = 1.0f}, keen{.distanceScale = 0.5f};
    CHECK(PlaceSound(plain, {400.0f, 0.0f}, falloff).gain == 0.0f);
    CHECK(PlaceSound(keen, {400.0f, 0.0f}, falloff).gain == Catch::Approx(0.5f));
}

TEST_CASE("Engine.SoundSpace.WithTwoListenersTheOneWhoHearsBestDecides", "[engine][audio]") {
    const std::array listeners{SoundListener{.position = {0.0f, 0.0f}}, SoundListener{.position = {2000.0f, 0.0f}}};
    const SoundFalloff falloff{.fullWithin = 100.0f, .silentBeyond = 500.0f};
    const SoundPlacement placement = PlaceSound(listeners, Float2{2100.0f, 0.0f}, falloff);
    CHECK(placement.gain == 1.0f);
    CHECK(placement.pan > 0.0f); // Right of the second one, although it is far right of the first as well
    CHECK(PlaceSound(listeners, Float2{-50.0f, 0.0f}, falloff).pan < 0.0f);
    CHECK(PlaceSound(std::span<const SoundListener>{}, Float2{}, falloff).gain == 1.0f);
}

TEST_CASE("Engine.SoundSpace.EarsKeepThePowerConstant", "[engine][audio]") {
    CHECK(PanEars(0.0f).left == Catch::Approx(1.0f));
    CHECK(PanEars(0.0f).right == Catch::Approx(1.0f));
    CHECK(PanEars(-1.0f).left == Catch::Approx(std::numbers::sqrt2_v<float>));
    CHECK(PanEars(-1.0f).right == Catch::Approx(0.0f).margin(0.0001));
    CHECK(PanEars(1.0f).right == Catch::Approx(std::numbers::sqrt2_v<float>));
    for (float pan = -1.0f; pan <= 1.0f; pan += 0.1f) {
        const SoundEars ears = PanEars(pan);
        CHECK(ears.left * ears.left + ears.right * ears.right == Catch::Approx(2.0f));
    }
}

TEST_CASE("Engine.SoundSpace.AListenerIsChecked", "[engine][audio]") {
    CHECK(IsValid(SoundListener{}));
    const float infinite = std::numeric_limits<float>::infinity();
    CHECK_FALSE(IsValid(SoundListener{.position = {infinite, 0.0f}}));
    CHECK_FALSE(IsValid(SoundListener{.panWidth = 0.0f}));
    CHECK_FALSE(IsValid(SoundListener{.maxPan = 1.5f}));
    CHECK_FALSE(IsValid(SoundListener{.distanceScale = -1.0f}));
    Bench bench;
    CHECK_THROWS_AS(bench.player.SetListener({.panWidth = 0.0f}), std::invalid_argument);
    REQUIRE(bench.player.Listeners().size() == 1);
    CHECK(bench.player.Listeners()[0].panWidth == SoundListener{}.panWidth); // A refused one changes nothing
}

// A play.

TEST_CASE("Engine.Sounds.APlaySaysHowItDiffers", "[engine][audio]") {
    Bench bench;
    bench.player.SetListener({.position = {0.0f, 0.0f}, .panWidth = 100.0f, .maxPan = 1.0f});
    CHECK(bench.player.Play(Sound::Hit, {.volume = 0.5f, .pitch = 1.0f, .at = Float2{50.0f, 0.0f}, .muffle = 0.25f, .fadeIn = 0.1f, .startAt = 0.3f}));
    const auto& voice = bench.output.Last();
    CHECK(voice.start.volume == 0.5f);
    CHECK(voice.start.pan == Catch::Approx(0.5f));
    CHECK(voice.start.gain == 1.0f);
    CHECK(voice.start.muffle == 0.25f);
    CHECK(voice.start.fadeIn == 0.1f);
    CHECK(voice.start.startAt == 0.3f);
    CHECK_FALSE(voice.start.loop);
    CHECK_FALSE(voice.start.owned);
}

TEST_CASE("Engine.Sounds.APlayWithAPanHasNoFalloff", "[engine][audio]") {
    Bench bench;
    bench.player.Update(10.0f);
    CHECK(bench.player.Play(Sound::Hit, {.pan = -0.5f}));
    CHECK(bench.output.Last().start.pan == -0.5f);
    CHECK(bench.output.Last().start.gain == 1.0f);
}

TEST_CASE("Engine.Sounds.BadValuesInAPlayThrow", "[engine][audio]") {
    Bench bench;
    const float nan = std::nanf("");
    const float infinite = std::numeric_limits<float>::infinity();
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.volume = 1.5f}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.pitch = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.at = Float2{nan, 0.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.pan = 2.0f}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.muffle = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.fadeIn = infinite}), std::invalid_argument);
    CHECK_THROWS_AS(bench.player.Play(Sound::Hit, {.startAt = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS((void)bench.player.Start(Sound::Hum, {.volume = nan}), std::invalid_argument);
    CHECK(bench.output.started.empty());
}

TEST_CASE("Engine.Sounds.ALoopCannotBeFiredAndForgotten", "[engine][audio]") {
    Bench bench;
    CHECK_THROWS_WITH(bench.player.Play(Sound::Hum), Catch::Matchers::ContainsSubstring("start it with Start"));
    CHECK_THROWS_AS(bench.player.Play(Sound::Hum), std::logic_error);
    CHECK(bench.output.started.empty());
}

TEST_CASE("Engine.Sounds.ADroppedPlayUsesUpNothing", "[engine][audio]") {
    class Full final : public FakeSoundOutput {
    public:
        [[nodiscard]] VoiceId StartVoice(SoundId, const VoiceStart&) override { return VoiceId{}; }
    };
    Full output;
    const SoundTables tables = TestSounds;
    SoundPlayer player(tables, output);
    CHECK_FALSE(player.Play(Sound::Hit)); // The device's overflow rule said no
}

// A live sound.

TEST_CASE("Engine.LiveSound.StartsFromItsRowAndIsHeldByTheHandle", "[engine][audio]") {
    Bench bench;
    LiveSound sound = bench.player.Start(Sound::Hum, {.volume = 0.5f});
    CHECK(sound.Playing());
    CHECK(static_cast<bool>(sound));
    const auto& voice = bench.output.Last();
    CHECK(voice.start.loop);
    CHECK(voice.start.owned);
    CHECK(voice.start.volume == 0.5f);
    const auto id = static_cast<std::size_t>(voice.id);
    CHECK((id == SoundTables::FirstGeneratedId + 1 || id == SoundTables::FirstGeneratedId + 2));
}

TEST_CASE("Engine.LiveSound.ANonLoopingRowCanBeLiveToo", "[engine][audio]") {
    Bench bench;
    LiveSound sound = bench.player.Start(Sound::Hit);
    CHECK_FALSE(bench.output.Last().start.loop);
    bench.output.Of(0).alive = false; // It reached its end
    CHECK_FALSE(sound.Playing());
    CHECK_NOTHROW(sound.SetVolume(0.5f));
    bench.player.Update(0.1f);
    CHECK_FALSE(sound.Playing());
    CHECK_NOTHROW(sound.SetPosition({1.0f, 2.0f}));
}

TEST_CASE("Engine.LiveSound.StartIgnoresCooldownsAndOverlap", "[engine][audio]") {
    Bench bench;
    LiveSound a = bench.player.Start(Sound::Hit);
    LiveSound b = bench.player.Start(Sound::Hit);
    CHECK(bench.output.started.size() == 2);
    CHECK(bench.player.Play(Sound::Hit)); // And a live one starts no cooldown
}

TEST_CASE("Engine.LiveSound.EveryChangeReachesItsVoice", "[engine][audio]") {
    Bench bench;
    LiveSound sound = bench.player.Start(Sound::Hum);
    sound.SetVolume(0.25f, 2.0f);
    sound.SetPitch(1.5f);
    sound.SetMuffle(0.75f);
    auto& voice = bench.output.Of(0);
    CHECK(voice.volume == 0.25f);
    CHECK(voice.lastFadeSeconds == 2.0f);
    CHECK(voice.pitch == 1.5f);
    CHECK(voice.muffle == 0.75f);
    sound.SetPan(-0.5f);
    CHECK(voice.pan == -0.5f);
    CHECK(voice.gain == 1.0f);
}

TEST_CASE("Engine.LiveSound.BadValuesThrowEvenWithNothingPlaying", "[engine][audio]") {
    LiveSound empty;
    const float nan = std::nanf("");
    CHECK_FALSE(empty.Playing());
    CHECK_THROWS_AS(empty.SetVolume(2.0f), std::invalid_argument);
    CHECK_THROWS_AS(empty.SetVolume(0.5f, nan), std::invalid_argument);
    CHECK_THROWS_AS(empty.SetPitch(0.0f), std::invalid_argument);
    CHECK_THROWS_AS(empty.SetPosition({nan, 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(empty.SetPan(-3.0f), std::invalid_argument);
    CHECK_THROWS_AS(empty.SetMuffle(3.0f), std::invalid_argument);
    CHECK_THROWS_AS(empty.Stop(-1.0f), std::invalid_argument);
    CHECK_NOTHROW(empty.SetVolume(0.5f)); // Good values do nothing
    CHECK_NOTHROW(empty.Stop());
}

TEST_CASE("Engine.LiveSound.PositionFollowsListenersWithoutACallOnTheHandle", "[engine][audio]") {
    Bench bench;
    bench.player.SetListener({.position = {0.0f, 0.0f}, .panWidth = 100.0f, .maxPan = 1.0f});
    LiveSound placed = bench.player.Start(Sound::Hum, {.at = Float2{300.0f, 0.0f}}); // Row: full within 100, silent at 500
    LiveSound flat = bench.player.Start(Sound::Hum, {.pan = -0.5f});
    auto& voice = bench.output.Of(0);
    auto& other = bench.output.Of(1);
    CHECK(voice.start.gain == Catch::Approx(0.5f));
    CHECK(voice.start.pan == 1.0f);

    bench.player.SetListener({.position = {300.0f, 0.0f}, .panWidth = 100.0f, .maxPan = 1.0f});
    CHECK(voice.gain == 1.0f);
    CHECK(voice.pan == 0.0f);
    CHECK(other.pan == -0.5f); // Not in the world: stays
    CHECK(other.gain == 1.0f);

    placed.SetPosition({250.0f, 0.0f});
    CHECK(voice.pan == Catch::Approx(-0.5f));
    placed.SetPan(0.25f); // Leaves the world
    bench.player.SetListener({.position = {9000.0f, 0.0f}});
    CHECK(voice.pan == 0.25f);
    CHECK(voice.gain == 1.0f);
    flat.SetPosition({9000.0f, 0.0f}); // Enters it
    CHECK(other.pan == 0.0f);
}

TEST_CASE("Engine.LiveSound.GoingAwayStopsItWithAFade", "[engine][audio]") {
    Bench bench;
    {
        LiveSound sound = bench.player.Start(Sound::Hum);
        CHECK(bench.output.Of(0).alive);
    }
    CHECK_FALSE(bench.output.Of(0).alive);
    CHECK(bench.output.Of(0).stoppedWithFade == LiveSound::DefaultStopFade);
}

TEST_CASE("Engine.LiveSound.StopTakesAFadeAndEmptiesTheHandle", "[engine][audio]") {
    Bench bench;
    LiveSound sound = bench.player.Start(Sound::Hum);
    sound.Stop(3.0f);
    CHECK(bench.output.Of(0).stoppedWithFade == 3.0f);
    CHECK_FALSE(sound.Playing());
    CHECK_NOTHROW(sound.SetPitch(2.0f));
    CHECK_NOTHROW(sound.Stop());
    CHECK(bench.output.Of(0).stoppedWithFade == 3.0f); // Once
}

TEST_CASE("Engine.LiveSound.MovingHandsOverTheSound", "[engine][audio]") {
    Bench bench;
    LiveSound a = bench.player.Start(Sound::Hum);
    LiveSound b = std::move(a);
    CHECK_FALSE(a.Playing()); // NOLINT: a moved-from handle is an empty one, by contract
    CHECK(b.Playing());
    CHECK(bench.output.Of(0).alive);
    b = bench.player.Start(Sound::Rain); // The old sound stops when it is replaced
    CHECK_FALSE(bench.output.Of(0).alive);
    CHECK(bench.output.Of(1).alive);
    LiveSound& self = b;
    b = std::move(self);
    CHECK(b.Playing());
}

TEST_CASE("Engine.LiveSound.AHandleCanOutliveThePlayer", "[engine][audio]") {
    FakeSoundOutput output;
    const SoundTables tables = TestSounds;
    LiveSound survivor;
    {
        SoundPlayer player(tables, output);
        survivor = player.Start(Sound::Hum);
        CHECK(survivor.Playing());
    }
    CHECK_FALSE(survivor.Playing());
    CHECK_NOTHROW(survivor.SetVolume(0.5f));
    CHECK_NOTHROW(survivor.SetPosition({1.0f, 1.0f}));
    CHECK_NOTHROW(survivor.Stop());
}

TEST_CASE("Engine.LiveSound.WrongKeysThrowLikeAPlay", "[engine][audio]") {
    enum class Stranger { Nobody, Count };
    Bench bench;
    CHECK_THROWS_AS((void)bench.player.Start(Stranger::Nobody), std::out_of_range);
    CHECK_THROWS_AS((void)bench.player.Start(SoundKey{}), std::out_of_range);
    CHECK_THROWS_AS((void)bench.player.Start(Sound::Count), std::out_of_range);
    CHECK(bench.output.started.empty());
}

TEST_CASE("Engine.LiveSound.PauseHoldsTheLiveSoundsOfABus", "[engine][audio]") {
    Bench bench;
    LiveSound world = bench.player.Start(Sound::Hum);
    LiveSound music = bench.player.Start(Sound::Music);
    bench.player.SetBusPaused(SoundBus::World, true);
    CHECK(bench.player.BusPaused(SoundBus::World));
    CHECK(bench.output.Of(0).paused);
    CHECK_FALSE(bench.output.Of(1).paused);
    LiveSound late = bench.player.Start(Sound::Rain); // Born paused, with no blip
    CHECK(bench.output.Last().start.paused);
    bench.player.Play(Sound::Hit); // Something happening still plays: a menu needs this
    CHECK_FALSE(bench.output.Last().start.paused);
    bench.player.SetBusPaused(SoundBus::World, false);
    CHECK_FALSE(bench.output.Of(0).paused);
    CHECK_FALSE(bench.output.Of(2).paused);
}

TEST_CASE("Engine.LiveSound.AHostsHoldIsOnTopOfTheGamesPause", "[engine][audio]") {
    Bench bench;
    LiveSound world = bench.player.Start(Sound::Hum);
    LiveSound music = bench.player.Start(Sound::Music);
    bench.player.SetHold(true);
    CHECK(bench.output.Of(0).paused);
    CHECK(bench.output.Of(1).paused);
    bench.player.SetBusPaused(SoundBus::World, true);
    bench.player.SetHold(false);
    CHECK(bench.output.Of(0).paused); // The game still has it paused
    CHECK_FALSE(bench.output.Of(1).paused);
    bench.player.SetHold(true);
    bench.player.SetBusPaused(SoundBus::World, false);
    CHECK(bench.output.Of(0).paused); // The host still holds it
}

TEST_CASE("Engine.LiveSound.TogetherShareAFrameOnTheClock", "[engine][audio]") {
    Bench bench;
    bench.output.clock = 5000;
    const std::array keys{SoundKey(Sound::Rain), SoundKey(Sound::Wind)};
    auto sounds = bench.player.StartTogether(keys, {.volume = 0.0f});
    REQUIRE(sounds.size() == 2);
    const auto frame = bench.output.Of(0).start.startFrame;
    CHECK(frame > 5000);
    CHECK(bench.output.Of(1).start.startFrame == frame);
    CHECK(bench.output.Of(0).start.volume == 0.0f);
    // All or none.
    const std::array bad{SoundKey(Sound::Rain), SoundKey{}};
    const auto before = bench.output.started.size();
    CHECK_THROWS_AS((void)bench.player.StartTogether(bad), std::out_of_range);
    CHECK(bench.output.started.size() == before);
}

TEST_CASE("Engine.Sounds.BusesPassThrough", "[engine][audio]") {
    Bench bench;
    bench.player.FadeBus(SoundBus::World, 0.25f, 1.5f);
    bench.player.MuffleBus(SoundBus::Ui, 0.5f);
    CHECK(bench.output.busFades[SoundBus::World] == std::pair(0.25f, 1.5f));
    CHECK(bench.output.busMuffles[SoundBus::Ui] == std::pair(0.5f, 0.0f));
}

TEST_CASE("Engine.Sounds.KeysCompareByEnumAndValue", "[engine][audio]") {
    enum class Other { Hit, Count };
    CHECK(SoundKey(Sound::Hit) == SoundKey(Sound::Hit));
    CHECK(SoundKey(Sound::Hit) != SoundKey(Sound::Hum));
    CHECK(SoundKey(Sound::Hit) != SoundKey(Other::Hit)); // Value 0 of both
    CHECK(SoundKey{} == SoundKey{});
    CHECK(SoundKey{} != SoundKey(Sound::Hit));
}

// Layers.

TEST_CASE("Engine.SoundLayers.SetStartsALayerSilentAndFadesItUp", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player, 2.0f);
    layers.Set(Sound::Rain, 0.75f);
    REQUIRE(bench.output.started.size() == 1);
    auto& voice = bench.output.Of(0);
    CHECK(voice.start.volume == 0.0f);
    CHECK(voice.volume == 0.75f);
    CHECK(voice.lastFadeSeconds == 2.0f);
    CHECK(layers.Volume(Sound::Rain) == 0.75f);
    layers.Set(Sound::Rain, 0.25f, 0.5f); // A second time: the same voice
    CHECK(bench.output.started.size() == 1);
    CHECK(voice.volume == 0.25f);
    CHECK(voice.lastFadeSeconds == 0.5f);
}

TEST_CASE("Engine.SoundLayers.MixSendsWhatItDoesNotNameToSilence", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    layers.Mix({{Sound::Rain, 1.0f}, {Sound::Wind, 0.5f}}, 3.0f);
    REQUIRE(bench.output.started.size() == 2);
    CHECK(bench.output.Of(0).volume == 1.0f);
    CHECK(bench.output.Of(1).volume == 0.5f);
    layers.Mix({{Sound::Wind, 1.0f}}, 3.0f);
    CHECK(bench.output.started.size() == 2);
    CHECK(bench.output.Of(0).alive); // Silent, but it keeps its place
    CHECK(bench.output.Of(0).volume == 0.0f);
    CHECK(bench.output.Of(1).volume == 1.0f);
    CHECK(layers.Volume(Sound::Rain) == 0.0f);
    CHECK(layers.Volume(Sound::Wind) == 1.0f);
}

TEST_CASE("Engine.SoundLayers.SilenceRemembersTheMix", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    layers.Set(Sound::Rain, 0.5f);
    layers.Silence(1.0f);
    CHECK(layers.Silenced());
    CHECK(bench.output.Of(0).volume == 0.0f);
    layers.Set(Sound::Rain, 0.75f); // A change meanwhile waits
    CHECK(bench.output.Of(0).volume == 0.0f);
    layers.Set(Sound::Wind, 1.0f);
    CHECK(bench.output.Of(1).volume == 0.0f);
    layers.Restore(1.0f);
    CHECK_FALSE(layers.Silenced());
    CHECK(bench.output.Of(0).volume == 0.75f);
    CHECK(bench.output.Of(1).volume == 1.0f);
}

TEST_CASE("Engine.SoundLayers.MuffleReachesLayersThatComeLater", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    layers.Set(Sound::Rain, 1.0f);
    layers.SetMuffle(0.5f, 0.0f);
    CHECK(bench.output.Of(0).muffle == 0.5f);
    layers.Set(Sound::Wind, 1.0f);
    CHECK(bench.output.Of(1).start.muffle == 0.5f);
}

TEST_CASE("Engine.SoundLayers.StopEndsThemAndGoingAwayToo", "[engine][audio]") {
    Bench bench;
    {
        SoundLayers layers(bench.player);
        layers.Set(Sound::Rain, 1.0f);
        layers.Stop(4.0f);
        CHECK(bench.output.Of(0).stoppedWithFade == 4.0f);
        CHECK(layers.Volume(Sound::Rain) == 0.0f);
        layers.Set(Sound::Rain, 1.0f); // From the top
        CHECK(bench.output.started.size() == 2);
    }
    CHECK_FALSE(bench.output.Of(1).alive);
}

TEST_CASE("Engine.SoundLayers.PreparedLayersShareTheirStart", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    layers.Prepare({Sound::Rain, Sound::Wind});
    REQUIRE(bench.output.started.size() == 2);
    CHECK(bench.output.Of(0).start.startFrame == bench.output.Of(1).start.startFrame);
    CHECK(bench.output.Of(0).start.volume == 0.0f);
    layers.Set(Sound::Wind, 1.0f);
    CHECK(bench.output.started.size() == 2); // Already running
    CHECK_THROWS_AS(layers.Prepare({Sound::Wind}), std::logic_error);
}

TEST_CASE("Engine.SoundLayers.ALayerThatEndedStartsAgain", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    layers.Set(Sound::Hit, 1.0f); // Not a loop: it ends by itself
    bench.output.Of(0).alive = false;
    layers.Set(Sound::Hit, 1.0f);
    CHECK(bench.output.started.size() == 2);
}

TEST_CASE("Engine.SoundLayers.BadValuesThrowAndChangeNothing", "[engine][audio]") {
    Bench bench;
    SoundLayers layers(bench.player);
    CHECK_THROWS_AS(layers.Set(Sound::Rain, 2.0f), std::invalid_argument);
    CHECK_THROWS_AS(layers.Mix({{Sound::Rain, -1.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(layers.SetMuffle(2.0f), std::invalid_argument);
    CHECK_THROWS_AS(layers.Silence(std::numeric_limits<float>::infinity()), std::invalid_argument);
    CHECK(bench.output.started.empty());
    CHECK_THROWS_AS(SoundLayers(bench.player, -1.0f), std::invalid_argument);
}

// The package. D37 leaves a sound out unless the settings name it, and these sounds are started long after startup.

TEST_CASE("Engine.Sounds.LoopsAndLayersReachThePackage", "[engine][audio]") {
    GameSettings settings;
    settings.sounds = TestSounds;
    settings.audio.music = {{MusicId{0}, "Audio/menu.ogg"}};
    settings.assets = {"Data/level.json"};
    const auto files = GameAssetManifest(settings);
    const auto named = [&files](const char* file) { return std::ranges::find(files, std::filesystem::path(file)) != files.end(); };
    CHECK(named("Audio/hit.wav"));
    CHECK(named("Audio/hum1.wav")); // A loop, every variant
    CHECK(named("Audio/hum2.wav"));
    CHECK(named("Audio/Ambience/rain.ogg")); // Layers, streamed or not
    CHECK(named("Audio/Ambience/wind.ogg"));
    CHECK(named("Audio/Stems/low.ogg"));
    CHECK(named("Audio/menu.ogg"));
    CHECK(named("Data/level.json"));
    CHECK(files.size() == 8);
    // The door is shut on anything else: a live sound and a layer start only from a key of a table.
    static_assert(!std::is_invocable_v<decltype(&SoundPlayer::Start), SoundPlayer&, const char*, const SoundPlay&>);
    static_assert(!std::is_constructible_v<SoundKey, const char*>);
    static_assert(!std::is_constructible_v<SoundKey, SoundId>);
}

TEST_CASE("Engine.Sounds.CatalogCarriesTheRowsNewOptions", "[engine][audio]") {
    const AudioCatalog catalog = CatalogWithSounds({}, TestSounds);
    REQUIRE(catalog.effects.size() == 6);
    CHECK_FALSE(catalog.effects[0].settings.streamed);
    CHECK(catalog.effects[0].settings.bus == SoundBus::World);
    CHECK(catalog.effects[3].settings.streamed);
    CHECK(catalog.effects[5].settings.bus == SoundBus::Music);
}
