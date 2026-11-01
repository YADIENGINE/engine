#include "Terminal/Internals/VtScreen.h"
#include <vterm.h>
#include <algorithm>
#include <cstdint>
#include <utility>
#include "Terminal/Internals/TerminalText.h"

namespace {
constexpr std::size_t HistoryLimit = 10000;

VTermColor ToVterm(std::uint32_t rgb) {
    VTermColor color;
    vterm_color_rgb(&color, static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
                    static_cast<std::uint8_t>(rgb));
    return color;
}
std::uint32_t ToRgb(const VTermScreen* screen, VTermColor color) {
    vterm_screen_convert_color_to_rgb(screen, &color);
    return (static_cast<std::uint32_t>(color.rgb.red) << 16) | (static_cast<std::uint32_t>(color.rgb.green) << 8) |
           color.rgb.blue;
}
VTermModifier ToVterm(TerminalModifiers modifiers) {
    int mod = VTERM_MOD_NONE;
    if (modifiers.shift) mod |= VTERM_MOD_SHIFT;
    if (modifiers.alt) mod |= VTERM_MOD_ALT;
    if (modifiers.ctrl) mod |= VTERM_MOD_CTRL;
    return static_cast<VTermModifier>(mod);
}
VTermKey ToVterm(TerminalKey key) {
    switch (key) {
    case TerminalKey::Enter    : return VTERM_KEY_ENTER;
    case TerminalKey::Tab      : return VTERM_KEY_TAB;
    case TerminalKey::Backspace: return VTERM_KEY_BACKSPACE;
    case TerminalKey::Escape   : return VTERM_KEY_ESCAPE;
    case TerminalKey::Up       : return VTERM_KEY_UP;
    case TerminalKey::Down     : return VTERM_KEY_DOWN;
    case TerminalKey::Left     : return VTERM_KEY_LEFT;
    case TerminalKey::Right    : return VTERM_KEY_RIGHT;
    case TerminalKey::Ins      : return VTERM_KEY_INS;
    case TerminalKey::Del      : return VTERM_KEY_DEL;
    case TerminalKey::Home     : return VTERM_KEY_HOME;
    case TerminalKey::End      : return VTERM_KEY_END;
    case TerminalKey::PageUp   : return VTERM_KEY_PAGEUP;
    case TerminalKey::PageDown : return VTERM_KEY_PAGEDOWN;
    }
    return VTERM_KEY_NONE;
}
bool IsLiteral(TerminalKey key) {
    return key == TerminalKey::Enter || key == TerminalKey::Backspace || key == TerminalKey::Escape;
}
} // namespace

