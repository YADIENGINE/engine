#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

// Background runs may never ask the developer anything and leave no locks another git (or an agent) could trip on.
// Interactive runs follow a click: the credential manager may open its sign-in window.
enum class GitAccess { Background, Interactive };

struct GitResult {
    bool started = false;
    bool ok = false; // Started and exited with 0
    unsigned long exitCode = 0;
    bool canceled = false;
    bool timedOut = false;
    std::string output;
    std::string errors;
};

[[nodiscard]] std::map<std::string, std::string> GitEnvironment(GitAccess access);

// The system's git.exe in one folder. Using the real command keeps LFS, the credential manager, hooks and submodules
// behaving as they do in the developer's terminal.
class GitClient {
public:
    GitClient(std::filesystem::path executable, std::filesystem::path directory)
        : _executable(std::move(executable))
        , _directory(std::move(directory)) {}
    [[nodiscard]] GitResult Run(const std::vector<std::string>& arguments, GitAccess access = GitAccess::Background,
                                const std::atomic<bool>* cancel = nullptr,
                                std::chrono::milliseconds timeout = std::chrono::minutes(2)) const;
    [[nodiscard]] GitClient At(std::filesystem::path directory) const { return {_executable, std::move(directory)}; }
    [[nodiscard]] const std::filesystem::path& Directory() const { return _directory; }

private:
    std::filesystem::path _executable;
    std::filesystem::path _directory;
};
