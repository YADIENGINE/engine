#include <catch2/catch_test_macros.hpp>
#include <functional>
#include "Tests/Support/TempDirectory.h"
#include "Platform/Internals/FileStorage.h"
#include "Modules/Persistence/Persistence.h"

namespace {
SaveSettings ProtectedBackup(const std::filesystem::path& keyFile) {
    return {.encryption = SaveEncryptionMode::Encrypted, .keepBackup = true, .keys = {.file = keyFile}};
}
const SaveSettings PlainBackup{.keepBackup = true};

void ExpectError(const std::function<void()>& action, SaveErrorCode code) {
    try {
        action();
        FAIL("Expected SaveError");
    } catch (const SaveError& error) {
        CHECK(error.Code() == code);
    }
}
std::size_t ArchiveCount(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& item : std::filesystem::directory_iterator(directory))
        if (item.path().filename().string().find(".invalid.") != std::string::npos) ++count;
    return count;
}
} // namespace

TEST_CASE("Persistence.Recovery.RestoresLatestAuthenticatedBackupAndArchivesDamage", "[persistence][recovery]") {
    TempDirectory temp;
    const auto protectedSettings = ProtectedBackup(temp.Path() / "keys/save.key");
    Persistence save(temp.Path() / "progress.json", protectedSettings);
    save.SetInt("progress.eggs", 12);
    save.Save();
    save.SetInt("progress.eggs", 27);
    save.Save();
    const auto trusted = ReadBinaryFile(save.Path());
    CHECK(ReadBinaryFile(save.BackupPath()).starts_with("ENGSAVE!"));
    auto damaged = trusted;
    damaged.back() ^= 1;
    WriteBinaryFileAtomic(save.Path(), damaged);
    save.SetInt("progress.eggs", 99);
    CHECK(save.Load() == SaveLoadResult::Recovered);
    CHECK(save.GetInt("progress.eggs") == 27);
    CHECK(ArchiveCount(temp.Path()) == 1);
    for (const auto& item : std::filesystem::directory_iterator(temp.Path()))
        if (item.path().filename().string().find(".invalid.") != std::string::npos)
            CHECK(ReadBinaryFile(item.path()) == damaged);
    CHECK(save.Load() == SaveLoadResult::Loaded);
    std::filesystem::remove(save.Path());
    CHECK(save.Load() == SaveLoadResult::Recovered);
    CHECK(ArchiveCount(temp.Path()) == 1);
}

TEST_CASE("Persistence.Recovery.RejectsUntrustedBackupsWithoutChangingMemoryOrFiles", "[persistence][recovery]") {
    TempDirectory temp;
    const auto protectedSettings = ProtectedBackup(temp.Path() / "keys/save.key");
    Persistence save(temp.Path() / "progress.json", protectedSettings);
    save.SetInt("progress.eggs", 27);
    save.Save();
    const auto trusted = ReadBinaryFile(save.BackupPath());
    SECTION("Tampered tag") {
        auto damaged = trusted;
        damaged[36] ^= 1;
        WriteBinaryFileAtomic(save.BackupPath(), damaged);
    }
    SECTION("Truncated envelope") { WriteBinaryFileAtomic(save.BackupPath(), trusted.substr(0, 40)); }
    SECTION("Plaintext downgrade") {
        WriteBinaryFileAtomic(save.BackupPath(), "{\"version\":2,\"values\":{\"progress.eggs\":999}}");
    }
    SECTION("Unsupported encrypted header") {
        auto unsupported = trusted;
        unsupported[8] = 2;
        WriteBinaryFileAtomic(save.BackupPath(), unsupported);
    }
    SECTION("Oversized backup") { WriteBinaryFileAtomic(save.BackupPath(), std::string(1024 * 1024 + 1, 'x')); }
    WriteBinaryFileAtomic(save.Path(), "damaged");
    save.SetInt("existing", 73);
    CHECK_THROWS_AS(save.Load(), SaveError);
    CHECK(save.GetInt("existing") == 73);
    CHECK(save.GetInt("progress.eggs") == 27);
    CHECK(ReadBinaryFile(save.Path()) == "damaged");
    CHECK(ArchiveCount(temp.Path()) == 0);
    std::filesystem::remove(save.Path());
    CHECK_THROWS_AS(save.Load(), SaveError);
    CHECK(save.GetInt("existing") == 73);
    CHECK_FALSE(std::filesystem::exists(save.Path()));
}

