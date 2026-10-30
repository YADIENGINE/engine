#include "Tests/Support/Diagnostics.h"
#include <filesystem>
#include <string>
#include <vector>
#include "Platform/Internals/FileStorage.h"
#include "TestValues.tuning.h"
#include "Tuning/Internals/TuningJson.h"
#include "Tuning/TuningFile.h"

namespace {
std::string Replaced(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

std::string FixtureText() { return ReadBinaryFile(std::filesystem::path(ENGINE_TUNING_TEST_FILE)); }
} // namespace

TEST_CASE("Engine.Tuning.SharedCasesMatchTheGenerator", "[engine][tuning]") {
    const std::filesystem::path cases(ENGINE_TUNING_CASES_DIR);
    const auto list = ParseTuningJson(ReadBinaryFile(cases / "cases.json"));
    const auto* entries = list.Find("cases");
    REQUIRE(entries);
    REQUIRE_FALSE(entries->items.empty());
    for (const auto& entry : entries->items) {
        const std::string file = entry.Find("file")->text;
        const auto* line = entry.Find("line");
        INFO(file);
        const std::string text = ReadBinaryFile(cases / file);
        if (line->kind == TuningJsonKind::Null) {
            CHECK_NOTHROW((void)ParseTuningFile(text));
            continue;
        }
        try {
            (void)ParseTuningFile(text);
            FAIL("expected an error on line " << line->text);
        } catch (const TuningFormatError& error) {
            INFO(error.what());
            CHECK(std::to_string(error.Line()) == line->text);
        }
    }
}

TEST_CASE("Engine.Tuning.GeneratedStructCarriesTheFileValues", "[engine][tuning]") {
    constexpr TestValuesTuning Values{};
    STATIC_REQUIRE(Values.count == 3);
    CHECK(Values.radius == 12.5f);
    CHECK(Values.enabled);
    CHECK(Values.tint == Color4{255, 64, 128, 255});
    CHECK(Values.offset.x == 1.5f);
    CHECK(Values.offset.y == -2.0f);
    CHECK(Values.spread.min == 1.0f);
    CHECK(Values.spread.max == 3.0f);
    CHECK(Values.steps == std::array{10, 15, 20});
    REQUIRE(Values.odds.size() == 2);
    CHECK(Values.odds[1].from == 5);
    CHECK(Values.odds[1].chances == std::array{55.0f, 45.0f});

    const auto& read = Tuning<TestValuesTuning>();
    CHECK(read.radius == Values.radius);
    CHECK(read.odds[0].chances == Values.odds[0].chances);
}

TEST_CASE("Engine.Tuning.PatchChangesOnlyThatValue", "[engine][tuning]") {
    const std::string text = FixtureText();
    const auto patched = PatchTuningValue(text, {"odds", 1, "chances", 0}, 60.0f);
    CHECK(patched == Replaced(text, "[55, 45]", "[60.0, 45]"));
    CHECK(std::get<float>(ParseTuningFile(patched)[7].rows[1][1][0]) == 60.0f);

    CHECK(PatchTuningValue(text, {"tint"}, Color4{1, 2, 3, 4}) == Replaced(text, "\"#FF4080\"", "\"#01020304\""));
    CHECK(PatchTuningValue(text, {"spread"}, FloatRange{0.5f, 2.0f}) ==
          Replaced(text, "{ \"min\": 1, \"max\": 3 }", "{ \"min\": 0.5, \"max\": 2.0 }"));
    CHECK(PatchTuningValue(text, {"steps", 0, {}, 2}, 25) == Replaced(text, "20]", "25]"));
}

TEST_CASE("Engine.Tuning.PatchRejectsInvalidValues", "[engine][tuning]") {
    const std::string text = FixtureText();
    CHECK_THROWS_AS(PatchTuningValue(text, {"radius"}, 41.0f), TuningFormatError);      // max 40
    CHECK_THROWS_AS(PatchTuningValue(text, {"count"}, 1.5f), TuningFormatError);        // int alan
    CHECK_THROWS_AS(PatchTuningValue(text, {"missing"}, 1.0f), TuningFormatError);      // alan yok
    CHECK_THROWS_AS(PatchTuningValue(text, {"steps", 0, {}, 3}, 1), TuningFormatError); // eleman yok
    CHECK_THROWS_AS(PatchTuningValue("{", {"radius"}, 1.0f), TuningFormatError);        // dosya bozuk
}

#if ENGINE_TUNING_LIVE
namespace {
struct Collected {
    TestValuesTuning values;
    std::vector<TuningFieldRef> fields;

    Collected() {
        TuningCollector collector(fields);
        values.VisitTuning(collector);
    }
};
} // namespace

TEST_CASE("Engine.Tuning.ApplyWritesEveryValue", "[engine][tuning]") {
    Collected target;
    std::string text = Replaced(FixtureText(), "\"value\": 12.5", "\"value\": 20");
    text = Replaced(text, "[55, 45]", "[50, 50]");
    const auto report = ApplyTuningText(text, target.fields);
    CHECK(report.outcome == TuningOutcome::Applied);
    CHECK(target.values.radius == 20.0f);
    CHECK(target.values.odds[1].chances == std::array{50.0f, 50.0f});
}

TEST_CASE("Engine.Tuning.ApplyIsAllOrNothing", "[engine][tuning]") {
    const TestValuesTuning original{};
    const std::string text = Replaced(FixtureText(), "\"value\": 12.5", "\"value\": 20");

    Collected invalid;
    const auto broken = ApplyTuningText(Replaced(text, "[55, 45]", "[55, 145]"), invalid.fields);
    CHECK(broken.outcome == TuningOutcome::Invalid);
    CHECK(broken.line == 17);
    CHECK(invalid.values.radius == original.radius);

    Collected added;
    const auto grown = ApplyTuningText(Replaced(text, "{\n", "{\n  \"extra\": { \"value\": 1 },\n"), added.fields);
    CHECK(grown.outcome == TuningOutcome::NeedsBuild);
    CHECK(added.values.radius == original.radius);

    Collected resized;
    const auto longer = ApplyTuningText(Replaced(text, "[10, 15, 20]", "[10, 15, 20, 25]"), resized.fields);
    CHECK(longer.outcome == TuningOutcome::NeedsBuild);
    CHECK(resized.values.radius == original.radius);

    Collected retyped;
    const auto changed =
        ApplyTuningText(Replaced(text, "{ \"type\": \"int\", \"value\": 3 }", "{ \"value\": 3 }"), retyped.fields);
    CHECK(changed.outcome == TuningOutcome::NeedsBuild);
    CHECK(retyped.values.radius == original.radius);
}
#endif
