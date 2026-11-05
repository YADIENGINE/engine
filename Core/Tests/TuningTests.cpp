#include "Tests/Support/Diagnostics.h"
#include <filesystem>
#include <string>
#include <vector>
#include "Platform/Internals/FileStorage.h"
#include "TestValues.tuning.h"
#include "Tuning/Internals/TuningJson.h"
#include "Tuning/TuningFile.h"
#if ENGINE_TUNING_LIVE
#include <algorithm>
#include "Simulation/ReplayCheck.h"
#include "Tests/Support/TempDirectory.h"
#include "Tuning/TuningLive.h"
#endif

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
    STATIC_REQUIRE(Values.mode == TestValuesTuning::Mode::Pulse);
    STATIC_REQUIRE(TestValuesTuning::ModeNames[2] == "Wave");
    CHECK(Values.moods == std::array{TestValuesTuning::Moods::Wild, TestValuesTuning::Moods::Calm});
    CHECK(Values.speed == 2.5f);
    CHECK(Values.kinds[1].kind == TestValuesTuning::KindsRow::Kind::Big);
    CHECK(Values.kinds[1].weight == 30);

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

TEST_CASE("Engine.Tuning.EditorTextAndChoicesAreRead", "[engine][tuning]") {
    const auto fields = ParseTuningFile(FixtureText());
    REQUIRE(fields.size() == 12);
    const auto& mode = fields[8];
    CHECK(mode.spec.type == TuningType::Choice);
    CHECK(mode.spec.choices == std::vector<std::string>{"Off", "Pulse", "Wave"});
    CHECK(std::get<TuningChoice>(mode.values[0]).index == 1);
    CHECK(mode.group == "Look");
    CHECK(mode.note == "How it moves");
    CHECK(fields[10].spec.unit == "px/s");
    const auto& kinds = fields[11];
    CHECK(kinds.note == "Weights per kind");
    CHECK(kinds.columns[1].spec.unit == "%");
    CHECK(std::get<TuningChoice>(kinds.rows[1][0][0]).index == 1);
}

TEST_CASE("Engine.Tuning.PatchWritesChoicesByName", "[engine][tuning]") {
    const std::string text = FixtureText();
    CHECK(PatchTuningValue(text, {"mode"}, TuningChoice{2}) ==
          Replaced(text, "\"value\": \"Pulse\"", "\"value\": \"Wave\""));
    CHECK(PatchTuningValue(text, {"kinds", 0, "kind", 0}, TuningChoice{1}) ==
          Replaced(text, "\"kind\": \"Small\"", "\"kind\": \"Big\""));
    CHECK_THROWS_AS(PatchTuningValue(text, {"mode"}, TuningChoice{3}), TuningFormatError);
    CHECK_THROWS_AS(PatchTuningValue(text, {"mode"}, 1), TuningFormatError);
}

TEST_CASE("Engine.Tuning.ResizeCopiesTheLastEntryOrRemovesOne", "[engine][tuning]") {
    const std::string text = FixtureText();
    CHECK(ResizeTuningField(text, {"steps"}, TuningResize::Append) ==
          Replaced(text, "[10, 15, 20]", "[10, 15, 20, 20]"));
    CHECK(ResizeTuningField(text, {"steps", 0, {}, 0}, TuningResize::Remove) ==
          Replaced(text, "[10, 15, 20]", "[15, 20]"));
    CHECK(ResizeTuningField(text, {"steps", 0, {}, 2}, TuningResize::Remove) ==
          Replaced(text, "[10, 15, 20]", "[10, 15]"));
    const std::string row = "      { \"from\": 5, \"chances\": [55, 45] }";
    CHECK(ResizeTuningField(text, {"odds"}, TuningResize::Append) == Replaced(text, row, row + ",\n" + row));
    CHECK(ResizeTuningField(text, {"odds", 1}, TuningResize::Remove) == Replaced(text, ",\n" + row, ""));
    CHECK(ParseTuningFile(ResizeTuningField(text, {"odds"}, TuningResize::Append))[7].rows.size() == 3);

    const std::string single =
        "{\n  \"t\": {\n    \"type\": \"table\",\n    \"columns\": { \"a\": { \"type\": \"int\" } },"
        "\n    \"rows\": [\n      { \"a\": 1 }\n    ]\n  }\n}\n";
    CHECK(ResizeTuningField(single, {"t"}, TuningResize::Append) ==
          Replaced(single, "      { \"a\": 1 }", "      { \"a\": 1 },\n      { \"a\": 1 }"));
    CHECK_THROWS_AS(ResizeTuningField(single, {"t"}, TuningResize::Remove), TuningFormatError); // One stays
    CHECK_THROWS_AS(ResizeTuningField(text, {"radius"}, TuningResize::Append), TuningFormatError);
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

    Collected rechosen; // A new choice changes the enum
    const auto more =
        ApplyTuningText(Replaced(text, "\"Pulse\", \"Wave\"]", "\"Pulse\", \"Wave\", \"Spin\"]"), rechosen.fields);
    CHECK(more.outcome == TuningOutcome::NeedsBuild);
}

