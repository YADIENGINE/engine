#include "Editor/Host/AssetCommandRunner.h"
#include <exception>

void AssetCommandRunner::Configure(const std::filesystem::path& python, const std::filesystem::path& engineDirectory,
                                   const std::filesystem::path& projectDirectory) {
    _configured = !python.empty() && !engineDirectory.empty() && !projectDirectory.empty();
    _base.executable = python;
    _base.directory = projectDirectory;
    _base.arguments = {engineDirectory.string(), "--project", projectDirectory.string(), "asset"};
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
        _process.Clear();
        _running = false;
    }
    if (_queue.empty()) return;
    auto arguments = std::move(_queue.front());
    _queue.pop_front();
    if (!_configured) {
        report("Asset actions need Python; reconfigure the editor build with python Engine editor.", true);
        return;
    }
    auto command = _base;
    command.arguments.insert(command.arguments.end(), arguments.begin(), arguments.end());
    try {
        _process.Start(command);
        _running = true;
    } catch (const std::exception& error) {
        report(std::string("Could not start the asset action: ") + error.what(), true);
    }
}
