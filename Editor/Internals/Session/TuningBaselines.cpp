#include "Editor/Internals/Session/TuningBaselines.h"
#include <algorithm>
#include <chrono>
#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Platform/Windows/CommandLocator.h"

namespace {
constexpr auto HeadInterval = std::chrono::seconds(2); // How often HEAD is checked
constexpr auto GitTimeout = std::chrono::seconds(10);

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}
} // namespace

TuningBaselines::TuningBaselines(std::filesystem::path projectDirectory)
    : _project(std::move(projectDirectory))
    , _thread([this](const std::stop_token& stop) { Run(stop); }) {}

TuningBaselines::~TuningBaselines() {
    _thread.request_stop();
    _wake.notify_all();
}

void TuningBaselines::Watch(const std::vector<std::filesystem::path>& files) {
    const std::scoped_lock lock(_mutex);
    if (files == _watched) return;
    _watched = files;
    _changed = true;
    _wake.notify_all();
}

std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> TuningBaselines::Take() {
    const std::scoped_lock lock(_mutex);
    return std::exchange(_read, {});
}

void TuningBaselines::Run(const std::stop_token& stop) {
    const auto executable = FindGitExecutable();
    const GitClient git(executable, _project);
    std::string head;
    std::vector<std::filesystem::path> known; // Read at `head`
    while (!stop.stop_requested()) {
        std::vector<std::filesystem::path> watched;
        {
            std::unique_lock lock(_mutex);
            _wake.wait_for(lock, stop, HeadInterval, [this] { return _changed; });
            if (stop.stop_requested()) return;
            _changed = false;
            watched = _watched;
        }
        std::string now;
        if (!executable.empty()) {
            const auto result =
                git.Run({"rev-parse", "--verify", "--quiet", "HEAD"}, GitAccess::Background, nullptr, GitTimeout);
            if (result.ok) now = result.output;
        }
        if (now != head) known.clear();
        head = now;
        std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> read;
        for (const auto& file : watched) {
            if (std::ranges::find(known, file) != known.end()) continue;
            std::optional<std::string> text;
            if (!head.empty()) {
                // "./" makes the path relative to the project folder, wherever the repository starts.
                const auto relative = file.lexically_relative(_project);
                const auto result =
                    git.Run({"show", "HEAD:./" + Utf8(relative)}, GitAccess::Background, nullptr, GitTimeout);
                if (result.ok) text = result.output;
            }
            read.emplace_back(file, std::move(text));
            known.push_back(file);
        }
        std::erase_if(known, [&](const auto& file) { return std::ranges::find(watched, file) == watched.end(); });
        if (read.empty()) continue;
        const std::scoped_lock lock(_mutex);
        for (auto& entry : read)
            _read.push_back(std::move(entry));
    }
}
