#pragma once
#include <raylib.h>
#include "Graphics/Draw/DrawTime.h"
#include "Graphics/Draw/Neon.h"
#include "Utilities/Rect.h"

class TextRenderer;

// Temalı widget'lar: stil etkin temadan (CurrentTheme().ui), animasyon zamanı sunum saatinden (DrawTime) gelir; kendi
// zamanını oynatan çizim (ör. önizleme kartı) 'time' verir. Tek seferlik bir varyant için stil kopyalanıp açık stil
// alan fonksiyona (DrawNeon*, DrawArcadeButton) verilir.
void DrawUiFrame(Rectangle rect, float notch, float thickness, Color color, float glow = 1.0f);
void DrawUiText(const TextRenderer& text, const char* label, int centerX, int y, int fontSize, Color color,
                int maxWidth = 0, float glow = 1.0f);
void DrawUiPanel(const Rect& rect, Color accent);
void DrawUiChip(const TextRenderer& text, const char* label, Vector2 center, int fontSize, Color color);
// Butonun gövdesi (etiketsiz); etiketin ya da ikonun çizileceği, animasyonlu gövde dikdörtgenini döndürür.
[[nodiscard]] Rectangle DrawUiButtonBody(const Rect& rect, const ButtonLook& look, float time = DrawTime());
void DrawUiButton(const Rect& rect, const char* label, int fontSize, const ButtonLook& look, const TextRenderer& text,
                  float time = DrawTime());
void DrawUiRoundButton(const Rect& rect, const char* label, int fontSize, Color accent, const TextRenderer& text);
// Tuş simgesi (Q / E gibi).
void DrawUiKeyCap(const Rect& area, const char* key, int fontSize, const TextRenderer& text);
void DrawUiProgressBar(Rectangle bar, const ProgressBarLook& look);
