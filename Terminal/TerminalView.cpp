#include "Terminal/TerminalView.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include "Terminal/TerminalSession.h"
#include "ToolUi/ToolTheme.h"

namespace {
constexpr float LineHeightScale = 1.2f;
constexpr float CursorThickness = 2.0f;
constexpr float DecorationThickness = 1.0f;
constexpr float FauxBoldOffset = 1.0f; // teknik fontun kalın kesimi yok: metin kaydırılarak iki kez çizilir

struct KeyBinding {
    ImGuiKey key;
    TerminalKey terminal;
};
constexpr std::array KeyBindings{
    KeyBinding{ImGuiKey_Enter, TerminalKey::Enter},      KeyBinding{ImGuiKey_KeypadEnter, TerminalKey::Enter},
    KeyBinding{ImGuiKey_Tab, TerminalKey::Tab},          KeyBinding{ImGuiKey_Backspace, TerminalKey::Backspace},
    KeyBinding{ImGuiKey_Escape, TerminalKey::Escape},    KeyBinding{ImGuiKey_UpArrow, TerminalKey::Up},
    KeyBinding{ImGuiKey_DownArrow, TerminalKey::Down},   KeyBinding{ImGuiKey_LeftArrow, TerminalKey::Left},
    KeyBinding{ImGuiKey_RightArrow, TerminalKey::Right}, KeyBinding{ImGuiKey_Insert, TerminalKey::Ins},
    KeyBinding{ImGuiKey_Delete, TerminalKey::Del},       KeyBinding{ImGuiKey_Home, TerminalKey::Home},
    KeyBinding{ImGuiKey_End, TerminalKey::End},          KeyBinding{ImGuiKey_PageUp, TerminalKey::PageUp},
    KeyBinding{ImGuiKey_PageDown, TerminalKey::PageDown}};
constexpr int FunctionKeys = 12; // ImGuiKey_F1..F12 ardışık

ImU32 ToImColor(std::uint32_t rgb) { return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF); }
void AppendUtf8(std::string& text, char32_t character) {
    const auto c = static_cast<std::uint32_t>(character);
    if (c < 0x80) {
        text += static_cast<char>(c);
    } else if (c < 0x800) {
        text += static_cast<char>(0xC0 | (c >> 6));
        text += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        text += static_cast<char>(0xE0 | (c >> 12));
        text += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        text += static_cast<char>(0xF0 | (c >> 18));
        text += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (c & 0x3F));
    }
}
std::string CellText(const TerminalCell& cell) {
    std::string text;
    for (const char32_t character : cell.text) {
        if (!character) break;
        AppendUtf8(text, character);
    }
    return text;
}
bool Plain(const TerminalCell& cell) { return cell.text[0] < 0x7F && cell.text[1] == 0; }
ImGuiKeyChord Modifiers(const ImGuiIO& io) {
    return (io.KeyCtrl ? ImGuiMod_Ctrl : 0) | (io.KeyShift ? ImGuiMod_Shift : 0) | (io.KeyAlt ? ImGuiMod_Alt : 0);
}
// ImGui karakter kuyruğu UTF-16 olabilir: vekil çiftler birleştirilir.
std::vector<char32_t> QueuedCharacters(const ImGuiIO& io) {
    std::vector<char32_t> characters;
    for (int index = 0; index < io.InputQueueCharacters.Size; ++index) {
        auto c = static_cast<char32_t>(io.InputQueueCharacters[index]);
        if (c >= 0xD800 && c < 0xDC00 && index + 1 < io.InputQueueCharacters.Size) {
            const auto low = static_cast<char32_t>(io.InputQueueCharacters[index + 1]);
            if (low >= 0xDC00 && low < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                ++index;
            }
        }
        if (c >= U' ' && c != 0x7F) characters.push_back(c);
    }
    return characters;
}

