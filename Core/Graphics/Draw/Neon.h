#pragma once
#include <raylib.h>
#include "Graphics/Draw/Neon.hpp"
#include "Utilities/Rect.h"

class TextRenderer;

// Eski yol (raylib): stiller Graphics/Draw/Neon.hpp'deki ortak tiplerdir; Painter karşılıkları da oradadır. Bu
// başlık göç bitince silinir.
// Helpers expect alpha blending on entry and return to it. Clipping belongs to the caller.
void FillNotched(Rectangle rect, float notch, Color color);
void DrawNeonFrame(Rectangle rect, float notch, float thickness, Color color, float glow, const NeonFrameStyle& style);
void DrawNeonText(const TextRenderer& text, const char* label, int centerX, int y, int fontSize, Color color,
                  int maxWidth, float glow, const NeonTextStyle& style);
void DrawNeonPanel(const Rect& rect, Color accent, const NeonPanelStyle& style);
void DrawNeonChip(const TextRenderer& text, const char* label, Vector2 center, int fontSize, Color color,
                  const NeonChipStyle& style);
[[nodiscard]] Rectangle DrawArcadeButtonBody(const Rect& rect, const ButtonLook& look, float timeSeconds,
                                             const ArcadeButtonStyle& style);
void DrawArcadeButton(const Rect& rect, const char* label, int fontSize, const ButtonLook& look,
                      const TextRenderer& text, float timeSeconds, const ArcadeButtonStyle& style);
void DrawRoundButton(const Rect& rect, const char* label, int fontSize, Color accent, const TextRenderer& text,
                     const RoundButtonStyle& style);
// İçte 'width' kalınlığında çember, çevresinde 'glow' saydamlığında (Fade) eklemeli hale; parıltı kapalıysa hale
// çizilmez.
void DrawNeonRing(Vector2 center, float radius, float width, Color color, float glow, int segments);
void DrawProgressBar(Rectangle bar, const ProgressBarLook& look, const ProgressBarStyle& style);