struct VtScreen::Callbacks {
    static int Damage(VTermRect rect, void* user) {
        auto& self = *static_cast<VtScreen*>(user);
        // During resize libvterm replaces its lineinfo before updating the state's pointers.
        if (self._resizing || self._alternateScreen) return 1;
        const auto* state = vterm_obtain_state(self._vterm.get());
        self._continuations.resize(static_cast<std::size_t>(self._rows));
        for (int row = rect.start_row; row < std::min(rect.end_row, self._rows); ++row)
            self._continuations[static_cast<std::size_t>(row)] = vterm_state_get_lineinfo(state, row)->continuation;
        self._pushedRows = 0;
        return 1;
    }
    static int PushLine(int columns, const VTermScreenCell* cells, void* user) {
        auto& self = *static_cast<VtScreen*>(user);
        // libvterm has already moved lineinfo when this callback runs. Damage callbacks keep the outgoing
        // continuation flags before that move, including multiple lines pushed by a single scroll.
        const bool continuation = self._pushedRows < static_cast<int>(self._continuations.size()) &&
                                  self._continuations[static_cast<std::size_t>(self._pushedRows)];
        self._history.push_back({{cells, cells + columns}, continuation});
        if (self._history.size() > HistoryLimit) self._history.pop_front();
        ++self._pushedRows;
        ++self._historyEnd;
        return 1;
    }
    static int PopLine(int columns, VTermScreenCell* cells, void* user) {
        auto& self = *static_cast<VtScreen*>(user);
        if (self._history.empty()) return 0;
        VTermScreenCell blank{};
        blank.width = 1;
        vterm_state_get_default_colors(vterm_obtain_state(self._vterm.get()), &blank.fg, &blank.bg);
        blank.fg.type |= VTERM_COLOR_DEFAULT_FG;
        blank.bg.type |= VTERM_COLOR_DEFAULT_BG;
        std::fill_n(cells, columns, blank);
        const auto& line = self._history.back().cells;
        std::copy_n(line.begin(), std::min(static_cast<std::size_t>(columns), line.size()), cells);
        // A wide glyph cannot straddle the right edge after a width change.
        if (columns > 0 && cells[columns - 1].width == 2) cells[columns - 1] = blank;
        self._history.pop_back();
        --self._historyEnd;
        return 1;
    }
    static int ClearHistory(void* user) {
        auto& self = *static_cast<VtScreen*>(user);
        self._history.clear();
        self._historyEnd = 0;
        self._pushedRows = 0;
        return 1;
    }
    static void Output(const char* bytes, std::size_t length, void* user) {
        static_cast<VtScreen*>(user)->_input.append(bytes, length);
    }
    static int MoveCursor(VTermPos position, VTermPos, int visible, void* user) {
        auto& cursor = static_cast<VtScreen*>(user)->_cursor;
        cursor.row = position.row;
        cursor.column = position.col;
        cursor.visible = visible != 0;
        return 1;
    }
    static int SetProperty(VTermProp property, VTermValue* value, void* user) {
        auto& self = *static_cast<VtScreen*>(user);
        switch (property) {
        case VTERM_PROP_CURSORVISIBLE: self._cursor.visible = value->boolean != 0; return 1;
        case VTERM_PROP_ALTSCREEN    : self._alternateScreen = value->boolean != 0; return 1;
        case VTERM_PROP_CURSORSHAPE:
            self._cursor.shape = value->number == VTERM_PROP_CURSORSHAPE_UNDERLINE  ? TerminalCursorShape::Underline
                                 : value->number == VTERM_PROP_CURSORSHAPE_BAR_LEFT ? TerminalCursorShape::Bar
                                                                                    : TerminalCursorShape::Block;
            return 1;
        case VTERM_PROP_TITLE:
            if (value->string.initial) self._title.clear();
            self._title.append(value->string.str, value->string.len);
            return 1;
        default: return 0;
        }
    }
    static int Bell(void* user) {
        static_cast<VtScreen*>(user)->_bell = true;
        return 1;
    }
    static const VTermScreenCallbacks& Table() {
        static const VTermScreenCallbacks table = [] {
            VTermScreenCallbacks callbacks{};
            callbacks.damage = Damage;
            callbacks.bell = Bell;
            callbacks.movecursor = MoveCursor;
            callbacks.settermprop = SetProperty;
            callbacks.sb_pushline = PushLine;
            callbacks.sb_popline = PopLine;
            callbacks.sb_clear = ClearHistory;
            return callbacks;
        }();
        return table;
    }
};

void VtScreen::VtermDeleter::operator()(VTerm* vterm) const { vterm_free(vterm); }

VtScreen::VtScreen(int rows, int columns, const TerminalPalette& palette)
    : _vterm(vterm_new(std::max(rows, 1), std::max(columns, 1)))
    , _palette(palette)
    , _rows(std::max(rows, 1))
    , _columns(std::max(columns, 1)) {
    vterm_set_utf8(_vterm.get(), 1);
    vterm_output_set_callback(_vterm.get(), Callbacks::Output, this);
    _screen = vterm_obtain_screen(_vterm.get());
    vterm_screen_set_callbacks(_screen, &Callbacks::Table(), this);
    // Continuation snapshots must be updated after each glyph, before another line can scroll out.
    vterm_screen_set_damage_merge(_screen, VTERM_DAMAGE_CELL);
    vterm_screen_enable_altscreen(_screen, 1);
    vterm_screen_enable_reflow(_screen, true);
    Reset();
}
VtScreen::~VtScreen() = default;

