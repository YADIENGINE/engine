#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Platform/Windows/ProcessLaunch.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

ChildProcess::ChildProcess() = default;
ChildProcess::~ChildProcess() { Clear(); }
bool ChildProcess::Running() const {
    // A wait failure never counts as proof that the executable is safe to rebuild.
    return _process && WaitForSingleObject(_process.Get(), 0) != WAIT_OBJECT_0;
}
std::optional<unsigned long> ChildProcess::ExitCode() const {
    if (!_process || Running()) return std::nullopt;
    DWORD code = 0;
    if (!GetExitCodeProcess(_process.Get(), &code)) ThrowWorkerWindowsError("Read child exit code");
    return code;
}
void ChildProcess::Start(const ProcessCommand& command) {
    if (Running()) throw std::logic_error("Child process is already running");
    Clear();
    _job.Reset(CreateJobObjectW(nullptr, nullptr));
    if (!_job) ThrowWorkerWindowsError("Create process job");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(_job.Get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        ThrowWorkerWindowsError("Configure process job");

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readHandle = nullptr;
    HANDLE writeHandle = nullptr;
    if (!CreatePipe(&readHandle, &writeHandle, &security, 0)) ThrowWorkerWindowsError("Create output pipe");
    _output.Reset(readHandle);
    NativeHandle writer(writeHandle);
    if (!SetHandleInformation(_output.Get(), HANDLE_FLAG_INHERIT, 0)) ThrowWorkerWindowsError("Protect output pipe");
    NativeHandle input(
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr));
    if (!input) ThrowWorkerWindowsError("Open child stdin");
    auto arguments = ProcessCommandLine(command);
    auto environment = ProcessEnvironmentBlock(command.environment);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = input.Get();
    startup.hStdOutput = writer.Get();
    startup.hStdError = writer.Get();
    PROCESS_INFORMATION information{};
    if (!CreateProcessW(command.executable.c_str(), arguments.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment.data(),
                        command.directory.empty() ? nullptr : command.directory.c_str(), &startup, &information))
        ThrowWorkerWindowsError("Start child process");
    _process.Reset(information.hProcess);
    NativeHandle thread(information.hThread);
    if (!AssignProcessToJobObject(_job.Get(), _process.Get())) {
        const DWORD error = GetLastError();
        TerminateProcess(_process.Get(), 1);
        WaitForSingleObject(_process.Get(), INFINITE);
        SetLastError(error);
        ThrowWorkerWindowsError("Own child process job");
    }
    if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1)) ThrowWorkerWindowsError("Resume child process");
}
std::vector<std::string> ChildProcess::DrainOutput() {
    std::vector<std::string> lines;
    if (!_output) return lines;
    constexpr std::size_t MaxBytesPerPoll = 64 * 1024;
    constexpr std::size_t MaxLineBytes = 16 * 1024;
    std::array<char, 4096> buffer{};
    std::size_t consumed = 0;
    // After exit, drain the entire pipe before Clear closes it (including bursts over 64 KB).
    while (consumed < MaxBytesPerPoll || !Running()) {
        DWORD available = 0;
        if (!PeekNamedPipe(_output.Get(), nullptr, 0, nullptr, &available, nullptr) || !available) break;
        DWORD read = 0;
        if (!ReadFile(_output.Get(), buffer.data(), std::min(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr) ||
            !read)
            break;
        consumed += read;
        _pendingOutput.append(buffer.data(), read);
        std::size_t newline = 0;
        while ((newline = _pendingOutput.find('\n')) != std::string::npos) {
            auto line = _pendingOutput.substr(0, newline);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(std::move(line));
            _pendingOutput.erase(0, newline + 1);
        }
        while (_pendingOutput.size() > MaxLineBytes) {
            lines.push_back(_pendingOutput.substr(0, MaxLineBytes));
            _pendingOutput.erase(0, MaxLineBytes);
        }
    }
    if (!Running() && !_pendingOutput.empty()) lines.push_back(std::exchange(_pendingOutput, {}));
    return lines;
}
void ChildProcess::Terminate() {
    if (_job && Running() && !TerminateJobObject(_job.Get(), 1)) ThrowWorkerWindowsError("Terminate child job");
}
void ChildProcess::Clear() {
    if (_process && Running()) {
        if (_job) TerminateJobObject(_job.Get(), 1);
        WaitForSingleObject(_process.Get(), INFINITE);
    }
    _process.Reset();
    _job.Reset();
    _output.Reset();
    _pendingOutput.clear();
}
