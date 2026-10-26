#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Process/ProcessCommand.h"

class ChildProcess {
public:
    ChildProcess();
    ~ChildProcess();
    void Start(const ProcessCommand& command);
    [[nodiscard]] bool Running() const;
    [[nodiscard]] std::optional<unsigned long> ExitCode() const;
    [[nodiscard]] std::vector<std::string> DrainOutput();
    void Terminate();
    void Clear();

private:
    NativeHandle _job;
    NativeHandle _process;
    NativeHandle _output;
    std::string _pendingOutput;

public:
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
};
