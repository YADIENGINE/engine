#pragma once
#include <imgui.h>
#include <utility>
#include <vector>

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
    void HandleInput(TerminalSession& session) const;
    [[nodiscard]] bool Reserved(ImGuiKeyChord chord) const;

    std::vector<ImGuiKeyChord> _reserved;
    bool _focused = false;
};
