#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Editor/Internals/Session/TuningBaselines.h"
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Tests/Support/TempDirectory.h"

namespace {
constexpr std::string_view Sample = "{\n  \"radius\": { \"value\": 12, \"min\": 0, \"max\": 40 },\n"
                                    "  \"steps\": { \"type\": \"int[]\", \"value\": [1, 2] }\n}\n";

void Write(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

std::string ReadText(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

std::string Quoted(const std::filesystem::path& path) { return "\"" + path.generic_string() + "\""; }

// A project with one built tuning file under the root "Game".
struct Project {
    TempDirectory temp;
    std::filesystem::path source = temp.Path() / "Game" / "Glow.tuning.json";
    std::filesystem::path build = temp.Path() / "build";
    TuningWorkspace workspace;

    Project() {
        Write(source, Sample);
        Write(build / "Tuning" / "TuningFiles.json",
              "{ \"version\": 2, \"project\": " + Quoted(temp.Path()) + ", \"roots\": [ { \"target\": \"MyGame\", " +
                  "\"root\": " + Quoted(temp.Path() / "Game") + " } ], \"files\": [ { \"target\": \"MyGame\", " +
                  "\"source\": " + Quoted(source) + " } ] }");
        workspace.Open(build);
    }
    [[nodiscard]] const TuningFileState& File() const { return *workspace.Find(source); }
};
} // namespace

TEST_CASE("Editor.Protocol.TuningStatusRoundTrips", "[engine-editor]") {
    const WorkerTuningStatus status{"C:/MyGame/Glow.tuning.json", WorkerTuningState::NeedsBuild, 0, "new field", true};
    const auto decoded = DecodeWorkerTuning(EncodeWorkerTuning(status));
    CHECK(decoded.source == status.source);
    CHECK(decoded.state == status.state);
    CHECK(decoded.text == status.text);
    CHECK(decoded.reload);
    auto unknown = EncodeWorkerTuning(status);
    unknown["state"] = "later";
    CHECK_THROWS(DecodeWorkerTuning(unknown));
}

TEST_CASE("Editor.Protocol.TuningPreviewsRoundTrip", "[engine-editor]") {
    for (const TuningValue& value :
         {TuningValue{2.5f}, TuningValue{-3}, TuningValue{true}, TuningValue{Color4{1, 2, 3, 4}},
          TuningValue{Float2{1, -2}}, TuningValue{FloatRange{0.5f, 2}}, TuningValue{TuningChoice{2}}}) {
        const WorkerTuningPreview preview{"C:/MyGame/Glow.tuning.json", {"odds", 1, "chances", 3}, value};
        const auto decoded = DecodeWorkerTuningPreview(EncodeWorkerTuningPreview(preview));
        CHECK(decoded.source == preview.source);
        CHECK(decoded.at == preview.at);
        CHECK(decoded.value == value);
    }
    CHECK_THROWS(DecodeTuningValue({{"type", 3}, {"value", {1, 2, 3, 300}}}));
    CHECK_THROWS(DecodeTuningValue({{"type", 9}, {"value", 1}}));
    const std::vector<std::string> used{"C:/a.tuning.json", "C:/b.tuning.json"};
    CHECK(DecodeWorkerTuningUsed(EncodeWorkerTuningUsed(used)) == used);
}

TEST_CASE("Editor.Tuning.WorkspaceListsBuiltAndNewFiles", "[engine-editor]") {
    Project project;
    REQUIRE(project.workspace.Files().size() == 2); // With the project's settings file, which comes first
    CHECK(project.workspace.Files().front().kind == TuningFileKind::Settings);
    const auto& file = project.File();
    CHECK(file.label == "Glow.tuning.json");
    CHECK(file.relative == "Game/Glow.tuning.json");
    CHECK(file.target == "MyGame");
    CHECK(file.fields.size() == 2);
    CHECK_FALSE(file.problem);

    // A file under a root shows before any build has seen it; one outside the roots does not.
    Write(project.temp.Path() / "Game" / "Effects" / "Spark.tuning.json", Sample);
    Write(project.temp.Path() / "Elsewhere" / "Lost.tuning.json", Sample);
    project.workspace.Open(project.build);
    REQUIRE(project.workspace.Files().size() == 3);
    const auto* spark = project.workspace.Find(project.temp.Path() / "Game" / "Effects" / "Spark.tuning.json");
    REQUIRE(spark);
    CHECK(spark->target.empty());
    CHECK(spark->label == "Effects/Spark.tuning.json");
}

TEST_CASE("Editor.Tuning.SetPatchesOnlyValidValues", "[engine-editor]") {
    Project project;
    CHECK(project.workspace.Set(project.source, {"radius"}, 20.0f).empty());
    const std::string written = ReadText(project.source);
    CHECK(written == std::string(Sample).replace(Sample.find("12"), 2, "20.0"));
    CHECK(std::get<float>(project.File().fields[0].values[0]) == 20.0f);

    CHECK_FALSE(project.workspace.Set(project.source, {"radius"}, 41.0f).empty()); // max 40
    CHECK_FALSE(project.workspace.Set(project.source, {"radius"}, 3).empty());     // A float field
    CHECK(ReadText(project.source) == written);

    Write(project.source, "{ \"radius\": "); // A half-written file is left alone
    CHECK_FALSE(project.workspace.Set(project.source, {"radius"}, 5.0f).empty());
    CHECK(ReadText(project.source) == "{ \"radius\": ");
    CHECK(project.File().problem);
}

TEST_CASE("Editor.Tuning.SameValueWritesNothing", "[engine-editor]") {
    Project project;
    CHECK(project.workspace.Set(project.source, {"radius"}, 12.0f).empty());
    CHECK(ReadText(project.source) == Sample); // "12" is not respelled "12.0"
    CHECK_FALSE(project.workspace.CanUndo());
}

TEST_CASE("Editor.Tuning.UndoAndRedoFollowTheFile", "[engine-editor]") {
    Project project;
    REQUIRE(project.workspace.Set(project.source, {"radius"}, 20.0f).empty());
    const std::string after = ReadText(project.source);
    REQUIRE(project.workspace.CanUndo());
    const auto undone = project.workspace.Undo();
    REQUIRE(undone);
    CHECK(undone->done);
    CHECK(undone->source == project.source);
    CHECK(ReadText(project.source) == Sample);
    CHECK(std::get<float>(project.File().fields[0].values[0]) == 12.0f);
    REQUIRE(project.workspace.CanRedo());
    CHECK(project.workspace.Redo()->done);
    CHECK(ReadText(project.source) == after);

    // Someone else changed the file since: the edit is not undone over their change.
    Write(project.source, std::string(Sample).replace(Sample.find("12"), 2, "30"));
    const auto refused = project.workspace.Undo();
    REQUIRE(refused);
    CHECK_FALSE(refused->done);
    CHECK(ReadText(project.source).find("30") != std::string::npos);
    CHECK_FALSE(project.workspace.CanUndo());
}

TEST_CASE("Editor.Tuning.ResizeChangesALengthAndUndoes", "[engine-editor]") {
    Project project;
    CHECK(project.workspace.Resize(project.source, {"steps"}, TuningResize::Append).empty());
    CHECK(project.File().fields[1].values.size() == 3);
    CHECK(project.workspace.Resize(project.source, {"steps", 0, {}, 0}, TuningResize::Remove).empty());
    CHECK(project.File().fields[1].values.size() == 2);
    CHECK_FALSE(project.workspace.Resize(project.source, {"radius"}, TuningResize::Append).empty());
    (void)project.workspace.Undo();
    (void)project.workspace.Undo();
    CHECK(ReadText(project.source) == Sample);
}

TEST_CASE("Editor.Tuning.RevertWritesTheCommittedValues", "[engine-editor]") {
    Project project;
    const auto& file = project.File();
    CHECK_FALSE(file.committed);
    CHECK_FALSE(project.workspace.Revert(project.source, "radius").empty()); // Nothing committed to go back to

    project.workspace.SetCommitted(project.source, std::string(Sample).replace(Sample.find("[1, 2]"), 6, "[5, 6]"));
    REQUIRE(project.File().committed);
    CHECK(project.workspace.Revert(project.source, "steps").empty());
    CHECK(ReadText(project.source) == std::string(Sample).replace(Sample.find("[1, 2]"), 6, "[5, 6]"));

    project.workspace.SetCommitted(project.source, std::string(Sample).replace(Sample.find("[1, 2]"), 6, "[5]"));
    CHECK_FALSE(project.workspace.Revert(project.source, "steps").empty()); // Another length: git restores it
    project.workspace.SetCommitted(project.source, std::nullopt);
    CHECK_FALSE(project.File().committed);
}

TEST_CASE("Editor.Tuning.WorkerReportsMatchTheFile", "[engine-editor]") {
    Project project;
    std::string source = project.source.generic_string();
    for (auto& c : source)
        if (c == '/') c = '\\';
    CHECK(project.workspace.Report({source, WorkerTuningState::Invalid, 3, "bad"}) == "Glow.tuning.json");
    REQUIRE(project.File().worker);
    CHECK(project.File().worker->line == 3);
    CHECK(project.workspace.Report({"C:/Elsewhere/Other.tuning.json"}).empty());
    CHECK_FALSE(project.File().used);
    project.workspace.SetUsed({std::filesystem::path(source)});
    CHECK(project.File().used);
    project.workspace.ClearWorkerStatus();
    CHECK_FALSE(project.File().worker);
    CHECK_FALSE(project.File().used);
}

TEST_CASE("Editor.Tuning.BaselinesReadTheCommittedText", "[engine-editor]") {
    const auto executable = FindGitExecutable();
    if (executable.empty()) SKIP("git is not installed");
    TempDirectory temp;
    const auto file = temp.Path() / "Game" / "Glow.tuning.json";
    const auto untracked = temp.Path() / "Game" / "New.tuning.json";
    Write(file, Sample);
    Write(untracked, Sample);
    const GitClient git(executable, temp.Path());
    REQUIRE(git.Run({"init", "--quiet"}).ok);
    REQUIRE(git.Run({"add", "Game/Glow.tuning.json"}).ok);
    REQUIRE(git.Run({"-c", "user.name=Test", "-c", "user.email=test@example.com", "commit", "--quiet", "-m", "x"}).ok);
    Write(file, "changed");

    TuningBaselines baselines(temp.Path());
    baselines.Watch({file, untracked});
    std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> read;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (read.size() < 2 && std::chrono::steady_clock::now() < deadline) {
        for (auto& entry : baselines.Take())
            read.push_back(std::move(entry));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    REQUIRE(read.size() == 2);
    for (const auto& [path, text] : read) {
        if (path == file) {
            REQUIRE(text);
            CHECK(*text == Sample);
        } else {
            CHECK_FALSE(text); // Never committed
        }
    }
}


TEST_CASE("Editor.Settings.FirstEditWritesTheFileAndUndoReturnsToDefaults", "[engine-editor]") {
    Project project;
    const auto* settings = project.workspace.Settings();
    REQUIRE(settings);
    const auto source = settings->source;
    CHECK(source == project.temp.Path() / "Game" / "Game.settings.json");
    CHECK(settings->label == "Project settings");
    CHECK_FALSE(settings->exists);
    CHECK(settings->set.empty());
    REQUIRE(settings->Field("canvasWidth"));
    CHECK(settings->Field("canvasWidth")->values.front() == TuningValue{1280}); // The default

    CHECK(project.workspace.Set(source, {"canvasWidth"}, TuningValue{640}).empty());
    CHECK(ReadText(source) == "{\n  \"canvasWidth\": 640\n}\n");
    settings = project.workspace.Settings();
    CHECK(settings->exists);
    CHECK(settings->set == std::vector<std::string>{"canvasWidth"});
    CHECK(project.workspace.Set(source, {"canvasWidth"}, TuningValue{0}).find("outside") != std::string::npos);
    CHECK(project.workspace.Set(source, {"title"}, TuningValue{TuningText{"My game"}}).empty());
    CHECK(project.workspace.ResetToDefault(source, "canvasWidth").empty());
    CHECK(ReadText(source) == "{\n  \"title\": \"My game\"\n}\n");

    REQUIRE(project.workspace.Undo());
    CHECK(ReadText(source).find("640") != std::string::npos);
    REQUIRE(project.workspace.Undo());
    REQUIRE(project.workspace.Undo());
    CHECK(ReadText(source) == "{}\n"); // Back to every default
}

TEST_CASE("Editor.Settings.RevertWritesTheCommittedValueOrTheDefault", "[engine-editor]") {
    Project project;
    const auto source = project.workspace.Settings()->source;
    Write(source, "{\n  \"title\": \"Now\",\n  \"canvasWidth\": 640\n}\n");
    project.workspace.Open(project.build);
    project.workspace.SetCommitted(source, std::string("{ \"title\": \"Then\" }"));
    CHECK(project.workspace.Revert(source, "title").empty());
    CHECK(project.workspace.Revert(source, "canvasWidth").empty()); // The commit left it out: the default
    CHECK(ReadText(source) == "{\n  \"title\": \"Then\"\n}\n");
}

TEST_CASE("Editor.InputMap.KeysAreSetPerPlayerAndButton", "[engine-editor]") {
    Project project;
    const auto source = project.temp.Path() / "Game" / "Simulation" / "Pad.input.json";
    Write(source, "{\n  \"buttons\": [\"Left\", \"Jump\"],\n  \"axes\": [],\n  \"players\": [\n"
                  "    { \"Jump\": [\"Space\"] },\n    {}\n  ]\n}\n");
    project.workspace.Open(project.build);
    const auto* map = project.workspace.Find(source);
    REQUIRE(map);
    CHECK(map->kind == TuningFileKind::InputMap);
    REQUIRE(map->input);
    CHECK(map->input->players.size() == 2);

    CHECK(project.workspace.SetKeys(source, 1, "Left", {"Left", "A"}).empty());
    CHECK(ReadText(source).find("{ \"Left\": [\"Left\", \"A\"] }") != std::string::npos);
    CHECK_FALSE(project.workspace.SetKeys(source, 0, "Left", {"Spacebar"}).empty()); // Not a key name: not written
    CHECK(project.workspace.SetKeys(source, 0, "Jump", {}).empty());
    CHECK_FALSE(project.workspace.Find(source)->input->players[0][1].has_value());
    REQUIRE(project.workspace.Undo());
    CHECK(project.workspace.Find(source)->input->players[0][1] == std::vector<std::string>{"Space"});
}
