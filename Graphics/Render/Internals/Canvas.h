#pragma once
#include <raylib.h>
#include <optional>
#include "Utilities/Vec2.h"

// Sabit mantıksal çözünürlüklü çizim hedefi (RAII: RenderTexture2D). Pencereye büyütülürken doku pencerenin piksel
// boyutunda açılır, çizim koordinatları yine mantıksal boyuttadır: yazı ve şekiller ekran çözünürlüğünde
// rasterize edilir, sunum birebir kopyadır. Küçültülürken doku mantıksal boyutta kalır.
// Window'dan sonra kurulmalı: GL context gerektirir.
class Canvas {
public:
    Canvas(int width, int height, TextureFilter filter = TEXTURE_FILTER_BILINEAR);
    ~Canvas();

    // Dokuyu pencerenin çizim alanına göre (boyut değiştiyse yeniden) açar; frame başında, Begin'den önce.
    void Fit(int windowWidth, int windowHeight);
    // Büyütmede dokunun yüksekliğine üst sınır (0: sınırsız, pencere çözünürlüğü); mantıksal boyutun altına inmez.
    // Sonraki Fit'te uygulanır.
    void SetMaxHeight(int height);
    // Begin/End arasındaki tüm çizimler canvas'a yapılır.
    void Begin() const;
    void End() const;
    // Borrowed until the next Fit or destruction; consume only after End.
    [[nodiscard]] const Texture2D& Texture() const { return _target.texture; }

    // Canvas'ı oranını koruyarak pencereye ortalı basar (letterbox).
    void Present(int windowWidth, int windowHeight) const;
    [[nodiscard]] std::optional<Vec2> ToCanvas(Vec2 windowPoint, int windowWidth, int windowHeight) const;

private:
    [[nodiscard]] Rectangle Destination(int windowWidth, int windowHeight) const;
    int _width;
    int _height;
    int _maxHeight = 0;
    TextureFilter _filter;
    RenderTexture2D _target;

public:
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    Canvas(Canvas&&) = delete;
    Canvas& operator=(Canvas&&) = delete;
};
