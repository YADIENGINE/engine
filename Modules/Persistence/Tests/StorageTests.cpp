#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include "Tests/Support/TempDirectory.h"
#include "Modules/Persistence/Persistence.hpp"

namespace {
void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Could not write test save fixture");
    output << text;
    if (!output) throw std::runtime_error("Could not finish test save fixture");
}
} // namespace

TEST_CASE("Persistence.Values.FallbackEraseAndKeyValidation", "[persistence]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "progress.json");
    CHECK_FALSE(save.Contains("missing"));
    CHECK(save.GetInt("missing", 42) == 42);
    save.SetInt("valid.Key_1-2", 123);
    CHECK(save.Contains("valid.Key_1-2"));
    CHECK(save.GetInt("valid.Key_1-2") == 123);
    save.Erase("valid.Key_1-2");
    CHECK_FALSE(save.Contains("valid.Key_1-2"));
    CHECK_THROWS_AS(save.SetInt("", 1), std::invalid_argument);
    save.SetInt("any UTF-8 / key", 1);
    CHECK(save.GetInt("any UTF-8 / key") == 1);
    CHECK_THROWS_AS(Persistence(std::filesystem::path{}), std::invalid_argument);
}

TEST_CASE("Persistence.RoundTrip.PreservesIntegerLimitsAndCreatesDirectories", "[persistence]") {
    TempDirectory temp;
    const auto path = temp.Path() / "nested" / "progress.json";
    Persistence save(path);
    save.SetInt("minimum", std::numeric_limits<std::int64_t>::min());
    save.SetInt("maximum", std::numeric_limits<std::int64_t>::max());
    save.SetInt("settings.language", 2);
    save.Save();
    CHECK(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    Persistence loaded(path);
    loaded.Load();
    CHECK(loaded.GetInt("minimum") == std::numeric_limits<std::int64_t>::min());
    CHECK(loaded.GetInt("maximum") == std::numeric_limits<std::int64_t>::max());
    CHECK(loaded.GetInt("settings.language") == 2);
    loaded.SetInt("settings.language", 1);
    loaded.Save();
    save.Load();
    CHECK(save.GetInt("settings.language") == 1);
}

TEST_CASE("Persistence.MissingFile.LoadStartsEmpty", "[persistence]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "absent.json");
    save.SetInt("existing", 10);
    CHECK_NOTHROW(save.Load());
    CHECK_FALSE(save.Contains("existing"));
    CHECK_FALSE(std::filesystem::exists(save.Path()));
}

TEST_CASE("Persistence.MalformedOrUnsupported.PreservesMemory", "[persistence]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "progress.json");
    for (const auto text : {
             "garbage",
             "{}",
             "{\"version\":3,\"values\":{}}",
             "{\"version\":0,\"values\":{}}",
             "{\"version\":2,\"values\":[]}",
             "{\"version\":2,\"values\":{\"\":2}}",
             "{\"version\":2,\"values\":{\"key\":1,\"key\":2}}",
             "{\"version\":2,\"values\":{}} trailing",
         }) {
        CAPTURE(text);
        save.SetInt("existing", 73);
        WriteFile(save.Path(), text);
        CHECK_THROWS(save.Load());
        CHECK(save.GetInt("existing") == 73);
        CHECK_FALSE(save.Contains("key"));
    }
}

TEST_CASE("Persistence.OldVersion.ReportsOutdatedAndPreservesMemory", "[persistence]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "progress.json", {.keepBackup = true});
    save.SetInt("existing", 73);
    WriteFile(save.Path(), "{\"version\":1,\"values\":{\"key\":5}}");
    WriteFile(save.BackupPath(), "{\"version\":1,\"values\":{\"key\":5}}");
    try {
        (void)save.Load();
        FAIL("Expected SaveError");
    } catch (const SaveError& error) {
        CHECK(error.Code() == SaveErrorCode::Outdated);
    }
    CHECK(save.GetInt("existing") == 73);
    CHECK_FALSE(save.Contains("key"));
}

TEST_CASE("Persistence.WriteFailure.RemovesTemporaryAndPreservesDestination", "[persistence]") {
    TempDirectory temp;
    const auto destination = temp.Path() / "occupied";
    std::filesystem::create_directory(destination);
    WriteFile(destination / "keep.txt", "keep");
    Persistence save(destination);
    save.SetInt("value", 1);
    CHECK_THROWS(save.Save());
    CHECK(std::filesystem::exists(destination / "keep.txt"));
    CHECK_FALSE(std::filesystem::exists(destination.string() + ".tmp"));
}
