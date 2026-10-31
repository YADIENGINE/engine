#pragma once
#include <imgui.h>
#include <utility>
#include <vector>
#include "Terminal/TerminalTypes.h"

class TerminalSession;

// Bir TerminalSession'ı geçerli ImGui penceresinin kalan alanına çizer. Pencere odaktayken klavye ve yapıştırma
// terminale gider; ekran boyutu alana göre ayarlanır. Aracın kendine ayırdığı akorlar (ör. editörün Ctrl+J'si)
// terminale gönderilmez.
class TerminalView {
public:
    explicit TerminalView(std::vector<ImGuiKeyChord> reserved = {})
        : _reserved(std::move(reserved)) {}
    void Draw(TerminalSession& session);
    // Son çizimde pencere odaktaydı (klavye terminale gidiyordu).
    [[nodiscard]] bool Focused() const { return _focused; }

private:
    void HandleInput(TerminalSession& session);
    void CopySelection(const TerminalSession& session) const;
    [[nodiscard]] bool Reserved(ImGuiKeyChord chord) const;

    std::vector<ImGuiKeyChord> _reserved;
    TerminalPosition _selectionAnchor;
    TerminalPosition _selectionEnd;
    int _rows = 0;
    int _columns = 0;
    bool _focused = false;
};
