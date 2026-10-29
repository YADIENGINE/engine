#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <limits>
#include "Tests/Support/TempDirectory.h"
#include "Modules/Persistence/Persistence.hpp"
#include "Platform/Internals/FileStorage.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {
SaveSettings Protected(const std::filesystem::path& keyFile) {
    return {.encryption = SaveEncryptionMode::Encrypted, .keys = {.file = keyFile}};
}
void ExpectError(const std::function<void()>& action, SaveErrorCode code) {
    try {
        action();
        FAIL("Expected SaveError");
    } catch (const SaveError& error) {
        CHECK(error.Code() == code);
    }
}
} // namespace

TEST_CASE("Persistence.Encryption.PlaintextAndEncryptedCoexistWithoutPlaintextKeyAccess", "[persistence][encryption]") {
    TempDirectory temp;
    const auto keyFile = temp.Path() / "keys/save.key";
    const auto settings = Protected(keyFile);
    CHECK_THROWS_AS(Persistence(temp.Path() / "invalid.json", {.encryption = SaveEncryptionMode::Encrypted}),
                    std::invalid_argument);
    Persistence plain(temp.Path() / "settings.json", {.keys = {.file = keyFile}});
    plain.SetInt("setting", 2);
    plain.Save();
    CHECK(plain.Load() == SaveLoadResult::Loaded);
    CHECK_FALSE(std::filesystem::exists(keyFile));
    Persistence encrypted(temp.Path() / "progress.json", settings);
    CHECK(encrypted.Load() == SaveLoadResult::Missing);
    CHECK_FALSE(std::filesystem::exists(keyFile));
    encrypted.SetInt("minimum", std::numeric_limits<std::int64_t>::min());
    encrypted.SetInt("maximum", std::numeric_limits<std::int64_t>::max());
    encrypted.SetInt("progress.eggs", 123);
    encrypted.Save();
    CHECK(ReadBinaryFile(encrypted.Path()).starts_with("ENGSAVE!"));
    CHECK(ReadBinaryFile(encrypted.Path()).find("progress.eggs") == std::string::npos);
    Persistence loaded(encrypted.Path(), settings);
    REQUIRE(loaded.Load() == SaveLoadResult::Loaded);
    CHECK(loaded.GetInt("minimum") == std::numeric_limits<std::int64_t>::min());
    CHECK(loaded.GetInt("maximum") == std::numeric_limits<std::int64_t>::max());
    CHECK(loaded.GetInt("progress.eggs") == 123);
    Persistence wrongMode(encrypted.Path());
    ExpectError([&] { (void)wrongMode.Load(); }, SaveErrorCode::InvalidData);
}

