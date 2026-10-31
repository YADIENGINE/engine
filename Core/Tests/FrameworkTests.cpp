#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <random>
#include <string>
#include <vector>
#include "Tests/Support/TempDirectory.h"
#include "Fx/CollectFlights.h"
#include "Gameplay/StatSheet.h"
#include "Gameplay/TimedEffectSet.h"
#include "Modules/Persistence/Persistence.h"
#include "Progress/TieredProgress.h"
#include "Utilities/ChoiceList.h"
#include "Utilities/EnumTable.h"
#include "Utilities/WeightedPick.h"

using Catch::Approx;

// Engine'in oyundan bağımsız çerçeveleri, oyunu bilmeyen sahte bir enum'la.
namespace {
enum class Attribute { Speed, Luck, Count };
struct AttributeRules {
    static float Base(Attribute attribute) { return attribute == Attribute::Speed ? 10.0f : 0.0f; }
    static float Finalize(Attribute attribute, float value) {
        return attribute == Attribute::Luck ? std::clamp(value, 0.0f, 1.0f) : value;
    }
};
using Attributes = StatSheet<Attribute, AttributeRules>;

enum class Effect { Haste, Shield, Count };

struct AttributeRow {
    Attribute id;
    int weight;
};
constexpr std::array OrderedRows{AttributeRow{Attribute::Speed, 1}, AttributeRow{Attribute::Luck, 2}};
constexpr std::array ShuffledRows{AttributeRow{Attribute::Luck, 2}, AttributeRow{Attribute::Speed, 1}};
constexpr std::array ShortRows{AttributeRow{Attribute::Speed, 1}};
static_assert(RowsInIdOrder(OrderedRows));
static_assert(!RowsInIdOrder(ShuffledRows));
static_assert(!RowsInIdOrder(ShortRows));
} // namespace

TEST_CASE("Engine.StatSheet.AddsBeforeMultiplyingAndFinalizes", "[engine][gameplay]") {
    Attributes stats;
    CHECK(stats.Get(Attribute::Speed) == Approx(10.0f));
    const std::array<Attributes::Modifier, 3> modifiers{{{Attribute::Speed, ModOp::Add, 2.0f},
                                                         {Attribute::Speed, ModOp::Multiply, 1.5f},
                                                         {Attribute::Luck, ModOp::Add, 4.0f}}};
    stats.Apply(modifiers);
    CHECK(stats.Get(Attribute::Speed) == Approx(18.0f));
    CHECK(stats.Get(Attribute::Speed, 2.0f) == Approx(21.0f)); // Anlık bonus çarpandan önce
    CHECK(stats.GetRounded(Attribute::Speed) == 18);
    CHECK(stats.Get(Attribute::Luck) == 1.0f); // Kural kırpar
    const std::array<Attributes::Modifier, 2> second{
        {{Attribute::Speed, ModOp::Multiply, 2.0f}, {Attribute::Speed, ModOp::Add, 1.0f}}};
    stats.Apply(second);
    CHECK(stats.Get(Attribute::Speed) == Approx(39.0f)); // Ayrı kaynaklar da aynı toplama/çarpma sırasını izler
    stats.Clear();
    CHECK(stats.Get(Attribute::Speed) == Approx(10.0f));
}

TEST_CASE("Engine.TimedEffectSet.AccumulatesTimedStacksPersistentAndExpires", "[engine][gameplay]") {
    constexpr int MaxStacks = 2;
    TimedEffectSet<Effect> effects;
    effects.AddTimed(Effect::Haste, 2.0f, MaxStacks);
    effects.AddPersistent(Effect::Shield);
    effects.AddPersistent(Effect::Shield);
    CHECK(effects.Active().size() == 3);
    CHECK(effects.Active().front().stacks == 1);
    CHECK(effects.FirstTimed() == Effect::Haste);
    CHECK_FALSE(effects.Update(1.5f));
    effects.AddTimed(Effect::Haste, 3.0f, MaxStacks); // Kalana eklenir, yeni kopya eklenmez
    CHECK(effects.Active().size() == 3);
    CHECK(effects.Active().front().remaining == Approx(3.5f));
    CHECK(effects.Active().front().duration == Approx(3.5f));
    CHECK(effects.Active().front().stacks == 2);
    effects.AddTimed(Effect::Haste, 1.0f, MaxStacks); // Yığın sınırda kalır, süre yine eklenir
    CHECK(effects.Active().front().stacks == MaxStacks);
    CHECK(effects.Active().front().remaining == Approx(4.5f));
    CHECK_FALSE(effects.Update(4.0f));
    CHECK(effects.Update(0.5f));
    CHECK(effects.Active().size() == 2);
    CHECK_FALSE(effects.FirstTimed().has_value());
}

