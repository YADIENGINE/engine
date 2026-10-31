#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>
#include "Terminal/Internals/VtScreen.h"
#include "Terminal/TerminalSession.h"

namespace {
constexpr TerminalModifiers NoModifiers{};
constexpr TerminalModifiers Shift{.shift = true};
constexpr TerminalModifiers Ctrl{.ctrl = true};

// Satırın ASCII metni, sondaki boşluklar atılmış.
template <typename Screen> std::string RowText(const Screen& screen, int row) {
    std::string text;
    for (int column = 0; column < screen.Columns(); ++column) {
        const auto cell = screen.Cell(row, column);
        if (cell.continuation) continue;
        text += cell.text[0] == 0 ? ' ' : static_cast<char>(cell.text[0]);
    }
    return text.substr(0, text.find_last_not_of(' ') + 1);
}
template <typename Screen> bool ScreenContains(const Screen& screen, const std::string& needle) {
    for (int row = 0; row < screen.Rows(); ++row)
        if (RowText(screen, row).find(needle) != std::string::npos) return true;
    return false;
}
// Program çıkana kadar pompalar; süre sınırı aşılırsa false.
bool PumpUntilExit(TerminalSession& session) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        session.Pump();
        if (session.ExitCode()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}
} // namespace

TEST_CASE("Terminal.Screen.TextColorsAndCursor", "[terminal]") {
    const TerminalPalette palette;
    VtScreen screen(4, 20, palette);
    screen.Feed("ab\x1b[31mc\x1b[7md\x1b[0m");
    CHECK(RowText(screen, 0) == "abcd");
    CHECK(screen.Cell(0, 0).foreground == palette.foreground);
    CHECK(screen.Cell(0, 0).defaultBackground);
    CHECK(screen.Cell(0, 2).foreground == palette.ansi[1]);
    // Ters: renkler yer değiştirir ve arka plan artık varsayılan sayılmaz.
    CHECK(screen.Cell(0, 3).background == palette.ansi[1]);
    CHECK_FALSE(screen.Cell(0, 3).defaultBackground);
    CHECK(screen.Cursor().row == 0);
    CHECK(screen.Cursor().column == 4);

    screen.Feed("\x1b[38;2;10;20;30mx");
    CHECK(screen.Cell(0, 4).foreground == 0x0A141E);
}

TEST_CASE("Terminal.Screen.WideCharactersTitleAndAlternateScreen", "[terminal]") {
    VtScreen screen(4, 20, {});
    screen.Feed("\xE7\x95\x8C!"); // U+754C geniş karakter
    CHECK(screen.Cell(0, 0).text[0] == U'\u754C');
    CHECK(screen.Cell(0, 0).width == 2);
    CHECK(screen.Cell(0, 1).continuation);
    CHECK(screen.Cell(0, 2).text[0] == U'!');

    screen.Feed("\x1b]0;My title\x07");
    CHECK(screen.Title() == "My title");
    screen.Feed("\x1b[?1049h");
    CHECK(screen.AlternateScreen());
    screen.Feed("\x1b[?1049l");
    CHECK_FALSE(screen.AlternateScreen());
}

TEST_CASE("Terminal.Screen.ResizeKeepsText", "[terminal]") {
    VtScreen screen(4, 20, {});
    screen.Feed("hello");
    screen.Resize(6, 30);
    CHECK(screen.Rows() == 6);
    CHECK(screen.Columns() == 30);
    CHECK(RowText(screen, 0) == "hello");
}

