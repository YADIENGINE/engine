#pragma once
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
};

// Sürükle-bırak: tek birincil temasın başlangıcını, konumunu ve bitişini izler. Dokunuşun ürettiği sahte fare
// olaylarını yok sayar; diğer parmaklar birincil teması bozmaz.
class DragPointer {
public:
    explicit DragPointer(int threshold);
    [[nodiscard]] DragPointerResult Update(const DeviceInput& input);
    // Süren teması iptal eder; bırakılışı (ve sahte fare olayları) yok sayılır.
    void Reset();

private:
    void Begin(Vec2 point, DragPointerResult& result);
    void Track(Vec2 point, DragPointerResult& result);
    void Finish(DragPointerResult& result);
    int _threshold;
    std::optional<int> _touch;
    std::optional<Vec2> _origin;
    std::optional<Vec2> _last;
    bool _dragged = false;
    bool _mouseActive = false;
    bool _suppressMouse = false;
    bool _cancelled = false;
};
