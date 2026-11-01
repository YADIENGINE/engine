#include "Editor/Internals/Session/WorkerSnapshots.h"
#include <algorithm>
#include <cwctype>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {
constexpr std::wstring_view SnapshotMarker = L".live-";
constexpr int MaxCaptureAttempts = 16; // Another editor of the same project may take a number at the same moment
std::optional<unsigned long> SnapshotNumber(const std::filesystem::path& file, const std::filesystem::path& built) {
    if (file.extension() != built.extension()) return std::nullopt;
    const auto stem = file.stem().wstring();
    const auto prefix = built.stem().wstring() + std::wstring(SnapshotMarker);
    if (!stem.starts_with(prefix) || stem.size() == prefix.size()) return std::nullopt;
    unsigned long number = 0;
    for (const wchar_t digit : std::wstring_view(stem).substr(prefix.size())) {
        if (!std::iswdigit(digit) || number > 99'999'999) return std::nullopt;
        number = number * 10 + static_cast<unsigned long>(digit - L'0');
    }
    return number;
}
std::filesystem::path SnapshotPath(const std::filesystem::path& built, unsigned long number) {
    auto name = built.stem().wstring() + std::wstring(SnapshotMarker) + std::to_wstring(number);
    return built.parent_path() / (name + built.extension().wstring());
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
        if (std::filesystem::copy_file(_built.executable, snapshot, std::filesystem::copy_options::none, error)) {
            _current = _built;
            _current->executable = snapshot;
            return;
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
        if (!IsSnapshot(file) || (_current && file == _current->executable)) continue;
        std::error_code ignored;
        std::filesystem::remove(file, ignored); // A file a worker still runs from is locked; a later Prune takes it.
    }
}
