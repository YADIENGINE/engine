#pragma once
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// The committed text (git HEAD) of tuning files, read with the system git on its own thread so the editor never waits.
// The Inspector's Modified and Revert compare with it (Docs/Tuning.md → Editor). Without git or a repository every file
// reads as having no commit.
class TuningBaselines {
public:
    explicit TuningBaselines(std::filesystem::path projectDirectory);
    ~TuningBaselines();
    // The files to follow. All are read again when HEAD moves, a new one when it joins.
    void Watch(const std::vector<std::filesystem::path>& files);
    // Texts read since the last call; nullopt: the file has no committed version.
    [[nodiscard]] std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> Take();

private:
    void Run(const std::stop_token& stop);

    std::filesystem::path _project;
    std::mutex _mutex;
    std::condition_variable_any _wake;
    std::vector<std::filesystem::path> _watched;
    bool _changed = false;
    std::vector<std::pair<std::filesystem::path, std::optional<std::string>>> _read;
    std::jthread _thread; // Last: stops and joins before the members above go away

public:
    TuningBaselines(const TuningBaselines&) = delete;
    TuningBaselines& operator=(const TuningBaselines&) = delete;
};
