#pragma once
#include <filesystem>
#include <string>
#include "Editor/Application/EditorPresentation.h"

// Projenin tuning dosyaları (Docs/Tuning.md): dosya seçimi, önizlemenin canlı durumu ve değerler. Sayılar kutuya
// yazılır, Enter'la girer; Enter'sız yazılan kutudan çıkınca geri alınır. Sınır dışı ya da geçersiz değer gönderilmez,
// kutu kırmızı kalır. Değer diske yamanır; çalışan önizleme dosyayı kendisi okur.
class TuningPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    std::filesystem::path _file; // Seçili dosya; liste değişse de korunur
    std::string _rejected;       // Son reddedilen kutunun kimliği ve nedeni
    std::string _rejectedText;
};