TEST_CASE("Terminal.Keys.EncodingForConPty", "[terminal]") {
    VtScreen screen(4, 20, {});
    screen.SendKey(TerminalKey::Enter, NoModifiers);
    CHECK(screen.TakeInput() == "\r");
    // Shift+Enter AI CLI'larında yeni satırdır (ESC CR); ConPTY CSI-u'yu tanımaz.
    screen.SendKey(TerminalKey::Enter, Shift);
    CHECK(screen.TakeInput() == "\x1b\r");
    screen.SendKey(TerminalKey::Enter, Ctrl);
    CHECK(screen.TakeInput() == "\r");
    screen.SendKey(TerminalKey::Backspace, Ctrl);
    CHECK(screen.TakeInput() == "\b");
    screen.SendKey(TerminalKey::Up, NoModifiers);
    CHECK(screen.TakeInput() == "\x1b[A");
    screen.SendCharacter(U'c', Ctrl);
    CHECK(screen.TakeInput() == "\x03");
    screen.SendCharacter(U'C', Ctrl);
    CHECK(screen.TakeInput() == "\x03");
    screen.SendCharacter(U'\u011F', NoModifiers); // ğ
    CHECK(screen.TakeInput() == "\xC4\x9F");
}

TEST_CASE("Terminal.Screen.SelectedTextTrimsLinesAndAcceptsReverseDrag", "[terminal]") {
    VtScreen screen(4, 12, {});
    screen.Feed("hello  \r\nworld");
    CHECK(screen.Text({0, 1}, {1, 3}) == "ello\nwor");
    CHECK(screen.Text({1, 3}, {0, 1}) == "ello\nwor");
    CHECK(screen.Text({0, 0}, {0, 0}).empty());
    CHECK(screen.Text({0, 0}, {1, 0}) == "hello\n");
}

TEST_CASE("Terminal.Screen.SelectedTextJoinsWrappedLines", "[terminal]") {
    VtScreen screen(4, 4, {});
    screen.Feed("abc def\r\nnext");
    // İlk satırın sonundaki gerçek boşluk, sarılan kelimeleri ayırmaya devam eder.
    CHECK(screen.Text({0, 0}, {1, 3}) == "abc def");
    CHECK(screen.Text({0, 0}, {2, 4}) == "abc def\nnext");
}

TEST_CASE("Terminal.Screen.SelectedTextPreservesUnicodeAndWholeWideCharacters", "[terminal]") {
    VtScreen screen(4, 12, {});
    const std::string text = "\xC4\x9F\xE7\x95\x8C"
                             "e\xCC\x81"; // ğ界é (birleşen aksan)
    screen.Feed(text);
    CHECK(screen.Text({0, 0}, {0, 4}) == text);
    CHECK(screen.Text({0, 2}, {0, 3}) == "\xE7\x95\x8C");
    CHECK(screen.Text({0, 1}, {0, 2}) == "\xE7\x95\x8C");
}

TEST_CASE("Terminal.Keys.PasteNormalizesLinesAndBrackets", "[terminal]") {
    VtScreen screen(4, 20, {});
    screen.Paste("a\r\nb\nc");
    CHECK(screen.TakeInput() == "a\rb\rc");
    screen.Feed("\x1b[?2004h"); // program bracketed paste ister
    screen.Paste("x\ny");
    CHECK(screen.TakeInput() == "\x1b[200~x\ry\x1b[201~");
}

TEST_CASE("Terminal.Session.RunsProgramAndReportsExit", "[terminal]") {
    TerminalSession session;
    REQUIRE(session.Start({.commandLine = "cmd.exe /d /c echo terminal-smoke& exit 3"}));
    REQUIRE(PumpUntilExit(session));
    CHECK(*session.ExitCode() == 3);
    CHECK_FALSE(session.Running());
    CHECK(ScreenContains(session, "terminal-smoke"));
    CHECK(ScreenContains(session, "exited with code 3"));
}

TEST_CASE("Terminal.Session.StopsARunningProgram", "[terminal]") {
    TerminalSession session;
    REQUIRE(session.Start({.commandLine = "cmd.exe /d /k"}));
    session.Pump();
    CHECK(session.Running());
    const auto started = std::chrono::steady_clock::now();
    session.Stop();
    CHECK_FALSE(session.Running());
    CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(5));
}

TEST_CASE("Terminal.Session.ReportsStartFailure", "[terminal]") {
    TerminalSession session;
    CHECK_FALSE(session.Start({.commandLine = "Z:\\definitely\\missing\\program.exe"}));
    CHECK_FALSE(session.Running());
    CHECK(ScreenContains(session, "Could not start"));
}