struct Grid {
    ImVec2 origin;
    float cellWidth;
    float cellHeight;
    float textOffset;
    [[nodiscard]] ImVec2 At(int row, int column) const {
        return {origin.x + static_cast<float>(column) * cellWidth, origin.y + static_cast<float>(row) * cellHeight};
    }
};

TerminalPosition PositionAt(const Grid& grid, ImVec2 mouse, const TerminalSession& session) {
    const int row =
        std::clamp(static_cast<int>(std::floor((mouse.y - grid.origin.y) / grid.cellHeight)), 0, session.Rows() - 1);
    const float column = (mouse.x - grid.origin.x) / grid.cellWidth;
    int boundary = std::clamp(static_cast<int>(std::round(column)), 0, session.Columns());
    if (boundary < session.Columns() && session.Cell(row, boundary).continuation)
        boundary += column < static_cast<float>(boundary) ? -1 : 1;
    return {row, boundary};
}

void DrawRow(ImDrawList& draw, const Grid& grid, int row, const std::vector<TerminalCell>& cells,
             TerminalPosition selectionStart, TerminalPosition selectionEnd) {
    const int columns = static_cast<int>(cells.size());
    // Arka planlar: aynı renkli komşu hücreler tek dikdörtgen.
    for (int column = 0; column < columns;) {
        const auto& cell = cells[static_cast<std::size_t>(column)];
        int end = column + 1;
        while (end < columns && cells[static_cast<std::size_t>(end)].defaultBackground == cell.defaultBackground &&
               cells[static_cast<std::size_t>(end)].background == cell.background)
            ++end;
        if (!cell.defaultBackground) {
            const auto from = grid.At(row, column);
            const auto to = grid.At(row + 1, end);
            draw.AddRectFilled(from, to, ToImColor(cell.background));
        }
        column = end;
    }
    if (row >= selectionStart.row && row <= selectionEnd.row) {
        const int first = row == selectionStart.row ? selectionStart.column : 0;
        const int last = row == selectionEnd.row ? selectionEnd.column : columns;
        if (first < last)
            draw.AddRectFilled(grid.At(row, first), grid.At(row + 1, last),
                               ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
    }
    // Metin: düz ASCII'ler aynı özellikte koşu olarak, diğerleri kendi hücresine hizalı tek tek.
    std::string run;
    int runStart = 0;
    const TerminalCell* runCell = nullptr;
    const auto flush = [&] {
        if (runCell && run.find_first_not_of(' ') != std::string::npos) {
            auto position = grid.At(row, runStart);
            position.y += grid.textOffset;
            const auto color = ToImColor(runCell->foreground);
            draw.AddText(position, color, run.c_str());
            if (runCell->bold) draw.AddText({position.x + FauxBoldOffset, position.y}, color, run.c_str());
        }
        run.clear();
        runCell = nullptr;
    };
    for (int column = 0; column < columns; ++column) {
        const auto& cell = cells[static_cast<std::size_t>(column)];
        if (cell.continuation) continue;
        const auto color = ToImColor(cell.foreground);
        const auto cellPosition = grid.At(row, column);
        const float width = grid.cellWidth * cell.width;
        if (cell.underline) {
            const float y = cellPosition.y + grid.cellHeight - DecorationThickness;
            draw.AddRectFilled({cellPosition.x, y}, {cellPosition.x + width, y + DecorationThickness}, color);
        }
        if (cell.strike) {
            const float y = cellPosition.y + grid.cellHeight * 0.5f;
            draw.AddRectFilled({cellPosition.x, y}, {cellPosition.x + width, y + DecorationThickness}, color);
        }
        if (Plain(cell)) {
            if (runCell && (runCell->foreground != cell.foreground || runCell->bold != cell.bold)) flush();
            if (!runCell) {
                runCell = &cell;
                runStart = column;
            }
            run += cell.text[0] ? static_cast<char>(cell.text[0]) : ' ';
            continue;
        }
        flush();
        const auto text = CellText(cell);
        const ImVec2 position{cellPosition.x, cellPosition.y + grid.textOffset};
        draw.AddText(position, color, text.c_str());
        if (cell.bold) draw.AddText({position.x + FauxBoldOffset, position.y}, color, text.c_str());
    }
    flush();
}

void DrawCursor(ImDrawList& draw, const Grid& grid, const TerminalSession& session, bool focused) {
    const auto cursor = session.Cursor();
    if (!session.Running() || !cursor.visible || cursor.row < 0 || cursor.row >= session.Rows() || cursor.column < 0 ||
        cursor.column >= session.Columns())
        return;
    const auto cell = session.Cell(cursor.row, cursor.column);
    const auto from = grid.At(cursor.row, cursor.column);
    const ImVec2 to{from.x + grid.cellWidth * cell.width, from.y + grid.cellHeight};
    const auto color = ToImColor(session.Palette().foreground);
    if (!focused) {
        draw.AddRect(from, to, color);
        return;
    }
    switch (cursor.shape) {
    case TerminalCursorShape::Block: {
        draw.AddRectFilled(from, to, color);
        const auto text = CellText(cell);
        if (!text.empty())
            draw.AddText({from.x, from.y + grid.textOffset},
                         ToImColor(cell.defaultBackground ? session.Palette().background : cell.background),
                         text.c_str());
        break;
    }
    case TerminalCursorShape::Underline: draw.AddRectFilled({from.x, to.y - CursorThickness}, to, color); break;
    case TerminalCursorShape::Bar      : draw.AddRectFilled(from, {from.x + CursorThickness, to.y}, color); break;
    }
}
} // namespace

