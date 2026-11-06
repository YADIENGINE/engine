#include "Diagnostics/Internals/CrashSession.h"
#include <cpptrace/from_current.hpp>
#include <algorithm>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#include "Diagnostics/Internals/CrashPacket.h"

namespace {
// Deliberately process-global: Windows/CRT fatal handlers have no user-data argument. Owns only crash resources.
struct ProcessCrashState {
    HANDLE mapping = nullptr;
    HANDLE request = nullptr;
    HANDLE done = nullptr;
    HANDLE helper = nullptr;
    HANDLE emergency = INVALID_HANDLE_VALUE;
    HANDLE reportReader = INVALID_HANDLE_VALUE;
    CrashPacket* packet = nullptr;
    LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
    std::terminate_handler previousTerminate = nullptr;
    void (*previousAbort)(int) = SIG_DFL;
    unsigned int previousAbortBehavior = 0;
    LONG reported = 0;
    volatile DWORD reportingThread = 0;
    unsigned owners = 0;
    bool installed = false;
    bool exitCleanupRegistered = false;
    std::filesystem::path directory;
};
ProcessCrashState crash;

std::wstring Environment(const wchar_t* name) {
    const DWORD count = GetEnvironmentVariableW(name, nullptr, 0);
    if (!count) return {};
    std::wstring value(count, L'\0');
    value.resize(GetEnvironmentVariableW(name, value.data(), count));
    return value;
}
std::wstring MappingName(std::string_view session) {
    // Session ids come from the engine's own worker launcher, not user paths.
    return L"Local\\EngineCrash-" + std::wstring(session.begin(), session.end());
}
template <class T, std::size_t N> void Copy(std::array<T, N>& out, std::basic_string_view<T> value) {
    const auto count = std::min(value.size(), N - 1);
    std::copy_n(value.data(), count, out.data());
    out[count] = 0;
}
void RawWrite(HANDLE file, const char* data, DWORD size) noexcept {
    if (!file || file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    (void)WriteFile(file, data, size, &written, nullptr);
}
void EmitReport() noexcept {
    auto& packet = *crash.packet;
    if (packet.directory[0] == 0) return;
    if (packet.state != 3 && (!crash.emergency || crash.emergency == INVALID_HANDLE_VALUE)) return;
    // The output is bounded so reporting cannot fill an unattended pipe. Full details remain in the file.
    std::array<char, 8192> output{};
    if (crash.reportReader != INVALID_HANDLE_VALUE) {
        DWORD read = 0;
        if (ReadFile(crash.reportReader, output.data(), static_cast<DWORD>(output.size()), &read, nullptr))
            RawWrite(GetStdHandle(STD_ERROR_HANDLE), output.data(), read);
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, packet.directory.data(), -1, output.data(),
                                           static_cast<int>(output.size()), nullptr, nullptr);
    if (length > 0) {
        constexpr std::string_view prefix = "\nCrash report: ";
        RawWrite(GetStdHandle(STD_ERROR_HANDLE), prefix.data(), static_cast<DWORD>(prefix.size()));
        RawWrite(GetStdHandle(STD_ERROR_HANDLE), output.data(), static_cast<DWORD>(length - 1));
        const std::string_view suffix = packet.state == 3 ? "/report.txt\n" : "/emergency.txt\n";
        RawWrite(GetStdHandle(STD_ERROR_HANDLE), suffix.data(), static_cast<DWORD>(suffix.size()));
    }
}
void Submit(DWORD code, EXCEPTION_POINTERS* exception, const char* message) noexcept {
    if (!crash.packet) return;
    if (InterlockedCompareExchange(&crash.reported, 1, 0) != 0) {
        // Another thread owns the report. Returning would let this fault end the process mid-capture, so park here;
        // the reporting thread terminates or exits the process. A fault inside Submit itself returns at once.
        if (crash.reportingThread != GetCurrentThreadId()) Sleep(INFINITE);
        return;
    }
    crash.reportingThread = GetCurrentThreadId();
    auto& packet = *crash.packet;
    packet.thread = GetCurrentThreadId();
    packet.code = code;
    packet.native = exception != nullptr;
    if (message) {
        // Avoid strlen on unbounded input in a fatal path.
        std::size_t i = 0;
        for (; i + 1 < packet.message.size() && message[i]; ++i)
            packet.message[i] = message[i];
        packet.message[i] = 0;
    }
    if (exception) {
        packet.exception = *exception->ExceptionRecord;
        packet.exception.ExceptionRecord = nullptr;
        packet.context = *exception->ContextRecord;
    } else {
        RtlCaptureContext(&packet.context);
        packet.exception.ExceptionCode = code;
        packet.exception.ExceptionAddress = reinterpret_cast<void*>(packet.context.Rip);
    }
    std::array<char, 160> minimal{};
    const int size = sprintf_s(minimal.data(), minimal.size(), "Crash: code=0x%08lx thread=%lu address=0x%llx\n", code,
                               packet.thread, static_cast<unsigned long long>(packet.context.Rip));
    if (size > 0) {
        RawWrite(crash.emergency, minimal.data(), static_cast<DWORD>(size));
        FlushFileBuffers(crash.emergency);
    }
    if (crash.helper && WaitForSingleObject(crash.helper, 0) == WAIT_TIMEOUT) {
        InterlockedExchange(&packet.state, 2);
        SetEvent(crash.request);
        std::array<HANDLE, 2> waits{crash.done, crash.helper};
        (void)WaitForMultipleObjects(static_cast<DWORD>(waits.size()), waits.data(), FALSE, 10000);
    }
    if (packet.state != 3) {
        if (crash.helper) TerminateProcess(crash.helper, 1);
        InterlockedExchange(&packet.state, 4);
        if (size > 0) RawWrite(GetStdHandle(STD_ERROR_HANDLE), minimal.data(), static_cast<DWORD>(size));
    }
    EmitReport();
}
LONG WINAPI FatalFilter(EXCEPTION_POINTERS* exception) noexcept {
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP || IsDebuggerPresent())
        return EXCEPTION_CONTINUE_SEARCH;
    Submit(code, exception, "Unhandled native exception");
    return EXCEPTION_EXECUTE_HANDLER;
}
void Terminated() noexcept {
    bool submitted = false;
    const char* message = "std::terminate";
    try {
        if (auto exception = std::current_exception()) std::rethrow_exception(exception);
    } catch (const std::exception& error) {
        Submit(0xE0000001, nullptr, error.what()); // what() lives only inside this handler
        submitted = true;
    } catch (...) {
        message = "std::terminate: unknown exception";
    }
    if (!submitted) Submit(0xE0000001, nullptr, message);
    TerminateProcess(GetCurrentProcess(), 1);
}
void Aborted(int) noexcept {
    Submit(0xE0000002, nullptr, "abort");
    TerminateProcess(GetCurrentProcess(), 1);
}
void Close(HANDLE& handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    handle = nullptr;
}
void Release() noexcept {
    if (crash.packet) {
        if (crash.installed) {
            std::set_terminate(crash.previousTerminate);
            SetUnhandledExceptionFilter(crash.previousFilter);
            std::signal(SIGABRT, crash.previousAbort);
            _set_abort_behavior(crash.previousAbortBehavior, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            crash.installed = false;
        }
        UnmapViewOfFile(crash.packet);
        crash.packet = nullptr;
    }
    Close(crash.request);
    Close(crash.done);
    // The helper also waits on the target process, but normal return should not leave it alive until process exit.
    if (crash.helper && !crash.reported) {
        TerminateProcess(crash.helper, 0);
        WaitForSingleObject(crash.helper, 1000);
    }
    Close(crash.helper);
    Close(crash.mapping);
    Close(crash.emergency);
    Close(crash.reportReader);
    if (!crash.reported && !crash.directory.empty()) {
        std::error_code ignored;
        std::filesystem::remove(crash.directory / "emergency.txt", ignored);
        std::filesystem::remove(crash.directory / "report.txt", ignored);
        std::filesystem::remove(crash.directory, ignored);
    }
}
constexpr std::size_t PlayerReportsKept = 10;
void Install(std::string_view kind, bool playerDialog) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    auto session = Environment(L"ENGINE_CRASH_SESSION");
    if (session.empty()) session = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    crash.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE, 0, sizeof(CrashPacket),
                                       (L"Local\\EngineCrash-" + session).c_str());
    if (!crash.mapping || GetLastError() == ERROR_ALREADY_EXISTS) throw std::runtime_error("Crash mapping unavailable");
    crash.packet =
        static_cast<CrashPacket*>(MapViewOfFile(crash.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashPacket)));
    if (!crash.packet) throw std::runtime_error("Crash mapping failed");
    std::construct_at(crash.packet);
    auto& packet = *crash.packet;
    packet.process = GetCurrentProcessId();
    crash.previousFilter = SetUnhandledExceptionFilter(FatalFilter);
    crash.previousTerminate = std::set_terminate(Terminated);
    crash.previousAbort = std::signal(SIGABRT, Aborted);
    crash.previousAbortBehavior = _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    crash.installed = true;
    ULONG guarantee = 64 * 1024;
    SetThreadStackGuarantee(&guarantee);
    Copy(packet.kind, kind);
    Copy(packet.engine, std::string_view(ENGINE_NAME " " ENGINE_VERSION));
    GetModuleFileNameW(nullptr, packet.executable.data(), static_cast<DWORD>(packet.executable.size()));
    const std::filesystem::path executable(packet.executable.data());
    auto root = Environment(L"ENGINE_CRASH_DIRECTORY");
    const bool player = playerDialog && root.empty() && Environment(L"ENGINE_ASSET_REPORT").empty();
    if (root.empty()) {
        if (player) {
            const auto data = Environment(L"LOCALAPPDATA");
            if (data.empty()) throw std::runtime_error("LOCALAPPDATA unavailable");
            root = (std::filesystem::path(data) / ENGINE_NAME / "Crashes" / executable.stem()).wstring();
        } else {
            root = (executable.parent_path() / "crashes").wstring();
        }
    }
    std::filesystem::create_directories(root);
    PruneCrashReports(root, player ? PlayerReportsKept : 0);
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::array<wchar_t, 128> id{};
    swprintf_s(id.data(), id.size(), L"%04u%02u%02uT%02u%02u%02u-%03u-%lu-%llu", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, packet.process, GetTickCount64());
    crash.directory = std::filesystem::path(root) / id.data();
    std::filesystem::create_directory(crash.directory);
    Copy(packet.directory, std::wstring_view(crash.directory.native()));
    packet.dialog = player;
    crash.emergency = CreateFileW((crash.directory / "emergency.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    crash.reportReader = CreateFileW((crash.directory / "report.txt").c_str(), GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    crash.request = CreateEventW(&security, FALSE, FALSE, nullptr);
    crash.done = CreateEventW(&security, FALSE, FALSE, nullptr);
    HANDLE target = nullptr;
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &target,
                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE | SYNCHRONIZE, TRUE, 0);
    if (!crash.request || !crash.done || !target) throw std::runtime_error("Crash events unavailable");
    auto helper = executable.parent_path() / "EngineCrashReporter.exe";
    if (const auto override = Environment(L"ENGINE_CRASH_REPORTER"); !override.empty()) helper = override;
    auto command = L"\"" + helper.wstring() + L"\" --monitor " +
                   std::to_wstring(reinterpret_cast<uintptr_t>(crash.mapping)) + L" " +
                   std::to_wstring(reinterpret_cast<uintptr_t>(crash.request)) + L" " +
                   std::to_wstring(reinterpret_cast<uintptr_t>(crash.done)) + L" " +
                   std::to_wstring(reinterpret_cast<uintptr_t>(target));
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> attributes(size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) {
        CloseHandle(target);
        throw std::runtime_error("Crash handle list failed");
    }
    std::array<HANDLE, 4> handles{crash.mapping, crash.request, crash.done, target};
    const BOOL updated = UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(),
                                                   sizeof(handles), nullptr, nullptr);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = list;
    PROCESS_INFORMATION process{};
    const BOOL started = updated && CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, TRUE,
                                                   CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                                   &startup.StartupInfo, &process);
    DeleteProcThreadAttributeList(list);
    CloseHandle(target);
    if (!started) throw std::runtime_error("Crash reporter could not start");
    CloseHandle(process.hThread);
    crash.helper = process.hProcess;
    if (WaitForSingleObject(crash.done, 2000) != WAIT_OBJECT_0 || packet.state != 1)
        throw std::runtime_error("Crash reporter did not become ready");
}
} // namespace

