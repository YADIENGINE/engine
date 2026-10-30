#pragma once
#include "Graphics/Render/WindowSettings.h"
#include "Utilities/Vec2.h"

// İşletim sistemi penceresi ve OpenGL context'inin sahibidir (RAII).
// GL kaynağı kullanan her nesneden önce kurulmalı, sonra yok edilmelidir.
class Window {
public:
    explicit Window(const WindowSettings& settings);
    ~Window();

    // Gizli açılan pencereyi gösterir (ilk kare çizildikten sonra); görünürse bir şey yapmaz.
    void Show();
    [[nodiscard]] bool ShouldClose() const;
    [[nodiscard]] int Width() const;
    [[nodiscard]] int Height() const;
    void SetTargetFps(int fps);
    void SetBorderlessFullscreen(bool enabled);
    // Kare değişimini monitör yenilemesine bağlar; FPS sınırıyla birlikte çalışır.
    void SetVSync(bool enabled);
    // Pencere modunda boyutu ayarlar ve monitörde ortalar; {0, 0} pencereyi büyütür. Tam ekranda bir şey yapmaz.
    void Resize(Vec2 size);
    [[nodiscard]] Vec2 MonitorSize() const;

private:
    bool _wasMaximized = false;
    bool _visible;

public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
};
