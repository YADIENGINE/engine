#pragma once
#include <memory>
#include <optional>
#include "Utilities/Vec2.h"

struct DeviceInput;
// Tek birincil temasın (dokunma ya da sol fare tuşu) bir frame'deki durumu.
struct DragPointerResult {
    std::optional<Vec2> pressed;  // Temas bu frame'de başladı (başlangıç noktası)
    std::optional<Vec2> position; // Temas sürüyor: şu anki konumu
    std::optional<Vec2> origin;   // Temas sürüyor ya da bu frame'de bitti: başladığı nokta
    std::optional<Vec2> released; // Temas bu frame'de bitti: son konumu
    std::optional<Vec2> hover;    // Basılı temas yokken farenin hover'ı
    bool dragging = false;        // Temas eşikten fazla kaydı (bittiği frame'de de true)
    bool tap = false;             // Temas eşiği aşmadan bitti
    bool cancelled = false;       // Temas iptal edildi; activation/drop ve yerel sürükleme görünüşü temizlenir.
};

// Sürükle-bırak: tek birincil temasın başlangıcını, konumunu ve bitişini izler. Dokunuşun ürettiği sahte fare
// olaylarını yok sayar; diğer parmaklar birincil teması bozmaz. Canvas dışına çıkan temas iptal edilir.
class DragPointer {
public:
    explicit DragPointer(int threshold);
    ~DragPointer();
    [[nodiscard]] DragPointerResult Update(const DeviceInput& input);
    // Süren teması iptal eder; bırakılışı (ve sahte fare olayları) yok sayılır.
    void Cancel();

private:
    struct State;
    std::unique_ptr<State> _state;

public:
    DragPointer(const DragPointer&) = delete;
    DragPointer& operator=(const DragPointer&) = delete;
};
