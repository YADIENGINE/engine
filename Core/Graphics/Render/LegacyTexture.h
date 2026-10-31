#pragma once
#include <raylib.h>

class Painter;
class Surface;

// Geçiş: bir Surface'in görüntüsü raylib dokusu olarak, henüz taşınmamış çizim kodu için (Painter::Legacy
// kapsamında, surface'e çizimden sonra). Boyutu mantıksaldır (kaynak dikdörtgenleri canvas birimiyle verilir) ve
// RenderLayer gibi ters durur: mantıksal (x, y) noktası (x / genişlik, 1 - y / yükseklik) UV'sidir. Görüntü yoksa
// id 0. Göç bitince silinir.
[[nodiscard]] Texture2D LegacyTexture(const Painter& paint, const Surface& surface);
