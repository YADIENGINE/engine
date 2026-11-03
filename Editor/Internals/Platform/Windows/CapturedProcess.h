#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include "Editor/Internals/Process/ProcessCommand.h"

struct CapturedProcessResult {
    bool started = false;
    unsigned long exitCode = 0;
    std::string output; // Raw stdout; machine-readable output stays free of warnings
    std::string errors;
    bool canceled = false;
    bool timedOut = false;
    bool truncated = false; // stdout reached the limit and the process was stopped
    std::string startError;
};

struct CapturedProcessOptions {
    const std::atomic<bool>* cancel = nullptr;
    std::chrono::milliseconds timeout{0}; // 0: none
    std::size_t maximumOutput = 256u * 1024u * 1024u;
};

// Runs a command to its end without a window or stdin. Stopping it ends its whole process tree.
[[nodiscard]] CapturedProcessResult RunCapturedProcess(const ProcessCommand& command,
                                                       const CapturedProcessOptions& options = {});
