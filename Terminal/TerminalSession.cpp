#include "Terminal/TerminalSession.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include "Terminal/Internals/PseudoConsole.h"
#include "Terminal/Internals/VtScreen.h"

namespace {
constexpr int DefaultRows = 24;
constexpr int DefaultColumns = 80;
constexpr auto ExitDrainTime = std::chrono::milliseconds(200);

std::wstring Wide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}
std::string Utf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), length, nullptr, nullptr);
    return utf8;
}
std::wstring Quoted(const std::wstring& path) { return L"\"" + path + L"\""; }
} // namespace

std::string DefaultTerminalShell() {
    std::array<wchar_t, MAX_PATH> found{};
    if (SearchPathW(nullptr, L"pwsh.exe", nullptr, static_cast<DWORD>(found.size()), found.data(), nullptr) > 0)
        return Utf8(Quoted(found.data()) + L" -NoLogo");
    std::array<wchar_t, MAX_PATH> system{};
    const UINT length = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    const std::wstring directory = length > 0 ? std::wstring(system.data(), length) : L"C:\\Windows\\System32";
    return Utf8(Quoted(directory + L"\\WindowsPowerShell\\v1.0\\powershell.exe") + L" -NoLogo");
}

TerminalSession::TerminalSession(const TerminalPalette& palette)
    : _screen(std::make_unique<VtScreen>(DefaultRows, DefaultColumns, palette)) {}
TerminalSession::~TerminalSession() = default;

bool TerminalSession::Start(const TerminalStartOptions& options) {
    _options = options;
    _console.reset();
    _exitSeen.reset();
    _exitCode.reset();
    _started = true;
    _screen->Reset();

    PseudoConsoleOptions console;
    console.commandLine = Wide(options.commandLine.empty() ? DefaultTerminalShell() : options.commandLine);
    console.directory = options.directory;
    console.rows = _screen->Rows();
    console.columns = _screen->Columns();
    // Node tabanlı CLI'lar truecolor'u ancak bu değişkenle açar.
    console.environment.emplace_back(L"COLORTERM", L"truecolor");
    for (const auto& [name, value] : options.environment)
        console.environment.emplace_back(Wide(name), Wide(value));
    std::string error;
    _console = PseudoConsole::Start(console, error);
    if (!_console) {
        Notice("Could not start the shell: " + error);
        return false;
    }
    return true;
}

void TerminalSession::Stop() {
    _console.reset();
    _exitSeen.reset();
    _exitCode.reset();
}

void TerminalSession::Pump() {
    if (!_console) return;
    _screen->Feed(_console->TakeOutput());
    Flush();
    const auto code = _console->ExitCode();
    if (!code) return;
    // ConPTY son çıktıyı süreç çıktıktan sonra da gönderebilir: kısa bir süre daha boşaltılır.
    const auto now = std::chrono::steady_clock::now();
    if (!_exitSeen) _exitSeen = now;
    if (now - *_exitSeen < ExitDrainTime) return;
    _screen->Feed(_console->TakeOutput());
    _console.reset();
    _exitSeen.reset();
    _exitCode = code;
    Notice("Process exited with code " + std::to_string(*code) + ". Press Enter to restart.");
}

void TerminalSession::Resize(int rows, int columns) {
    rows = std::max(rows, 1);
    columns = std::max(columns, 1);
    if (rows == _screen->Rows() && columns == _screen->Columns()) return;
    _screen->Resize(rows, columns);
    if (_console) _console->Resize(_screen->Rows(), _screen->Columns());
}

void TerminalSession::SendKey(TerminalKey key, TerminalModifiers modifiers) {
    if (!Running()) {
        if (_started && key == TerminalKey::Enter) Restart();
        return;
    }
    _screen->SendKey(key, modifiers);
    Flush();
}

void TerminalSession::SendCharacter(char32_t character, TerminalModifiers modifiers) {
    if (!Running()) return;
    _screen->SendCharacter(character, modifiers);
    Flush();
}

void TerminalSession::SendFunctionKey(int number, TerminalModifiers modifiers) {
    if (!Running()) return;
    _screen->SendFunctionKey(number, modifiers);
    Flush();
}

void TerminalSession::Paste(std::string_view utf8) {
    if (!Running() || utf8.empty()) return;
    _screen->Paste(utf8);
    Flush();
}

bool TerminalSession::Running() const { return _console != nullptr; }
int TerminalSession::Rows() const { return _screen->Rows(); }
int TerminalSession::Columns() const { return _screen->Columns(); }
TerminalCell TerminalSession::Cell(int row, int column) const { return _screen->Cell(row, column); }
std::string TerminalSession::Text(TerminalPosition from, TerminalPosition to) const { return _screen->Text(from, to); }
TerminalCursor TerminalSession::Cursor() const { return _screen->Cursor(); }
const std::string& TerminalSession::Title() const { return _screen->Title(); }
const TerminalPalette& TerminalSession::Palette() const { return _screen->Palette(); }

void TerminalSession::Flush() {
    const auto input = _screen->TakeInput();
    if (_console && !input.empty()) _console->Write(input);
}

void TerminalSession::Notice(std::string_view text) {
    // Programın değil oturumun mesajı: soluk renkte, kendi satırında.
    std::string line = "\r\n\x1b[90m";
    line += text;
    line += "\x1b[0m\r\n";
    _screen->Feed(line);
    static_cast<void>(_screen->TakeInput()); // ekranın kendi cevapları programa gitmez
}
