#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
std::wstring QuoteArgument(const std::wstring& argument) {
    if (argument.find(L'\0') != std::wstring::npos) throw std::runtime_error("NUL in process argument");
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        result.append(character == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result.push_back(character);
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}
std::wstring Upper(std::wstring text) {
    if (!text.empty()) CharUpperBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}
std::vector<wchar_t> Environment(const std::map<std::string, std::string>& overrides) {
    std::map<std::wstring, std::wstring> values;
    const auto release = [](wchar_t* pointer) {
        if (pointer) FreeEnvironmentStringsW(pointer);
    };
    std::unique_ptr<wchar_t, decltype(release)> block(GetEnvironmentStringsW(), release);
    if (!block) ThrowWorkerWindowsError("Read environment");
    for (const wchar_t* cursor = block.get(); *cursor;) {
        const std::wstring row(cursor);
        cursor += row.size() + 1;
        const auto separator = row.find(L'=', row.front() == L'=' ? 1 : 0);
        if (separator != std::wstring::npos) values[Upper(row.substr(0, separator))] = row.substr(separator + 1);
    }
    for (const auto& [name, value] : overrides) {
        if (name.empty() || name.find_first_of("=\0", 0, 2) != std::string::npos ||
            value.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid environment override");
        values[Upper(WorkerWide(name))] = WorkerWide(value);
    }
    std::vector<wchar_t> result;
    for (const auto& [key, value] : values) {
        result.insert(result.end(), key.begin(), key.end());
        result.push_back(L'=');
        result.insert(result.end(), value.begin(), value.end());
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    if (result.size() == 1) result.push_back(L'\0');
    return result;
}
} // namespace

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
    std::wstring arguments = QuoteArgument(command.executable.wstring());
    for (const auto& argument : command.arguments)
        arguments += L" " + QuoteArgument(WorkerWide(argument));
    auto environment = Environment(command.environment);
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
