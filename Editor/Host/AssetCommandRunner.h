#pragma once
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include "Editor/Internals/Process/ProcessCommand.h"

// Runs the Assets panel's file actions through `python Engine asset ...`, one at a time, so the panel and command line
// share one implementation.
class AssetCommandRunner {
public:
    void Configure(const std::filesystem::path& python, const std::filesystem::path& engineDirectory,
                   const std::filesystem::path& projectDirectory);
    void Run(const std::vector<std::string>& arguments) { _queue.push_back(arguments); }
    // Call once per frame. `report(text, failed)` receives each finished command's result.
    void Tick(const std::function<void(const std::string&, bool)>& report);

private:
    ProcessCommand _base;
    bool _configured = false;
    std::deque<std::vector<std::string>> _queue;
    ChildProcess _process;
    bool _running = false;
};
