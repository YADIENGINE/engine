#pragma once
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include "Editor/Internals/Process/ProcessCommand.h"

// Runs the Assets panel's file actions through `python Engine asset ...`, one at a time, so the panel and command line
// share one implementation.
class AssetCommandRunner {
public:
    void Configure(const std::filesystem::path& python, const std::filesystem::path& engineDirectory,
                   const std::filesystem::path& projectDirectory);
    // Queues the action; the id asks Result how it went.
    [[nodiscard]] std::uint64_t Run(const std::vector<std::string>& arguments);
    // Running until the action finished; the last few results are kept.
    [[nodiscard]] AssetCommandResult Result(std::uint64_t id) const;
    // Call once per frame. `report(text, failed)` receives each finished command's result.
    void Tick(const std::function<void(const std::string&, bool)>& report);

private:
    struct Request {
        std::uint64_t id = 0;
        std::vector<std::string> arguments;
    };
    void Finish(std::uint64_t id, AssetCommandResult result);

    ProcessCommand _base;
    bool _configured = false;
    std::deque<Request> _queue;
    std::deque<std::pair<std::uint64_t, AssetCommandResult>> _results;
    std::uint64_t _nextId = 1;
    std::uint64_t _runningId = 0;
    ChildProcess _process;
    bool _running = false;
};
