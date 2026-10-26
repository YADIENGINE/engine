#pragma once
#include <raylib.h>
#include "Utilities/Rect.h"

// Tarama çizgilerinin geometrisi; koyuluk çizim anında verilir.
struct ScanlineStyle {
    Color color = BLACK; // Opak renk; çizimde verilen koyulukla soldurulur
    int spacing = 4;
    int thickness = 1;
};

// Ekran arka planı: kayan grid, kenar karartması ve tarama çizgisi koyuluğu. Katmanlar ayrı çizilir; sırayı çağıran
// belirler.
struct BackdropStyle {
    int gridCell = 50;
    float gridScrollSpeed = 0.0f; // birim/s, aşağı doğru
    Color gridColor = BLANK;
    int vignetteSize = 0; // 0: vinyet yok
    Color vignetteColor = BLANK;
    float scanlineAlpha = 0.0f;
};

void DrawGrid2D(const Rect& bounds, int spacing, Vector2 offset, Color color, int thickness);
// Arka plan efektleri kapalıysa (EffectQuality) çizilmez.
void DrawScanlines(const Rect& bounds, Color color, int spacing, int thickness);
void DrawScanlines(const Rect& bounds, const ScanlineStyle& style, float alpha);
// Nonpositive size disables drawing; leaves clipping to caller.
void DrawVignette(const Rect& bounds, int size, Color color);

// 'time': çağıranın saniye cinsinden zamanı; arka plan efektleri kapalıyken grid durur.
void DrawBackdropGrid(const BackdropStyle& style, const Rect& area, float time);
void DrawBackdropVignette(const BackdropStyle& style, const Rect& area);
void DrawBackdropScanlines(const BackdropStyle& style, const ScanlineStyle& scanlines, const Rect& area);