CrashSession::CrashSession(std::string_view kind, bool playerDialog) noexcept {
    if (crash.owners++ != 0) return;
    crash.reported = 0;
    crash.directory.clear();
    if (!crash.exitCleanupRegistered) {
        // Asset-list queries intentionally use std::exit before opening a window; automatic locals do not unwind.
        crash.exitCleanupRegistered = std::atexit([] {
                                          if (crash.owners) {
                                              crash.owners = 0;
                                              Release();
                                          }
                                      }) == 0;
    }
    try {
        Install(kind, playerDialog);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Crash reporting partial: %s\n", error.what());
    }
}
CrashSession::~CrashSession() {
    if (--crash.owners == 0) Release();
}
void ReportCrashException(const char* message) noexcept {
    if (!crash.packet) {
        std::fprintf(stderr, "Crash: %s (reporting unavailable)\n", message);
        return;
    }
    try {
        const auto& frames = cpptrace::raw_trace_from_current_exception().frames;
        crash.packet->frameCount = static_cast<DWORD>(std::min(frames.size(), crash.packet->frames.size()));
        std::copy_n(frames.begin(), crash.packet->frameCount, crash.packet->frames.begin());
    } catch (...) {
    }
    Submit(0xE06D7363, nullptr, message);
}
int RunCrashBoundary(const std::function<int()>& action) noexcept {
    int result = 1;
    cpptrace::try_catch([&] { result = action(); },
                        [](const std::exception& error) { ReportCrashException(error.what()); },
                        [] { ReportCrashException("Unknown C++ exception"); });
    return result;
}
void RunWithCrashCleanup(const std::function<void()>& action, const std::function<void()>& cleanup) {
    cpptrace::try_catch([&] { action(); },
                        [&] {
                            cleanup();
                            cpptrace::rethrow();
                        });
}
void SetCrashContext(std::string_view context) noexcept {
    if (crash.packet) Copy(crash.packet->label, context);
}
bool WorkerCrashPending(std::string_view session) noexcept {
    try {
        HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, MappingName(session).c_str());
        if (!mapping) return false;
        auto* packet =
            static_cast<const CrashPacket*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(CrashPacket)));
        const bool pending = packet && packet->state >= 2;
        if (packet) UnmapViewOfFile(packet);
        CloseHandle(mapping);
        return pending;
    } catch (...) {
        return false;
    }
}
void PruneCrashReports(const std::filesystem::path& root, std::size_t keep) noexcept {
    try {
        std::error_code error;
        const auto parent = std::filesystem::absolute(root, error).lexically_normal();
        if (error) return;
        std::vector<std::filesystem::path> records;
        std::filesystem::directory_iterator entries(root, error);
        for (std::filesystem::directory_iterator end; !error && entries != end; entries.increment(error)) {
            std::error_code ignored;
            if (!entries->is_directory(ignored) || entries->is_symlink(ignored)) continue;
            const auto incident = entries->path();
            if (std::filesystem::absolute(incident, ignored).lexically_normal().parent_path() != parent) continue;
            const auto emergency = incident / "emergency.txt";
            std::error_code missing;
            const auto size = std::filesystem::file_size(emergency, missing);
            if (std::filesystem::is_regular_file(incident / "report.json", ignored) || (!missing && size > 0)) {
                records.push_back(incident);
                continue;
            }
            // An empty emergency file records no crash. Its live owner holds it open without delete sharing, so the
            // delete succeeds only once that process has ended: a killed process, not one still starting or running.
            if (!missing && DeleteFileW(emergency.c_str())) std::filesystem::remove_all(incident, ignored);
        }
        if (keep == 0 || records.size() <= keep) return;
        std::ranges::sort(records); // Incident names start with their UTC time
        for (std::size_t i = 0; i + keep < records.size(); ++i) {
            std::error_code ignored;
            std::filesystem::remove_all(records[i], ignored);
        }
    } catch (...) {
        // Housekeeping must never prevent reporting.
    }
}
