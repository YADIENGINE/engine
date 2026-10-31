#include "Terminal/Internals/VtScreen.h"
#include <vterm.h>
#include <algorithm>
#include <cstdint>
#include <utility>

namespace {
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
    static const VTermScreenCallbacks& Table() {
        static const VTermScreenCallbacks table = [] {
            VTermScreenCallbacks callbacks{};
            callbacks.movecursor = MoveCursor;
            callbacks.settermprop = SetProperty;
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
    vterm_screen_enable_altscreen(_screen, 1);
    vterm_screen_enable_reflow(_screen, true);
    Reset();
}
VtScreen::~VtScreen() = default;

void VtScreen::Reset() {
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
    _rows = rows;
    _columns = columns;
    vterm_set_size(_vterm.get(), rows, columns);
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
    VTermScreenCell cell{};
    if (!vterm_screen_get_cell(_screen, VTermPos{row, column}, &cell)) return result;
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
        return TerminalPosition{std::clamp(position.row, 0, _rows - 1), std::clamp(position.column, 0, _columns)};
    };
    from = clamp(from);
    to = clamp(to);
    if (to < from) std::swap(from, to);
    if (from == to) return {};
    // Geniş karakterin ortasındaki sınır karakteri bölmez.
    if (from.column < _columns && Cell(from.row, from.column).continuation) --from.column;
    if (to.column < _columns && Cell(to.row, to.column).continuation) ++to.column;

    const auto* state = vterm_obtain_state(_vterm.get());
    std::string text;
    for (int row = from.row; row <= to.row; ++row) {
        const int first = row == from.row ? from.column : 0;
        const int last = row == to.row ? to.column : _columns;
        const VTermRect rect{row, row + 1, first, last};
        std::string line(vterm_screen_get_text(_screen, nullptr, 0, rect), '\0');
        vterm_screen_get_text(_screen, line.data(), line.size(), rect);
        const bool wrapped = row < to.row && vterm_state_get_lineinfo(state, row + 1)->continuation;
        if (!wrapped) line.erase(line.find_last_not_of(' ') + 1);
        text += line;
        if (row < to.row && !wrapped) text += '\n';
    }
    return text;
}

TerminalCursor VtScreen::Cursor() const { return _cursor; }
