#include "Editor/Host/AssetCommandRunner.h"
#include <algorithm>
#include <exception>
#include "Editor/Internals/Session/AssetNames.h"

namespace {
constexpr std::size_t KeptResults = 16;
} // namespace

void AssetCommandRunner::Configure(const std::filesystem::path& python, const std::filesystem::path& engineDirectory,
                                   const std::filesystem::path& projectDirectory) {
    _configured = !python.empty() && !engineDirectory.empty() && !projectDirectory.empty();
    _base.executable = python;
    _base.directory = projectDirectory;
    _base.arguments = {engineDirectory.string(), "--project", projectDirectory.string(), "asset"};
}

std::uint64_t AssetCommandRunner::Run(const std::vector<std::string>& arguments) {
    const auto id = _nextId++;
    _queue.push_back({id, arguments});
    return id;
}

AssetCommandResult AssetCommandRunner::Result(std::uint64_t id) const {
    const auto found = std::ranges::find(_results, id, &std::pair<std::uint64_t, AssetCommandResult>::first);
    return found == _results.end() ? AssetCommandResult{} : found->second;
}

void AssetCommandRunner::Finish(std::uint64_t id, AssetCommandResult result) {
    _results.emplace_back(id, std::move(result));
    if (_results.size() > KeptResults) _results.pop_front();
}

void AssetCommandRunner::Tick(const std::function<void(const std::string&, bool)>& report) {
    if (_running) {
        if (_process.Running()) return;
        const auto code = _process.ExitCode();
        std::string output;
        for (const auto& line : _process.DrainOutput())
            output += (output.empty() ? "" : "\n") + line;
        const bool failed = !code || *code != 0;
        report(failed ? "Asset action failed:\n" + output
                      : "Asset action done; Build & Refresh compiles it.\n" + output,
               failed);
        Finish(_runningId, failed ? AssetCommandResult{AssetCommandState::Failed, AssetCommandError(output)}
                                  : AssetCommandResult{AssetCommandState::Done, {}});
        _process.Clear();
        _running = false;
    }
    if (_queue.empty()) return;
    auto request = std::move(_queue.front());
    _queue.pop_front();
    if (!_configured) {
        const std::string problem =
            "Asset actions need Python; reconfigure the editor build with python Engine editor.";
        report(problem, true);
        Finish(request.id, {AssetCommandState::Failed, problem});
        return;
    }
    auto command = _base;
    command.arguments.insert(command.arguments.end(), request.arguments.begin(), request.arguments.end());
    try {
        _process.Start(command);
        _running = true;
        _runningId = request.id;
    } catch (const std::exception& error) {
        const std::string problem = std::string("Could not start the asset action: ") + error.what();
        report(problem, true);
        Finish(request.id, {AssetCommandState::Failed, problem});
    }
}