void TerminalView::Draw(TerminalSession& session) {
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
    const Grid grid{ImGui::GetCursorScreenPos(), std::max(1.0f, ImGui::CalcTextSize("M").x),
                    std::max(1.0f, std::round(ImGui::GetFontSize() * LineHeightScale)),
                    std::round(ImGui::GetFontSize() * (LineHeightScale - 1.0f) * 0.5f)};
    const auto available = ImGui::GetContentRegionAvail();
    session.Resize(std::max(1, static_cast<int>(available.y / grid.cellHeight)),
                   std::max(1, static_cast<int>(available.x / grid.cellWidth)));
    if (_rows != session.Rows() || _columns != session.Columns()) {
        _selectionAnchor = _selectionEnd = {};
        _rows = session.Rows();
        _columns = session.Columns();
    }
    ImGui::InvisibleButton("##terminal", {std::max(1.0f, available.x), std::max(1.0f, available.y)},
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        _selectionAnchor = _selectionEnd = PositionAt(grid, ImGui::GetMousePos(), session);
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _selectionEnd = PositionAt(grid, ImGui::GetMousePos(), session);
    _focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
               !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (_focused) {
        // rlImGui karakterleri yalnız WantCaptureKeyboard true iken kuyruğa alır. InvisibleButton klavye
        // odağını tutmaz; backend NewFrame'den önce okuduğu için bayrak şimdi de yayınlanır.
        ImGui::SetNextFrameWantCaptureKeyboard(true);
        ImGui::GetIO().WantCaptureKeyboard = true;
        HandleInput(session);
    }

    auto& draw = *ImGui::GetWindowDrawList();
    const auto& palette = session.Palette();
    draw.AddRectFilled(grid.origin, {grid.origin.x + available.x, grid.origin.y + available.y},
                       ToImColor(palette.background));
    const auto selectionStart = std::min(_selectionAnchor, _selectionEnd);
    const auto selectionEnd = std::max(_selectionAnchor, _selectionEnd);
    std::vector<TerminalCell> cells(static_cast<std::size_t>(session.Columns()));
    for (int row = 0; row < session.Rows(); ++row) {
        for (int column = 0; column < session.Columns(); ++column)
            cells[static_cast<std::size_t>(column)] = session.Cell(row, column);
        DrawRow(draw, grid, row, cells, selectionStart, selectionEnd);
    }
    DrawCursor(draw, grid, session, _focused);
    ImGui::PopFont();

    if (ImGui::BeginPopupContextItem("##terminal-menu")) {
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, _selectionAnchor != _selectionEnd)) CopySelection(session);
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, session.Running()))
            if (const char* text = ImGui::GetClipboardText()) {
                _selectionAnchor = _selectionEnd = {};
                session.Paste(text);
            }
        if (ImGui::MenuItem("Restart")) {
            _selectionAnchor = _selectionEnd = {};
            session.Restart();
        }
        ImGui::EndPopup();
    }
}

