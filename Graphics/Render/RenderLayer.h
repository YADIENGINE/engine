#pragma once
#include <raylib.h>

// Ekran dışı çizim katmanı (RAII: RenderTexture2D); çizilen görüntü sonra doku olarak kullanılır (ayna, kırılan ekran).
// Window'dan sonra kurulmalı: GL context gerektirir. Begin/End canvas'ın (ya da başka bir katmanın) içinde açılabilir;
// End önceki hedefe döner. Kamera (BeginMode2D) ve scissor Begin/End'in içinde açılıp kapanmalı.
class RenderLayer {
public:
    RenderLayer(int width, int height, TextureFilter filter = TEXTURE_FILTER_BILINEAR);
    ~RenderLayer();

    void Begin() const;
    void End() const;
    // Begin/End arasında, kamerasız: renklere dokunmadan alfayı 1 yapar. Yarı saydam çizimler katmanın alfasını
    // düşürür; katman sonra çizilirken altındaki görünmesin.
    void MakeOpaque() const;
    // RenderTexture OpenGL'de ters durur: katman noktası (x, y) dokunun (x / genişlik, 1 - y / yükseklik) UV'sidir.
    [[nodiscard]] const Texture2D& Texture() const { return _target.texture; }

private:
    RenderTexture2D _target;

public:
    RenderLayer(const RenderLayer&) = delete;
    RenderLayer& operator=(const RenderLayer&) = delete;
    RenderLayer(RenderLayer&&) = delete;
    RenderLayer& operator=(RenderLayer&&) = delete;
};
