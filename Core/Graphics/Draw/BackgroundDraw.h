#pragma once
#include <raylib.h>
#include "Graphics/Draw/BackgroundDraw.hpp"
#include "Utilities/Rect.h"

// Eski yol (raylib): stiller Graphics/Draw/BackgroundDraw.hpp'deki ortak tiplerdir; Painter karşılıkları da oradadır.
// Bu başlık göç bitince silinir.
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
