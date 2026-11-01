#pragma once
#include <filesystem>
#include "Terminal/TerminalSession.h"
#include "Terminal/TerminalView.h"

// Alt dock'taki terminal: proje kökünde düz bir kabuk. Kabuk panel ilk çizildiğinde başlar, editör kapanınca
// kapanır; panel gizliyken de çalışmaya devam eder.
class ConsolePanel {
public:
    ConsolePanel();
    void SetDirectory(const std::filesystem::path& directory) { _directory = directory; }
    // Kabuğa eklenen ortam (ör. editörün kontrol pipe'ı); kabuk başlamadan verilir.
    void SetEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _environment = std::move(environment);
    }
    // Her kare, panel görünmese de: çıktıyı işler, çıkışı fark eder.
    void Update() { _session.Pump(); }
    void Draw();
    // Son çizimde klavye terminaldeydi.
    [[nodiscard]] bool Focused() const { return _view.Focused(); }

private:
    TerminalSession _session;
    TerminalView _view;
    std::filesystem::path _directory;
    std::vector<std::pair<std::string, std::string>> _environment;
};
