#pragma once
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Terminal/TerminalTypes.h"

class PseudoConsole;
class VtScreen;

struct TerminalStartOptions {
    std::string commandLine; // UTF-8; boşsa DefaultTerminalShell()
    std::filesystem::path directory;
    std::vector<std::pair<std::string, std::string>> environment; // tool ortamına eklenir / üstüne yazar
};

// pwsh PATH'teyse o, yoksa Windows PowerShell (logo'suz).
[[nodiscard]] std::string DefaultTerminalShell();

// Sahte konsolda çalışan bir program ve onun ekranı. ImGui bilmez; görünüm ekranı okur, input'u buraya yazar.
// Bütün çağrılar aynı (UI) thread'inden. Program çıkınca ekran kalır; Enter aynı ayarlarla yeniden başlatır.
class TerminalSession {
public:
    explicit TerminalSession(const TerminalPalette& palette = {});
    ~TerminalSession();

    // Çalışan program varsa önce kapatılır. Başarısızsa false; hata ekrana da yazılır.
    bool Start(const TerminalStartOptions& options);
    bool Restart() { return Start(_options); }
    void Stop();
    // Her kare: çıktıyı ekrana işler, bekleyen input'u gönderir, çıkışı fark eder.
    void Pump();
    void Resize(int rows, int columns);

    void SendKey(TerminalKey key, TerminalModifiers modifiers);
    void SendCharacter(char32_t character, TerminalModifiers modifiers);
    void SendFunctionKey(int number, TerminalModifiers modifiers); // F1..F12
    void Paste(std::string_view utf8);

    [[nodiscard]] bool Started() const { return _started; }
    [[nodiscard]] bool Running() const;
    [[nodiscard]] std::optional<int> ExitCode() const { return _exitCode; }
    [[nodiscard]] int Rows() const;
    [[nodiscard]] int Columns() const;
    [[nodiscard]] TerminalCell Cell(int row, int column) const;
    // İki hücre sınırı arasındaki UTF-8 metin; son sınır dahil değildir. Sarılmış satırlar birleştirilir.
    [[nodiscard]] std::string Text(TerminalPosition from, TerminalPosition to) const;
    [[nodiscard]] TerminalCursor Cursor() const;
    [[nodiscard]] const std::string& Title() const;
    [[nodiscard]] const TerminalPalette& Palette() const;

private:
    void Flush();
    void Notice(std::string_view text);

    std::unique_ptr<VtScreen> _screen;
    std::unique_ptr<PseudoConsole> _console;
    TerminalStartOptions _options;
    std::optional<int> _exitCode;
    std::optional<std::chrono::steady_clock::time_point> _exitSeen; // çıkış görüldü, son çıktı boşaltılıyor
    bool _started = false;

public:
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
};