TEST_CASE("Engine.TimedEffectSet.RefreshPreservesStacksAndExcludesTheRequestedId", "[engine][gameplay]") {
    TimedEffectSet<Effect> effects;
    effects.AddTimed(Effect::Haste, 6.0f, 3);
    effects.AddTimed(Effect::Haste, 4.0f, 3);
    effects.AddTimed(Effect::Shield, 8.0f, 3);
    effects.AddPersistent(Effect::Shield);
    effects.Update(2.0f);
    effects.RefreshTimed(Effect::Shield);
    CHECK(effects.Active()[0].remaining == Approx(10.0f));
    CHECK(effects.Active()[0].duration == Approx(10.0f));
    CHECK(effects.Active()[0].stacks == 2);
    CHECK(effects.Active()[1].remaining == Approx(6.0f));
    CHECK_FALSE(effects.Active()[2].remaining);
    effects.RefreshTimed(Effect::Shield);
    CHECK(effects.Active()[0].remaining == Approx(10.0f));
    effects.Update(6.0f); // Süreli Shield biter; kalıcı olan aynı kimlikle kalır.
    effects.RefreshTimed();
    REQUIRE(effects.Active().size() == 2);
    CHECK(effects.Active()[0].remaining == Approx(10.0f));
    CHECK(effects.Active()[0].stacks == 2);
    CHECK_FALSE(effects.Active()[1].remaining);
}

TEST_CASE("Engine.TimedEffectSet.FirstTimedKeepsPickupOrderUntilExpiration", "[engine][gameplay]") {
    TimedEffectSet<Effect> effects;
    effects.AddPersistent(Effect::Shield);
    CHECK_FALSE(effects.FirstTimed());
    effects.AddTimed(Effect::Haste, 1.0f, 2);
    effects.AddTimed(Effect::Shield, 3.0f, 2);
    effects.Update(0.5f);
    effects.AddTimed(Effect::Haste, 1.0f, 2);
    CHECK(effects.FirstTimed() == Effect::Haste);
    effects.Update(1.5f);
    CHECK(effects.FirstTimed() == Effect::Shield);
    effects.Update(1.0f);
    CHECK_FALSE(effects.FirstTimed());
}

TEST_CASE("Engine.WeightedIndex.NeverPicksZeroWeights", "[engine][random]") {
    std::mt19937 rng(7);
    const std::vector<int> weights{0, 3, 0, 1};
    for (int i = 0; i < 16; ++i) {
        const std::size_t picked = WeightedIndex(weights, rng);
        CHECK((picked == 1 || picked == 3));
    }
    CHECK(WeightedIndex(std::array{0, 0, 1, 0}, rng) == 2);
}

TEST_CASE("Engine.ChoiceList.ReadsStoredValuesAndStepsWithWrap", "[engine][settings]") {
    constexpr std::array choices{30, 60, 120, 0};
    static_assert(StoredChoice(120, choices, 60) == 120);
    static_assert(StoredChoice(45, choices, 60) == 60);
    static_assert(StepChoice(0, choices, 1) == 30);
    static_assert(StepChoice(30, choices, -1) == 0);
    static_assert(StepChoice(45, choices, 1) == 60); // Listede olmayan ilkten sayılır

    const std::vector<int> filtered{0, 30, 60};
    CHECK(StepChoice(60, std::span<const int>(filtered), 1) == 0);
    CHECK(StepChoice(0, std::span<const int>(filtered), -1) == 60);
    CHECK(StepChoice(120, std::span<const int>(filtered), 1) == 30); // Süzülüp listeden çıkan değer ilkten sayılır
    CHECK(StepChoice(45, std::span<const int>(), 1) == 45);          // Boş listede değişmez
}

TEST_CASE("Engine.TieredProgress.ClampsLoadsAndSkipsUnsavedProgress", "[engine][save]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "goals.json");
    save.SetInt("goal.a.tier", 9);
    save.SetInt("goal.a.seen", 9);
    save.SetInt("goal.b.tier", 1);
    save.SetInt("goal.b.progress", -5);
    const auto keyOf = [](std::size_t i) { return std::string(i == 0 ? "goal.a" : "goal.b"); };
    TieredProgress<2> progress;
    progress.Load(save, keyOf, [](std::size_t) { return 3; });
    CHECK(progress.Tier(0) == 3);
    CHECK(progress.Unseen(0) == 0);
    CHECK(progress.Tier(1) == 1);
    CHECK(progress.Unseen(1) == 1);
    CHECK(progress.Progress(1) == 0.0f);
    progress.CompleteTier(1);
    progress.SetProgress(0, 12.7f);
    progress.SetProgress(1, 4.0f);
    CHECK(progress.UnseenCount() == 2);
    Persistence out(temp.Path() / "out.json");
    progress.Store(out, keyOf, [](std::size_t i) { return i == 0; });
    CHECK(out.GetInt("goal.a.progress") == 12); // Aşağı yuvarlanır
    CHECK_FALSE(out.Contains("goal.b.progress"));
    CHECK(out.GetInt("goal.b.tier") == 2);
    progress.MarkSeen();
    CHECK(progress.UnseenCount() == 0);
}

TEST_CASE("Engine.CollectFlights.HoldCreditUntilArrival", "[engine][fx]") {
    CollectFlights<Effect> flights(0.5f);
    flights.Emit(Effect::Shield, {}, 3, 0.25f);
    flights.Emit(Effect::Haste, {}, 1, 0.0f);
    CHECK(flights.Pending(Effect::Shield) == 3);
    flights.Update(0.5f);
    CHECK(flights.Pending(Effect::Haste) == 0);
    CHECK(flights.Pending(Effect::Shield) == 3);
    flights.Update(0.25f);
    CHECK(flights.Active().empty());
}
