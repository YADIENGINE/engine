#pragma once
#include <memory>
#include <string>
#include <string_view>
#include "Terminal/TerminalTypes.h"

struct VTerm;
struct VTermScreen;

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

    [[nodiscard]] int Rows() const { return _rows; }
    [[nodiscard]] int Columns() const { return _columns; }
    [[nodiscard]] TerminalCell Cell(int row, int column) const;
    [[nodiscard]] std::string Text(TerminalPosition from, TerminalPosition to) const;
    [[nodiscard]] TerminalCursor Cursor() const;
    [[nodiscard]] const std::string& Title() const { return _title; }
    [[nodiscard]] bool AlternateScreen() const { return _alternateScreen; }
    [[nodiscard]] const TerminalPalette& Palette() const { return _palette; }

private:
    struct Callbacks; // libvterm geri çağrıları (.cpp)
    struct VtermDeleter {
        void operator()(VTerm* vterm) const;
    };
    std::unique_ptr<VTerm, VtermDeleter> _vterm;
    VTermScreen* _screen = nullptr; // _vterm'e aittir
    TerminalPalette _palette;
    std::string _input;
    std::string _title;
    TerminalCursor _cursor;
    int _rows = 0;
    int _columns = 0;
    bool _alternateScreen = false;

public:
    VtScreen(const VtScreen&) = delete;
    VtScreen& operator=(const VtScreen&) = delete;
};