TEST_CASE("Persistence.Recovery.KeyAndIoFailuresPreserveRecords", "[persistence][recovery]") {
    TempDirectory temp;
    const auto keyFile = temp.Path() / "keys/save.key";
    const auto protectedSettings = ProtectedBackup(keyFile);
    Persistence save(temp.Path() / "progress.json", protectedSettings);
    save.SetInt("progress.eggs", 27);
    save.Save();
    const auto trusted = ReadBinaryFile(save.Path());
    const auto trustedBackup = ReadBinaryFile(save.BackupPath());
    SECTION("Missing key is not regenerated") {
        std::filesystem::remove(keyFile);
        ExpectError([&] { save.Load(); }, SaveErrorCode::KeyUnavailable);
        CHECK_FALSE(std::filesystem::exists(keyFile));
        std::filesystem::remove(save.Path());
        ExpectError([&] { save.Load(); }, SaveErrorCode::KeyUnavailable);
        WriteBinaryFileAtomic(save.Path(), trusted);
    }
    SECTION("Wrong key cannot authenticate backup") {
        const auto otherSettings = ProtectedBackup(temp.Path() / "other/save.key");
        Persistence other(temp.Path() / "other.json", otherSettings);
        other.Save();
        Persistence wrong(save.Path(), otherSettings);
        wrong.SetInt("existing", 73);
        ExpectError([&] { wrong.Load(); }, SaveErrorCode::AuthenticationFailed);
        CHECK(wrong.GetInt("existing") == 73);
    }
    SECTION("Transient key provider error") {
        std::filesystem::remove(keyFile);
        std::filesystem::create_directory(keyFile);
        Persistence unavailable(save.Path(), protectedSettings);
        unavailable.SetInt("existing", 73);
        ExpectError([&] { unavailable.Load(); }, SaveErrorCode::Io);
        CHECK(unavailable.GetInt("existing") == 73);
    }
    CHECK(ReadBinaryFile(save.Path()) == trusted);
    CHECK(ReadBinaryFile(save.BackupPath()) == trustedBackup);
    CHECK(ArchiveCount(temp.Path()) == 0);
}

TEST_CASE("Persistence.Recovery.BackupWriteFailurePreservesCommittedPrimary", "[persistence][recovery]") {
    TempDirectory temp;
    Persistence save(temp.Path() / "settings.json", PlainBackup);
    save.SetInt("settings.language", 1);
    save.Save();
    const auto original = ReadBinaryFile(save.Path());
    std::filesystem::remove(save.BackupPath());
    std::filesystem::create_directory(save.BackupPath());
    save.SetInt("settings.language", 2);
    ExpectError([&] { save.Save(); }, SaveErrorCode::Io);
    CHECK(ReadBinaryFile(save.Path()) == original);
    CHECK(std::filesystem::is_directory(save.BackupPath()));
}

TEST_CASE("Persistence.Recovery.PlainSettingsRecoverIndependentlyWithoutKeys", "[persistence][recovery]") {
    TempDirectory temp;
    Persistence settings(temp.Path() / "settings.json", PlainBackup);
    CHECK(settings.Load() == SaveLoadResult::Missing);
    settings.SetInt("settings.language", 1);
    settings.SetInt("audio.music_percent", 60);
    settings.Save();
    CHECK(ReadBinaryFile(settings.BackupPath()).starts_with("{"));
    const auto protectedSettings = ProtectedBackup(temp.Path() / "keys/save.key");
    Persistence progress(temp.Path() / "progress.json", protectedSettings);
    progress.SetInt("progress.eggs", 27);
    progress.Save();
    const auto originalProgress = ReadBinaryFile(progress.Path());
    std::filesystem::remove(temp.Path() / "keys/save.key");
    WriteBinaryFileAtomic(settings.Path(), "{unfinished");
    CHECK(settings.Load() == SaveLoadResult::Recovered);
    CHECK(settings.GetInt("audio.music_percent") == 60);
    CHECK(settings.GetInt("settings.language") == 1);
    CHECK(ReadBinaryFile(progress.Path()) == originalProgress);
    CHECK_FALSE(std::filesystem::exists(temp.Path() / "keys/save.key"));
    CHECK(settings.Load() == SaveLoadResult::Loaded);
}

TEST_CASE("Persistence.Recovery.PlaintextBackupIsRejectedInEncryptedMode", "[persistence][recovery]") {
    TempDirectory temp;
    const auto protectedSettings = ProtectedBackup(temp.Path() / "keys/save.key");
    const auto file = temp.Path() / "progress.json";
    Persistence plaintextBackup(file.string() + ".bak");
    plaintextBackup.SetInt("progress.eggs", 27);
    plaintextBackup.Save();
    const auto originalBackup = ReadBinaryFile(plaintextBackup.Path());
    WriteBinaryFileAtomic(file, "damaged");
    Persistence protectedSave(file, protectedSettings);
    protectedSave.SetInt("existing", 73);
    CHECK_THROWS_AS(protectedSave.Load(), SaveError);
    CHECK(protectedSave.GetInt("existing") == 73);
    CHECK(ReadBinaryFile(file) == "damaged");
    CHECK(ReadBinaryFile(protectedSave.BackupPath()) == originalBackup);
    CHECK(ArchiveCount(temp.Path()) == 0);
    CHECK_FALSE(std::filesystem::exists(protectedSettings.keys.file));
}
