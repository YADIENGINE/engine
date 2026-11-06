#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Animation/Tween.h"
#include "Simulation/StateDump.h"
#include "Simulation/StateHash.h"
#include "Simulation/StepSmoothed.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"
#include "Utilities/Reflection.h"

using Catch::Approx;

// Reflected types need a name outside functions and anonymous namespaces (Utilities/Reflection.h).
enum class StateFieldsMode { Calm, Angry };
struct StateFieldsEnemy {
    int hp = 3;
    Float2 at{};
};
struct StateFieldsRun {
    Rng rng{7};
    StepSmoothed<float> shipX{2.5f};
    StepTween<float> recoil{1.0f};
    std::vector<StateFieldsEnemy> enemies;
    std::array<bool, 2> awake{};
    std::optional<int> best;
    StateFieldsMode mode = StateFieldsMode::Calm;
    Color4 tint{1, 2, 3, 4};
    std::string name = "run";
    int score = 0;
    static constexpr int NotAField = 9;
};
// A type with its own State wins over its fields.
struct StateFieldsCustom {
    int shown = 1;
    int hidden = 2;
    void State(StateVisitor& state) const { state.Field("shown", shown); }
};
struct StateFieldsOuter {
    StateFieldsEnemy boss;
    StateFieldsCustom custom;
};

namespace {
class Recorder final : public StateVisitor {
public:
    std::string text;

protected:
    void Value(std::string_view name, const StateValue&) override {
        text += name;
        text += ';';
    }
    void BeginGroup(std::string_view name, std::int64_t index) override {
        text += name;
        if (index >= 0) text += '[' + std::to_string(index) + ']';
        text += '{';
    }
    void EndGroup() override { text += '}'; }
};
const JsonIO::Document& At(const StateDump& dump, std::string_view key) { return dump.Result().At(key); }
bool Has(const StateDump& dump, std::string_view key) { return dump.Result().Members().contains(key); }
} // namespace

TEST_CASE("Engine.Reflection.WalksFieldsInDeclarationOrder", "[engine][simulation]") {
    STATIC_REQUIRE(PlainStruct<StateFieldsRun>);
    STATIC_REQUIRE(FieldCount<StateFieldsRun> == 10);
    STATIC_REQUIRE(FieldName<9, StateFieldsRun>() == "score");
    STATIC_REQUIRE(!PlainStruct<Rng>);              // Private fields
    STATIC_REQUIRE(!PlainStruct<std::array<int, 3>>); // A list, not a struct
    STATIC_REQUIRE(!PlainStruct<int>);
    std::vector<std::string> names;
    StateFieldsEnemy enemy{.hp = 5};
    ForEachField(enemy, [&](std::string_view name, auto& field) {
        names.emplace_back(name);
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(field)>, int>) field = 8;
    });
    CHECK(names == std::vector<std::string>{"hp", "at"});
    CHECK(enemy.hp == 8);
}

TEST_CASE("Engine.State.APlainStructDumpsEveryFieldByItsName", "[engine][simulation]") {
    StateFieldsRun run;
    run.enemies = {{.hp = 1, .at = {4.0f, 5.0f}}, {.hp = 2}};
    run.awake = {false, true};
    run.score = 12;
    StateDump dump;
    dump.Fields(run);
    CHECK(Has(dump, "rng.state"));
    CHECK(At(dump, "shipX").AsDouble() == Approx(2.5)); // Only the current value
    CHECK(At(dump, "recoil.value").AsDouble() == Approx(1.0));
    CHECK(At(dump, "enemies[0].hp").AsInt64() == 1);
    CHECK(At(dump, "enemies[0].at.x").AsDouble() == Approx(4.0));
    CHECK(At(dump, "enemies[1].hp").AsInt64() == 2);
    CHECK(At(dump, "awake[1]").AsBool());
    CHECK_FALSE(Has(dump, "best"));
    CHECK(At(dump, "mode").AsInt64() == 0);
    CHECK(At(dump, "tint.b").AsInt64() == 3);
    CHECK(At(dump, "name").AsString() == "run");
    CHECK(At(dump, "score").AsInt64() == 12);
    CHECK_FALSE(Has(dump, "NotAField"));

    run.best = 40;
    StateDump grouped;
    grouped.Field("run", run);
    CHECK(At(grouped, "run.best").AsInt64() == 40);
    CHECK(At(grouped, "run.enemies[0].hp").AsInt64() == 1);
}

TEST_CASE("Engine.State.OwnStateWinsAndNestedStructsGroup", "[engine][simulation]") {
    Recorder recorder;
    recorder.Field("outer", StateFieldsOuter{});
    CHECK(recorder.text == "outer{boss{hp;at{x;y;}}custom{shown;}}");
}

TEST_CASE("Engine.State.ReflectedFieldsHashLikeWrittenOnes", "[engine][simulation]") {
    StateFieldsEnemy enemy{.hp = 4, .at = {1.0f, 2.0f}};
    StateHash reflected;
    reflected.Fields(enemy);
    StateHash written;
    written.Field("hp", enemy.hp);
    written.Field("at", enemy.at);
    CHECK(reflected.Result() == written.Result());

    StateFieldsRun a;
    StateFieldsRun b;
    b.awake[1] = true; // A field no State line names still reaches the hash
    StateHash first;
    first.Fields(a);
    StateHash second;
    second.Fields(b);
    CHECK(first.Result() != second.Result());

    a.best = 0; // An empty optional and a held 0 differ too
    StateHash third;
    third.Fields(a);
    CHECK(third.Result() != first.Result());
}

TEST_CASE("Engine.State.AnUnnamedGroupIsItsEnclosingGroup", "[engine][simulation]") {
    StateDump dump;
    dump.Group("list", [&] {
        dump.Group({}, 0, [&] { dump.Field("hp", 1); });
        dump.Field("count", 1);
    });
    dump.Field("after", 2);
    CHECK(Has(dump, "list[0].hp"));
    CHECK(Has(dump, "list.count"));
    CHECK(Has(dump, "after"));
}