void VtScreen::Reset() {
    _history.clear();
    _historyEnd = 0;
    _pushedRows = 0;
    _continuations.assign(static_cast<std::size_t>(_rows), false);
    _alternateScreen = false;
    vterm_screen_reset(_screen, 1);
    auto* state = vterm_obtain_state(_vterm.get());
    for (int index = 0; index < static_cast<int>(_palette.ansi.size()); ++index) {
        const auto color = ToVterm(_palette.ansi[static_cast<std::size_t>(index)]);
        vterm_state_set_palette_color(state, index, &color);
    }
    const auto foreground = ToVterm(_palette.foreground);
    const auto background = ToVterm(_palette.background);
    vterm_screen_set_default_colors(_screen, &foreground, &background);
    _title.clear();
    _cursor = {};
    _alternateScreen = false;
}

void VtScreen::Feed(std::string_view bytes) {
    if (bytes.empty()) return;
    vterm_input_write(_vterm.get(), bytes.data(), bytes.size());
    vterm_screen_flush_damage(_screen);
}

void VtScreen::Resize(int rows, int columns) {
    rows = std::max(rows, 1);
    columns = std::max(columns, 1);
    if (rows == _rows && columns == _columns) return;
    _pushedRows = 0;
    _resizing = true;
    vterm_set_size(_vterm.get(), rows, columns);
    _resizing = false;
    _rows = rows;
    _columns = columns;
    _continuations.resize(static_cast<std::size_t>(_rows));
    const auto* state = vterm_obtain_state(_vterm.get());
    if (!_alternateScreen)
        for (int row = 0; row < _rows; ++row)
            _continuations[static_cast<std::size_t>(row)] = vterm_state_get_lineinfo(state, row)->continuation;
    _pushedRows = 0;
    vterm_screen_flush_damage(_screen);
}

void VtScreen::SendKey(TerminalKey key, TerminalModifiers modifiers) {
    // ConPTY CSI-u dizilerini tanımaz: Enter/Backspace/Escape'te Shift/Ctrl düşer. Shift+Enter AI CLI'larının
    // çok satırlı girişi için Alt+Enter (ESC CR) gönderir; Ctrl+Backspace kelime siler (BS).
    if (key == TerminalKey::Enter && modifiers.shift && !modifiers.ctrl && !modifiers.alt) {
        _input += "\x1b\r";
        return;
    }
    if (key == TerminalKey::Backspace && modifiers.ctrl && !modifiers.alt && !modifiers.shift) {
        _input += '\b';
        return;
    }
    if (IsLiteral(key)) modifiers.shift = modifiers.ctrl = false;
    vterm_keyboard_key(_vterm.get(), ToVterm(key), ToVterm(modifiers));
}

void VtScreen::SendCharacter(char32_t character, TerminalModifiers modifiers) {
    // Ctrl+harf kontrol karakteridir; büyük harf de aynı koda gider (Ctrl+Shift+C kopyalamayı görünüm yakalar).
    if (modifiers.ctrl && character >= U'A' && character <= U'Z') character += U'a' - U'A';
    vterm_keyboard_unichar(_vterm.get(), static_cast<std::uint32_t>(character), ToVterm(modifiers));
}

void VtScreen::SendFunctionKey(int number, TerminalModifiers modifiers) {
    if (number >= 1 && number <= 12)
        vterm_keyboard_key(_vterm.get(), static_cast<VTermKey>(VTERM_KEY_FUNCTION(number)), ToVterm(modifiers));
}