TEST_CASE("Persistence.Encryption.Keys.SharedPersistenceMissingKeyAndExplicitReset", "[persistence][encryption]") {
    TempDirectory temp;
    const auto keyFile = temp.Path() / "keys/save.key";
    const auto settings = Protected(keyFile);
    Persistence first(temp.Path() / "first.json", settings);
    Persistence second(temp.Path() / "second.json", settings);
    first.Set("value", "first");
    second.Set("value", "second");
    first.Save();
    second.Save();
    const auto oldRecord = ReadBinaryFile(first.Path());
    Persistence reopened(first.Path(), settings);
    REQUIRE(reopened.Load() == SaveLoadResult::Loaded);
    REQUIRE(reopened.Get("value"));
    CHECK(reopened.Get("value")->AsString() == "first");
    Persistence other(temp.Path() / "other.json", Protected(temp.Path() / "other/save.key"));
    other.Save();
    Persistence wrong(first.Path(), Protected(temp.Path() / "other/save.key"));
    ExpectError([&] { (void)wrong.Load(); }, SaveErrorCode::AuthenticationFailed);
    std::filesystem::remove(keyFile);
    ExpectError([&] { (void)first.Load(); }, SaveErrorCode::KeyUnavailable);
    CHECK_FALSE(std::filesystem::exists(keyFile));
    WriteBinaryFileAtomic(keyFile, "invalid key");
    ExpectError([&] { first.Save(); }, SaveErrorCode::KeyUnavailable);
    CHECK(ReadBinaryFile(keyFile) == "invalid key");
    first.ResetEncryptionKey();
    CHECK_FALSE(std::filesystem::exists(keyFile));
    first.Save();
    CHECK(first.Load() == SaveLoadResult::Loaded);
    ExpectError([&] { (void)second.Load(); }, SaveErrorCode::AuthenticationFailed);
    WriteBinaryFileAtomic(first.Path(), oldRecord);
    ExpectError([&] { (void)first.Load(); }, SaveErrorCode::AuthenticationFailed);
#ifndef _WIN32
    struct stat info{};
    REQUIRE(stat(keyFile.c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
    REQUIRE(stat(keyFile.parent_path().c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0700);
#endif
}

TEST_CASE("Persistence.Encryption.PlaintextIsRejectedWithoutConversionOrKeyCreation", "[persistence][encryption]") {
    TempDirectory temp;
    const auto file = temp.Path() / "progress.json";
    const auto settings = Protected(temp.Path() / "keys/save.key");
    Persistence plain(file);
    plain.SetInt("progress.eggs", 55);
    plain.Save();
    const auto original = ReadBinaryFile(file);
    Persistence rejected(file, settings);
    rejected.SetInt("existing", 73);
    ExpectError([&] { (void)rejected.Load(); }, SaveErrorCode::EncryptionRequired);
    CHECK(rejected.GetInt("existing") == 73);
    CHECK(ReadBinaryFile(file) == original);
    CHECK_FALSE(std::filesystem::exists(settings.keys.file));
}

TEST_CASE("Persistence.Encryption.NewFieldsUseFallbackAndPreserveExistingProgress", "[persistence][encryption]") {
    TempDirectory temp;
    const auto settings = Protected(temp.Path() / "keys/save.key");
    Persistence previous(temp.Path() / "progress.json", settings);
    previous.SetInt("progress.score", 73);
    previous.Save();

    Persistence updated(previous.Path(), settings);
    REQUIRE(updated.Load() == SaveLoadResult::Loaded);
    CHECK(updated.GetInt("progress.score") == 73);
    CHECK(updated.GetInt("progress.new_stat", 5) == 5);
    CHECK_FALSE(updated.Contains("progress.new_stat"));
    updated.SetInt("progress.new_stat", 8);
    updated.Save();

    REQUIRE(previous.Load() == SaveLoadResult::Loaded);
    CHECK(previous.GetInt("progress.score") == 73);
    CHECK(previous.GetInt("progress.new_stat") == 8);
}

TEST_CASE("Persistence.Encryption.SizeLimitsAndArchivesPreserveValues", "[persistence][encryption]") {
    TempDirectory temp;
    auto settings = Protected(temp.Path() / "keys/save.key");
    settings.maxFileBytes = 256;
    Persistence save(temp.Path() / "progress.json", settings);
    save.SetInt("existing", 73);
    WriteBinaryFileAtomic(save.Path(), std::string(257, 'x'));
    ExpectError([&] { (void)save.Load(); }, SaveErrorCode::InvalidData);
    CHECK(save.GetInt("existing") == 73);
    save.ArchiveFiles();
    CHECK_FALSE(std::filesystem::exists(save.Path()));
    save.Save();
    CHECK(save.Load() == SaveLoadResult::Loaded);
    save.ArchiveFiles();
    std::size_t archives = 0;
    for (const auto& entry : std::filesystem::directory_iterator(temp.Path()))
        if (entry.path().filename().string().find(".invalid.") != std::string::npos) ++archives;
    CHECK(archives == 2);
    save.Set("large", std::string(256, 'x'));
    ExpectError([&] { save.Save(); }, SaveErrorCode::InvalidData);
    CHECK_FALSE(std::filesystem::exists(save.Path()));
}
