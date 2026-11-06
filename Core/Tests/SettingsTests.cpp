#include "Tests/Support/Diagnostics.h"
#include <filesystem>
#include <optional>
#include <string>
#include "Platform/Internals/FileStorage.h"
#include "Runtime/ProjectSettingsStore.h"
#include "Tests/Support/TempDirectory.h"
#include "Tuning/Internals/TuningJson.h"
#include "Tuning/TuningFile.h"
#include "Tuning/TuningLive.h"

namespace {
void Parse(const std::string& kind, const std::string& text) {
    if (kind == "schema")
        (void)ParseSettingsSchema(text);
    else if (kind == "tuning")
        (void)ParseTuningFile(text);
    else
        (void)ParseSettingsValues(text, ProjectSettingsSchema());
}
} // namespace

TEST_CASE("Engine.Settings.SharedCasesMatchTheGenerator", "[engine][settings]") {
    const std::filesystem::path cases(ENGINE_SETTINGS_CASES_DIR);
    const auto list = ParseTuningJson(ReadBinaryFile(cases / "cases.json"));
    const auto* entries = list.Find("cases");
    REQUIRE(entries);
    REQUIRE_FALSE(entries->items.empty());
    for (const auto& entry : entries->items) {
        const std::string file = entry.Find("file")->text;
        const std::string kind = entry.Find("kind")->text;
        const auto* line = entry.Find("line");
        INFO(file);
        const std::string text = ReadBinaryFile(cases / file);
        if (line->kind == TuningJsonKind::Null) {
            CHECK_NOTHROW(Parse(kind, text));
            continue;
        }
        try {
            Parse(kind, text);
            FAIL("expected an error on line " << line->text);
        } catch (const TuningFormatError& error) {
            INFO(error.what());
            CHECK(std::to_string(error.Line()) == line->text);
        }
    }
}

TEST_CASE("Engine.Settings.ValuesFileFillsTheStruct", "[engine][settings]") {
    const auto settings = ReadProjectSettings(R"({ "title": "Café", "canvasWidth": 640, "smoothCanvas": false,
        "background": "#10203040", "font": "Fonts/A.ttf" })");
    CHECK(settings.title == "Caf\xC3\xA9");
    CHECK(settings.canvasWidth == 640);
    CHECK(settings.canvasHeight == ProjectSettings{}.canvasHeight); // The default
    CHECK_FALSE(settings.smoothCanvas);
    CHECK(settings.background == Color4{0x10, 0x20, 0x30, 0x40});
    CHECK(settings.font == "Fonts/A.ttf");
    CHECK(ReadProjectSettings("{}") == ProjectSettings{});
}

TEST_CASE("Engine.Settings.SchemaDefaultsMatchTheStruct", "[engine][settings]") {
    // The committed header comes from the schema (check --fix); its defaults are what an empty file reads.
    const auto& schema = ProjectSettingsSchema();
    REQUIRE(schema.size() > 10);
    CHECK(schema.front().name == "title");
    const auto values = ParseSettingsValues("{}", schema);
    CHECK(values.set.empty());
    CHECK(values.fields.size() == schema.size());
}

TEST_CASE("Engine.Settings.PatchSetsAndRemovesInSchemaOrder", "[engine][settings]") {
    const auto& schema = ProjectSettingsSchema();
    auto text = PatchSettingsValue("{}", schema, "canvasWidth", TuningValue{640});
    CHECK(text == "{\n  \"canvasWidth\": 640\n}\n");
    text = PatchSettingsValue(text, schema, "title", TuningValue{TuningText{"My \"Game\""}});
    CHECK(text == "{\n  \"title\": \"My \\\"Game\\\"\",\n  \"canvasWidth\": 640\n}\n");
    CHECK(ParseSettingsValues(text, schema).set == std::vector<std::string>{"title", "canvasWidth"});
    text = PatchSettingsValue(text, schema, "canvasWidth", std::nullopt);
    CHECK(text == "{\n  \"title\": \"My \\\"Game\\\"\"\n}\n");
    CHECK(PatchSettingsValue(text, schema, "title", std::nullopt) == "{}\n");
    // Other values keep their spelling; invalid values write nothing.
    CHECK(PatchSettingsValue("{\"targetFps\": 60, \"canvasHeight\": 360}", schema, "title",
                             TuningValue{TuningText{"A"}}) ==
          "{\n  \"title\": \"A\",\n  \"targetFps\": 60,\n  \"canvasHeight\": 360\n}\n");
    CHECK_THROWS_AS(PatchSettingsValue("{}", schema, "canvasWidth", TuningValue{0}), TuningFormatError);
    CHECK_THROWS_AS(PatchSettingsValue("{}", schema, "icon", TuningValue{TuningPath{"../x.ico"}}), TuningFormatError);
    CHECK_THROWS_AS(PatchSettingsValue("{}", schema, "title", TuningValue{3}), TuningFormatError);
    CHECK_THROWS_AS(PatchSettingsValue("{}", schema, "bogus", TuningValue{3}), TuningFormatError);
}

TEST_CASE("Engine.Settings.PathsAndVersions", "[engine][settings]") {
    CHECK(IsTuningPath(""));
    CHECK(IsTuningPath("Game/Icon.ico"));
    CHECK_FALSE(IsTuningPath("Game\\Icon.ico"));
    CHECK_FALSE(IsTuningPath("/Icon.ico"));
    CHECK_FALSE(IsTuningPath("Game/../Icon.ico"));
    CHECK_FALSE(IsTuningPath("Game/"));
    CHECK(IsTuningVersion("1"));
    CHECK(IsTuningVersion("1.2.3.65535"));
    CHECK_FALSE(IsTuningVersion("1.2.3.4.5"));
    CHECK_FALSE(IsTuningVersion("1.65536"));
    CHECK_FALSE(IsTuningVersion("01"));
    CHECK_FALSE(IsTuningVersion("1."));
}

TEST_CASE("Engine.Settings.DevelopmentReadsTheFileOverTheBuiltValues", "[engine][settings]") {
    const TempDirectory project;
    std::filesystem::create_directories(project.Path() / "Game");
    const auto previousRoot = TuningRoot();
    SetTuningRoot(project.Path());
    ProjectSettings built;
    built.title = "Built";

    RegisterProjectSettings(built, false); // Release: the built values, whatever the file says
    REQUIRE(WriteBinaryFileAtomic(project.Path() / "Game" / "Game.settings.json", "{ \"title\": \"File\" }"));
    CHECK(CurrentProjectSettings().title == "Built");

    RegisterProjectSettings(built, true);
    CHECK(CurrentProjectSettings().title == "File");

    REQUIRE(WriteBinaryFileAtomic(project.Path() / "Game" / "Game.settings.json", "{ \"title\": 3 }"));
    RegisterProjectSettings(built, true);
    CHECK(CurrentProjectSettings().title == "Built"); // Invalid: the built values, with a warning

    RegisterProjectSettings(ProjectSettings{}, false);
    SetTuningRoot(previousRoot);
}
