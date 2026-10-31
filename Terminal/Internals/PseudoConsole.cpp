#include "Terminal/Internals/PseudoConsole.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cwchar>
#include <cwctype>

namespace {
std::string WindowsError(const char* operation) {
    return std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
}
bool SameName(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index)
        if (std::towupper(a[index]) != std::towupper(b[index])) return false;
    return true;
}
// "=C:=C:\..." gibi gizli girdilerin adı '=' ile başlar; ad ilk '='den önceki kısımdır (0. karakter hariç).
std::wstring_view NameOf(std::wstring_view entry) {
    const auto equals = entry.find(L'=', 1);
    return entry.substr(0, equals == std::wstring_view::npos ? entry.size() : equals);
}
std::vector<wchar_t> EnvironmentBlock(const std::vector<std::pair<std::wstring, std::wstring>>& overrides) {
    std::vector<std::wstring> entries;
    if (auto* strings = GetEnvironmentStringsW()) {
        for (const wchar_t* entry = strings; *entry; entry += std::wcslen(entry) + 1)
            entries.emplace_back(entry);
        FreeEnvironmentStringsW(strings);
    }
    for (const auto& [name, value] : overrides) {
        std::erase_if(entries, [&](const std::wstring& entry) { return SameName(NameOf(entry), name); });
        entries.push_back(name + L"=" + value);
    }
    std::vector<wchar_t> block;
    for (const auto& entry : entries) {
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
COORD Size(int rows, int columns) {
    return COORD{static_cast<SHORT>(std::max(columns, 1)), static_cast<SHORT>(std::max(rows, 1))};
}
} // namespace

void PseudoConsole::HandleCloser::operator()(void* handle) const {
    if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
}
void PseudoConsole::ConsoleCloser::operator()(void* console) const {
    if (console) ClosePseudoConsole(static_cast<HPCON>(console));
}

std::unique_ptr<PseudoConsole> PseudoConsole::Start(const PseudoConsoleOptions& options, std::string& error) {
    auto self = std::make_unique<PseudoConsole>(Token{});
    HANDLE inputRead = nullptr, inputWrite = nullptr, outputRead = nullptr, outputWrite = nullptr;
    if (!CreatePipe(&inputRead, &inputWrite, nullptr, 0)) {
        error = WindowsError("CreatePipe");
        return nullptr;
    }
    const Handle consoleInput(inputRead);
    self->_input.reset(inputWrite);
    if (!CreatePipe(&outputRead, &outputWrite, nullptr, 0)) {
        error = WindowsError("CreatePipe");
        return nullptr;
    }
    const Handle consoleOutput(outputWrite);
    self->_output.reset(outputRead);

    HPCON console = nullptr;
    const HRESULT created =
        CreatePseudoConsole(Size(options.rows, options.columns), inputRead, outputWrite, 0, &console);
    if (FAILED(created)) {
        error = "CreatePseudoConsole failed (HRESULT " + std::to_string(created) + ")";
        return nullptr;
    }
    self->_console.reset(console);

    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<std::byte> attributeStorage(attributeSize);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize)) {
        error = WindowsError("InitializeProcThreadAttributeList");
        return nullptr;
    }
    struct AttributeListGuard {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeListGuard() { DeleteProcThreadAttributeList(list); }
    } attributeGuard{attributes};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console, sizeof(console),
                                   nullptr, nullptr)) {
        error = WindowsError("UpdateProcThreadAttribute");
        return nullptr;
    }

    self->_job.reset(CreateJobObjectW(nullptr, nullptr));
    if (!self->_job) {
        error = WindowsError("CreateJobObject");
        return nullptr;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(self->_job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits));

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    // Editörün std handle'ları yönlendirilmişse (ör. araçtan başlatıldıysa) çocuk onları değil ConPTY'yi kullanmalı.
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = attributes;
    auto environment = EnvironmentBlock(options.environment);
    std::wstring commandLine = options.commandLine;
    const std::wstring directory = options.directory.empty() ? std::wstring{} : options.directory.wstring();
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                        environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo,
                        &process)) {
        error = WindowsError("CreateProcess");
        return nullptr;
    }
    self->_process.reset(process.hProcess);
    const Handle thread(process.hThread);
    AssignProcessToJobObject(self->_job.get(), process.hProcess);
    ResumeThread(process.hThread);

    self->_reader = std::thread([raw = self.get()] { raw->ReadLoop(); });
    return self;
}

PseudoConsole::~PseudoConsole() {
    if (_job) TerminateJobObject(_job.get(), 1);
    _input.reset();
    _console.reset();
    if (_reader.joinable()) {
        CancelSynchronousIo(_reader.native_handle());
        _reader.join();
    }
}

void PseudoConsole::ReadLoop() {
    std::array<char, 16 * 1024> buffer{};
    DWORD read = 0;
    while (ReadFile(_output.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0) {
        const std::lock_guard lock(_mutex);
        _pending.append(buffer.data(), read);
    }
}

void PseudoConsole::Write(std::string_view bytes) {
    while (!bytes.empty() && _input) {
        DWORD written = 0;
        if (!WriteFile(_input.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)) return;
        bytes.remove_prefix(written);
    }
}

void PseudoConsole::Resize(int rows, int columns) {
    if (_console) ResizePseudoConsole(static_cast<HPCON>(_console.get()), Size(rows, columns));
}

std::string PseudoConsole::TakeOutput() {
    const std::lock_guard lock(_mutex);
    return std::exchange(_pending, {});
}

std::optional<int> PseudoConsole::ExitCode() const {
    if (!_process || WaitForSingleObject(_process.get(), 0) != WAIT_OBJECT_0) return std::nullopt;
    DWORD code = 0;
    GetExitCodeProcess(_process.get(), &code);
    return static_cast<int>(code);
}
