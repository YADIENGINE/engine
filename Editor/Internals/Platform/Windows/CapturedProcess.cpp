#include "Editor/Internals/Platform/Windows/CapturedProcess.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <exception>
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Platform/Windows/ProcessLaunch.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
constexpr DWORD PollMilliseconds = 10;
constexpr DWORD PipeBytes = 1024 * 1024;
void Pipe(NativeHandle& reader, NativeHandle& writer) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readHandle = nullptr;
    HANDLE writeHandle = nullptr;
    if (!CreatePipe(&readHandle, &writeHandle, &security, PipeBytes)) ThrowWorkerWindowsError("Create output pipe");
    reader.Reset(readHandle);
    writer.Reset(writeHandle);
    if (!SetHandleInformation(reader.Get(), HANDLE_FLAG_INHERIT, 0)) ThrowWorkerWindowsError("Protect output pipe");
}
// Reads what is there without blocking: a process left behind by the child can keep the pipe open forever.
void Drain(const NativeHandle& pipe, std::string& text) {
    std::array<char, 64 * 1024> buffer{};
    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe.Get(), nullptr, 0, nullptr, &available, nullptr) || !available) return;
        DWORD read = 0;
        if (!ReadFile(pipe.Get(), buffer.data(), std::min(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr) ||
            !read)
            return;
        text.append(buffer.data(), read);
    }
}
} // namespace

CapturedProcessResult RunCapturedProcess(const ProcessCommand& command, const CapturedProcessOptions& options) {
    CapturedProcessResult result;
    try {
        NativeHandle job(CreateJobObjectW(nullptr, nullptr));
        if (!job) ThrowWorkerWindowsError("Create process job");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
            ThrowWorkerWindowsError("Configure process job");
        NativeHandle outputRead, outputWrite, errorRead, errorWrite;
        Pipe(outputRead, outputWrite);
        Pipe(errorRead, errorWrite);
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        NativeHandle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                       OPEN_EXISTING, 0, nullptr));
        if (!input) ThrowWorkerWindowsError("Open child stdin");
        auto arguments = ProcessCommandLine(command);
        auto environment = ProcessEnvironmentBlock(command.environment);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
        startup.hStdInput = input.Get();
        startup.hStdOutput = outputWrite.Get();
        startup.hStdError = errorWrite.Get();
        PROCESS_INFORMATION information{};
        if (!CreateProcessW(command.executable.c_str(), arguments.data(), nullptr, nullptr, TRUE,
                            CREATE_SUSPENDED | CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment.data(),
                            command.directory.empty() ? nullptr : command.directory.c_str(), &startup, &information))
            ThrowWorkerWindowsError("Start child process");
        NativeHandle process(information.hProcess);
        NativeHandle thread(information.hThread);
        if (!AssignProcessToJobObject(job.Get(), process.Get())) {
            const DWORD error = GetLastError();
            TerminateProcess(process.Get(), 1);
            SetLastError(error);
            ThrowWorkerWindowsError("Own child process job");
        }
        if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1)) ThrowWorkerWindowsError("Resume child process");
        result.started = true;
        outputWrite.Reset();
        errorWrite.Reset();
        const auto start = std::chrono::steady_clock::now();
        bool stopped = false;
        while (WaitForSingleObject(process.Get(), PollMilliseconds) != WAIT_OBJECT_0) {
            Drain(outputRead, result.output);
            Drain(errorRead, result.errors);
            if (stopped) continue;
            if (options.cancel && options.cancel->load()) result.canceled = stopped = true;
            if (options.timeout.count() > 0 && std::chrono::steady_clock::now() - start > options.timeout)
                result.timedOut = stopped = true;
            if (result.output.size() > options.maximumOutput) result.truncated = stopped = true;
            if (stopped) TerminateJobObject(job.Get(), 1);
        }
        Drain(outputRead, result.output);
        Drain(errorRead, result.errors);
        DWORD code = 0;
        if (GetExitCodeProcess(process.Get(), &code)) result.exitCode = code;
    } catch (const std::exception& error) {
        result.startError = error.what();
    }
    return result;
}