TEST_CASE("Engine.Tuning.ApplyWritesChoices", "[engine][tuning]") {
    Collected target;
    std::string text = Replaced(FixtureText(), "\"value\": \"Pulse\"", "\"value\": \"Off\"");
    text = Replaced(text, "\"kind\": \"Big\"", "\"kind\": \"Small\"");
    REQUIRE(ApplyTuningText(text, target.fields).outcome == TuningOutcome::Applied);
    CHECK(target.values.mode == TestValuesTuning::Mode::Off);
    CHECK(target.values.kinds[1].kind == TestValuesTuning::KindsRow::Kind::Small);
}

TEST_CASE("Engine.Tuning.LiveFileIsFoundFromTheProjectRoot", "[engine][tuning]") {
    CHECK(std::filesystem::exists(TuningRoot() / "Project.json"));
    const auto& read = Tuning<TestValuesTuning>();
    CHECK(read.mode == TestValuesTuning::Mode::Pulse);
    const auto source = TuningRoot() / std::filesystem::path(TestValuesTuning::TuningSource);
    CHECK(std::filesystem::equivalent(source, std::filesystem::path(ENGINE_TUNING_TEST_FILE)));
}

TEST_CASE("Engine.Tuning.UsePeriodsListTheFilesRead", "[engine][tuning]") {
    (void)Tuning<TestValuesTuning>();
    BeginTuningUse();
    const auto revision = TuningUseRevision();
    CHECK(TuningUsed().empty());
    (void)Tuning<TestValuesTuning>();
    (void)Tuning<TestValuesTuning>();
    const auto used = TuningUsed();
    REQUIRE(used.size() == 1);
    CHECK(std::filesystem::equivalent(used[0], std::filesystem::path(ENGINE_TUNING_TEST_FILE)));
    CHECK(TuningUseRevision() == revision + 1);
}

TEST_CASE("Engine.Tuning.PreviewWritesOneValueUntilTheFileIsRead", "[engine][tuning]") {
    const std::filesystem::path file(ENGINE_TUNING_TEST_FILE);
    const float original = Tuning<TestValuesTuning>().radius;
    const auto before = TuningDigests();
    CHECK(PreviewTuning(file, {"radius"}, 30.0f).empty());
    CHECK(Tuning<TestValuesTuning>().radius == 30.0f);
    CHECK(TuningDigests() != before);
    CHECK(PreviewTuning(file, {"kinds", 1, "kind", 0}, TuningChoice{0}).empty());
    CHECK(Tuning<TestValuesTuning>().kinds[1].kind == TestValuesTuning::KindsRow::Kind::Small);
    CHECK_FALSE(PreviewTuning(file, {"radius"}, 3).empty());             // Wrong type
    CHECK_FALSE(PreviewTuning(file, {"mode"}, TuningChoice{9}).empty()); // No such choice
    CHECK_FALSE(PreviewTuning(file, {"missing"}, 1.0f).empty());
    CHECK_FALSE(PreviewTuning(file.parent_path() / "Other.tuning.json", {"radius"}, 1.0f).empty());
    ReloadTuning(file);
    CHECK(Tuning<TestValuesTuning>().radius == original);
    CHECK(TuningDigests() == before);
}

TEST_CASE("Engine.Tuning.TapeNamesTheFilesThatChanged", "[engine][tuning]") {
    (void)Tuning<TestValuesTuning>();
    // The generated name, relative to the project: "Engine/Core/..." when the engine sits inside a game project
    const std::string source(TestValuesTuning::TuningSource.begin(), TestValuesTuning::TuningSource.end());
    const auto now = TuningDigests();
    const auto current = std::ranges::find(now, source, &TuningDigest::source);
    REQUIRE(current != now.end());

    InputTape tape;
    tape.tuning = {{source, current->hash}, {"Game/NotReadHere.tuning.json", 1}};
    CHECK(TuningChanged(tape).empty()); // Same values; a file this process never read is left out
    tape.tuning[0].hash ^= 1U;
    CHECK(TuningChanged(tape) == std::vector<std::string>{source});

    tape.hashes = {7};
    TempDirectory directory;
    tape.Save(directory.Path() / "run.replay");
    CHECK(InputTape::Load(directory.Path() / "run.replay").tuning == tape.tuning);
}
#endif
