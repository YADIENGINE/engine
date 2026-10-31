#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "Editor/Internals/Protocol/PreviewProtocol.h"
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Tests/Support/TempDirectory.h"

namespace {
constexpr std::string_view Sample = "{\n  \"radius\": { \"value\": 12, \"min\": 0, \"max\": 40 }\n}\n";

void Write(const std::filesystem::path& file, std::string_view text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}

std::string ReadText(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

// Build dizini + listede tek dosya.
struct Project {
    TempDirectory temp;
    std::filesystem::path source = temp.Path() / "Game" / "Glow.tuning.json";
    TuningWorkspace workspace;

    Project() {
        Write(source, Sample);
        const auto build = temp.Path() / "build";
        Write(build / "Tuning" / "TuningFiles.json",
              "{ \"version\": 1, \"files\": [ { \"target\": \"MyGame\", \"source\": \"" + source.generic_string() +
                  "\" } ] }");
        workspace.Open(build);
    }
};
} // namespace

TEST_CASE("Editor.Protocol.TuningStatusRoundTrips", "[engine-editor]") {
    const PreviewTuningStatus status{"C:/MyGame/Glow.tuning.json", PreviewTuningState::NeedsBuild, 0, "new field",
                                     true};
    const auto decoded = DecodePreviewTuning(EncodePreviewTuning(status));
    CHECK(decoded.source == status.source);
    CHECK(decoded.state == status.state);
    CHECK(decoded.text == status.text);
    CHECK(decoded.reload);
    auto unknown = EncodePreviewTuning(status);
    unknown["state"] = "later";
    CHECK_THROWS(DecodePreviewTuning(unknown));
}

TEST_CASE("Editor.Tuning.WorkspaceReadsTheBuildList", "[engine-editor]") {
    Project project;
    const auto& files = project.workspace.Files();
    REQUIRE(files.size() == 1);
    CHECK(files[0].label == "Glow.tuning.json");
    CHECK(files[0].target == "MyGame");
    REQUIRE(files[0].fields.size() == 1);
    CHECK_FALSE(files[0].problem);
}

TEST_CASE("Editor.Tuning.SetPatchesOnlyValidValues", "[engine-editor]") {
    Project project;
    CHECK(project.workspace.Set(0, {"radius"}, 20.0f).empty());
    const std::string written = ReadText(project.source);
    CHECK(written == std::string(Sample).replace(Sample.find("12"), 2, "20.0"));
    CHECK(std::get<float>(project.workspace.Files()[0].fields[0].values[0]) == 20.0f);

    CHECK_FALSE(project.workspace.Set(0, {"radius"}, 41.0f).empty()); // max 40
    CHECK_FALSE(project.workspace.Set(0, {"radius"}, 3).empty());     // float alan
    CHECK(ReadText(project.source) == written);

    Write(project.source, "{ \"radius\": "); // Dışarıdan yarım yazılmış dosyaya dokunulmaz
    CHECK_FALSE(project.workspace.Set(0, {"radius"}, 5.0f).empty());
    CHECK(ReadText(project.source) == "{ \"radius\": ");
    CHECK(project.workspace.Files()[0].problem);
}

TEST_CASE("Editor.Tuning.WorkerReportsMatchTheFile", "[engine-editor]") {
    Project project;
    std::string source = project.source.generic_string();
    for (auto& c : source)
        if (c == '/') c = '\\';
    CHECK(project.workspace.Report({source, PreviewTuningState::Invalid, 3, "bad"}) == "Glow.tuning.json");
    REQUIRE(project.workspace.Files()[0].worker);
    CHECK(project.workspace.Files()[0].worker->line == 3);
    CHECK(project.workspace.Report({"C:/Elsewhere/Other.tuning.json"}).empty());
    project.workspace.ClearWorkerStatus();
    CHECK_FALSE(project.workspace.Files()[0].worker);
}