void TerminalView::HandleInput(TerminalSession& session) {
    const auto& io = ImGui::GetIO();
    const TerminalModifiers modifiers{.shift = io.KeyShift, .alt = io.KeyAlt, .ctrl = io.KeyCtrl};
    const auto chord = Modifiers(io);
    // AltGr = Ctrl+Alt: yazdığı karakterler (ör. Türkçe klavyede @) kuyruktan gelir, kontrol karakteri değildir.
    const bool control = io.KeyCtrl && !io.KeyAlt;
    const auto pressed = [&](ImGuiKey key, bool repeat = true) {
        return ImGui::IsKeyPressed(key, repeat) && !Reserved(chord | key);
    };
    const bool hasSelection = _selectionAnchor != _selectionEnd;
    const bool copy = control && pressed(ImGuiKey_C, false) && (hasSelection || io.KeyShift);
    if (copy) CopySelection(session);
    if (hasSelection && pressed(ImGuiKey_Escape)) {
        _selectionAnchor = _selectionEnd = {};
        return;
    }

    // Basılı tutulan yapıştırma tekrarlanmaz.
    const bool paste =
        (control && pressed(ImGuiKey_V, false)) || (io.KeyShift && !io.KeyCtrl && pressed(ImGuiKey_Insert, false));
    if (paste)
        if (const char* text = ImGui::GetClipboardText()) {
            _selectionAnchor = _selectionEnd = {};
            session.Paste(text);
        }
    for (const auto& binding : KeyBindings)
        if (pressed(binding.key) && !(paste && binding.key == ImGuiKey_Insert)) {
            _selectionAnchor = _selectionEnd = {};
            session.SendKey(binding.terminal, modifiers);
        }
    for (int number = 1; number <= FunctionKeys; ++number)
        if (pressed(static_cast<ImGuiKey>(ImGuiKey_F1 + number - 1))) {
            _selectionAnchor = _selectionEnd = {};
            session.SendFunctionKey(number, modifiers);
        }
    if (control)
        for (int key = ImGuiKey_A; key <= ImGuiKey_Z; ++key) {
            // Seçim varken Ctrl+C kopyalar; yokken programa kesme gider. Ctrl+Shift+C daima kopyalar.
            if (key == ImGuiKey_V || (key == ImGuiKey_C && (hasSelection || io.KeyShift))) continue;
            if (pressed(static_cast<ImGuiKey>(key))) {
                _selectionAnchor = _selectionEnd = {};
                session.SendCharacter(static_cast<char32_t>(U'a' + (key - ImGuiKey_A)), modifiers);
            }
        }
    if (!control) {
        const TerminalModifiers characterModifiers{.alt = io.KeyAlt && !io.KeyCtrl};
        for (const char32_t character : QueuedCharacters(io)) {
            _selectionAnchor = _selectionEnd = {};
            session.SendCharacter(character, characterModifiers);
        }
    }
}

void TerminalView::CopySelection(const TerminalSession& session) const {
    if (_selectionAnchor == _selectionEnd) return;
    const auto text = session.Text(_selectionAnchor, _selectionEnd);
    ImGui::SetClipboardText(text.c_str());
}

bool TerminalView::Reserved(ImGuiKeyChord chord) const {
    return std::find(_reserved.begin(), _reserved.end(), chord) != _reserved.end();
}
