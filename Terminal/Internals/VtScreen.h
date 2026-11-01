#pragma once
#include <vterm.h>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Terminal/TerminalTypes.h"

// libvterm sarmalayıcısı: programın çıktısını ekran hücrelerine çevirir, klavyeyi VT dizilerine kodlar.
// Programa gidecek baytlar (tuşlar, yapıştırma, imleç sorgusu cevapları) TakeInput ile alınır. Thread güvenli değil.
class VtScreen {
public:
    VtScreen(int rows, int columns, const TerminalPalette& palette);
    ~VtScreen();

    void Feed(std::string_view bytes);
    void Resize(int rows, int columns);
    void Reset();
    void SendKey(TerminalKey key, TerminalModifiers modifiers);
    void SendCharacter(char32_t character, TerminalModifiers modifiers);
    void SendFunctionKey(int number, TerminalModifiers modifiers); // F1..F12
    void Paste(std::string_view utf8);
    [[nodiscard]] std::string TakeInput();
    // Whether the program rang the bell (BEL) since the last call.
    [[nodiscard]] bool TakeBell() { return std::exchange(_bell, false); }

    [[nodiscard]] int Rows() const { return _rows; }
    [[nodiscard]] int Columns() const { return _columns; }
    [[nodiscard]] int HistoryRows() const { return _alternateScreen ? 0 : static_cast<int>(_history.size()); }
    [[nodiscard]] std::int64_t HistoryEnd() const { return _historyEnd; }
    // Negative rows address scrollback; -1 is the line immediately above the live screen.
    [[nodiscard]] TerminalCell Cell(int row, int column) const;
    [[nodiscard]] std::string Text(TerminalPosition from, TerminalPosition to) const;
    [[nodiscard]] TerminalCursor Cursor() const;
    [[nodiscard]] const std::string& Title() const { return _title; }
    [[nodiscard]] bool AlternateScreen() const { return _alternateScreen; }
    [[nodiscard]] const TerminalPalette& Palette() const { return _palette; }

private:
    struct HistoryLine {
        std::vector<VTermScreenCell> cells;
        bool continuation = false;
    };
    struct Callbacks; // libvterm geri çağrıları (.cpp)
    [[nodiscard]] bool Continues(int row) const;
    [[nodiscard]] int LineColumns(int row) const;
    struct VtermDeleter {
        void operator()(VTerm* vterm) const;
    };
    std::unique_ptr<VTerm, VtermDeleter> _vterm;
    VTermScreen* _screen = nullptr; // _vterm'e aittir
    TerminalPalette _palette;
    std::string _input;
    std::string _title;
    TerminalCursor _cursor;
    std::deque<HistoryLine> _history;
    std::vector<bool> _continuations;
    std::int64_t _historyEnd = 0;
    int _pushedRows = 0;
    int _rows = 0;
    int _columns = 0;
    bool _alternateScreen = false;
    bool _resizing = false;
    bool _bell = false;

public:
    VtScreen(const VtScreen&) = delete;
    VtScreen& operator=(const VtScreen&) = delete;
};