void VtScreen::Paste(std::string_view utf8) {
    // Satır sonları CR'dir (Enter); program bracketed paste açtıysa libvterm sarar.
    vterm_keyboard_start_paste(_vterm.get());
    for (std::size_t index = 0; index < utf8.size(); ++index) {
        const char c = utf8[index];
        if (c == '\r') {
            _input += '\r';
            if (index + 1 < utf8.size() && utf8[index + 1] == '\n') ++index;
        } else if (c == '\n') {
            _input += '\r';
        } else {
            _input += c;
        }
    }
    vterm_keyboard_end_paste(_vterm.get());
}

std::string VtScreen::TakeInput() { return std::exchange(_input, {}); }

TerminalCell VtScreen::Cell(int row, int column) const {
    TerminalCell result;
    result.foreground = _palette.foreground;
    result.background = _palette.background;
    VTermScreenCell cell{};
    if (row < 0) {
        if (row < -HistoryRows() || column < 0 || column >= LineColumns(row)) return result;
        cell = _history[static_cast<std::size_t>(HistoryRows() + row)].cells[static_cast<std::size_t>(column)];
    } else if (!vterm_screen_get_cell(_screen, VTermPos{row, column}, &cell)) {
        return result;
    }
    if (cell.chars[0] == static_cast<std::uint32_t>(-1)) {
        result.continuation = true;
        return result;
    }
    for (std::size_t index = 0; index < VTERM_MAX_CHARS_PER_CELL && cell.chars[index]; ++index)
        result.text[index] = static_cast<char32_t>(cell.chars[index]);
    result.foreground = ToRgb(_screen, cell.fg);
    result.background = ToRgb(_screen, cell.bg);
    result.defaultBackground = VTERM_COLOR_IS_DEFAULT_BG(&cell.bg);
    if (cell.attrs.reverse) {
        std::swap(result.foreground, result.background);
        result.defaultBackground = false;
    }
    result.bold = cell.attrs.bold != 0;
    result.italic = cell.attrs.italic != 0;
    result.underline = cell.attrs.underline != 0;
    result.strike = cell.attrs.strike != 0;
    result.width = cell.width > 1 ? 2 : 1;
    return result;
}

std::string VtScreen::Text(TerminalPosition from, TerminalPosition to) const {
    const auto clamp = [&](TerminalPosition position) {
        const int row = std::clamp(position.row, -HistoryRows(), _rows - 1);
        return TerminalPosition{row, std::clamp(position.column, 0, LineColumns(row))};
    };
    from = clamp(from);
    to = clamp(to);
    if (to < from) std::swap(from, to);
    if (from == to) return {};
    // Geniş karakterin ortasındaki sınır karakteri bölmez.
    if (from.column < LineColumns(from.row) && Cell(from.row, from.column).continuation) --from.column;
    if (to.column < LineColumns(to.row) && Cell(to.row, to.column).continuation) ++to.column;

    std::string text;
    for (int row = from.row; row <= to.row; ++row) {
        const int first = row == from.row ? from.column : 0;
        const int last = row == to.row ? to.column : LineColumns(row);
        std::string line;
        for (int column = first; column < last; ++column) {
            const auto cell = Cell(row, column);
            if (cell.continuation) continue;
            line += cell.text[0] ? TerminalCellText(cell) : " ";
        }
        const bool wrapped = row < to.row && Continues(row + 1);
        if (!wrapped) line.erase(line.find_last_not_of(' ') + 1);
        text += line;
        if (row < to.row && !wrapped) text += '\n';
    }
    return text;
}

bool VtScreen::Continues(int row) const {
    if (row < 0) return _history[static_cast<std::size_t>(HistoryRows() + row)].continuation;
    return vterm_state_get_lineinfo(vterm_obtain_state(_vterm.get()), row)->continuation;
}

int VtScreen::LineColumns(int row) const {
    if (row < 0) return static_cast<int>(_history[static_cast<std::size_t>(HistoryRows() + row)].cells.size());
    return _columns;
}

TerminalCursor VtScreen::Cursor() const { return _cursor; }
