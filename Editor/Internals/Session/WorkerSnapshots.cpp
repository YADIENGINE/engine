#include "Editor/Internals/Session/WorkerSnapshots.h"
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <windows.h>

namespace {
constexpr std::wstring_view SnapshotMarker = L".live-";
constexpr int MaxCaptureAttempts = 16; // Another editor of the same project may take a number at the same moment
std::optional<unsigned long> SnapshotNumber(const std::filesystem::path& file, const std::filesystem::path& built) {
    const auto stem = file.filename().wstring();
    const auto prefix = built.stem().wstring() + std::wstring(SnapshotMarker);
    if (!stem.starts_with(prefix) || stem.size() == prefix.size()) return std::nullopt;
    unsigned long number = 0;
    for (const wchar_t digit : std::wstring_view(stem).substr(prefix.size())) {
        if (digit < L'0' || digit > L'9' || number > 99'999'999) return std::nullopt;
        number = number * 10 + static_cast<unsigned long>(digit - L'0');
    }
    return number;
}
std::filesystem::path SnapshotPath(const std::filesystem::path& built, unsigned long number) {
    auto name = built.stem().wstring() + std::wstring(SnapshotMarker) + std::to_wstring(number);
    return built.parent_path() / name;
}
} // namespace

WorkerSnapshots::WorkerSnapshots(ProcessCommand built)
    : _built(std::move(built)) {}
bool WorkerSnapshots::BuildAvailable() const { return std::filesystem::is_regular_file(_built.executable); }
bool WorkerSnapshots::IsSnapshot(const std::filesystem::path& file) const {
    return SnapshotNumber(file, _built.executable).has_value();
}
void WorkerSnapshots::Capture() {
    if (!BuildAvailable())
        throw std::runtime_error("Worker executable missing; use Build & Refresh to build the worker target");
    unsigned long next = 1;
    for (const auto& entry : std::filesystem::directory_iterator(_built.executable.parent_path()))
        if (const auto number = SnapshotNumber(entry.path(), _built.executable)) next = std::max(next, *number + 1);
    for (int attempt = 0; attempt < MaxCaptureAttempts; ++attempt, ++next) {
        const auto snapshot = SnapshotPath(_built.executable, next);
        std::error_code error;
        if (std::filesystem::create_directory(snapshot, error)) {
            try {
                std::filesystem::copy_file(_built.executable, snapshot / _built.executable.filename());
                auto pdb = _built.executable;
                pdb.replace_extension(".pdb");
                std::filesystem::copy_file(pdb, snapshot / pdb.filename());
                std::filesystem::copy_file(_built.executable.parent_path() / "EngineCrashReporter.exe",
                                           snapshot / "EngineCrashReporter.exe");
                HANDLE lock = CreateFileW((snapshot / "snapshot.lock").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (lock == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot reserve snapshot lease");
                CloseHandle(lock);
                _current = _built;
                _current->executable = snapshot / _built.executable.filename();
                return;
            } catch (...) {
                // This exact child was reserved above; never remove a caller-supplied directory.
                if (snapshot.parent_path() == _built.executable.parent_path())
                    std::filesystem::remove_all(snapshot, error);
                throw;
            }
        }
        if (error != std::errc::file_exists)
            throw std::filesystem::filesystem_error("Snapshot the worker executable", _built.executable, snapshot,
                                                    error);
    }
    throw std::runtime_error("Could not reserve a name for the scene snapshot");
}
void WorkerSnapshots::Prune() const {
    std::error_code error;
    std::filesystem::directory_iterator entries(_built.executable.parent_path(), error);
    for (std::filesystem::directory_iterator end; !error && entries != end; entries.increment(error)) {
        const auto& file = entries->path();
        // Before snapshots became folders they were `<name>.live-<n>.exe` files; a running one stays locked.
        if (entries->is_regular_file(error) && file.extension() == _built.executable.extension() &&
            IsSnapshot(file.parent_path() / file.stem())) {
            std::error_code ignored;
            std::filesystem::remove(file, ignored);
            continue;
        }
        if (!entries->is_directory(error) || entries->is_symlink(error) || !IsSnapshot(file) ||
            (_current && file == _current->executable.parent_path()))
            continue;
        // A running worker's owner holds this lease without delete sharing; test it BEFORE deleting any PDB.
        HANDLE lease = CreateFileW((file / "snapshot.lock").c_str(), DELETE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lease == INVALID_HANDLE_VALUE) continue;
        std::error_code ignored;
        if (std::filesystem::absolute(file).lexically_normal().parent_path() ==
            std::filesystem::absolute(_built.executable.parent_path()).lexically_normal())
            std::filesystem::remove_all(file, ignored);
        CloseHandle(lease);
        // Windows can defer deletion of the locked marker until the handle closes.
        if (std::filesystem::absolute(file).lexically_normal().parent_path() ==
            std::filesystem::absolute(_built.executable.parent_path()).lexically_normal())
            std::filesystem::remove(file, ignored);
    }
}
