#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include "Modules/RunTracking/RunTracking.h"
#include "Tests/Support/GzipInflate.h"
#include "Tests/Support/TempDirectory.h"

namespace {
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, {}};
}
std::string ReadJson(const std::filesystem::path& path) { return InflateGzip(Read(path)); }
} // namespace
TEST_CASE("RunTracking.Archive.EscapesJsonAndReplacesOneRecord", "[engine][run-tracking][run-statistics]") {
    TempDirectory temporary;
    JsonIO::Document record = JsonIO::Document::MakeObject();
    record["schema_version"] = 1;
    record["label"] = "quote\" slash\\\nTürkçe";
    record["choices"] = JsonIO::Document::Array{JsonIO::Document{}, true, 2, 1.25};
    record["wallet_eggs"] = 123;
    RunTracking archive(temporary.Path() / "runs");
    const auto file = archive.Write("run-1", record);
    CHECK(file.filename() == "run-1.json.gz");
    CHECK(ReadJson(file) == JsonIO::Serialize(record, {.format = JsonIO::Format::Compact}));
    CHECK(ReadJson(file).find(R"(quote\" slash\\\nTürkçe)") != std::string::npos);
    CHECK(ReadJson(file).find(R"("choices":[null,true,2,1.25])") != std::string::npos);
    CHECK(ReadJson(file).find(R"("wallet_eggs":123)") != std::string::npos);
    record["schema_version"] = 2;
    CHECK(archive.Write("run-1", record) == file);
    CHECK(ReadJson(file).find(R"("schema_version":2)") != std::string::npos);
    CHECK(std::distance(std::filesystem::directory_iterator(archive.Directory()),
                        std::filesystem::directory_iterator{}) == 1);
}
TEST_CASE("RunTracking.Archive.RejectsInvalidDataWithoutDamagingPriorRecord",
          "[engine][run-tracking][run-statistics]") {
    TempDirectory temporary;
    RunTracking archive(temporary.Path() / "runs");
    const auto file = archive.Write("stable", JsonIO::Document::Object{{"score", 42}});
    const std::string previous = Read(file);
    CHECK_THROWS_AS(archive.Write("../escape", {}), std::invalid_argument);
    CHECK_THROWS_AS(archive.Write("stable", JsonIO::Document(std::numeric_limits<double>::infinity())), JsonIO::Error);
    CHECK(Read(file) == previous);
    CHECK(std::distance(std::filesystem::directory_iterator(archive.Directory()),
                        std::filesystem::directory_iterator{}) == 1);
    CHECK_THROWS_AS(RunTracking(std::filesystem::path{}), std::invalid_argument);
}
