#pragma once
#include <raylib.h>
#include <array>
#include "Graphics/Anim/ButtonAnim.h"
#include "Utilities/Rect.h"

class TextRenderer;

struct NeonFrameStyle {
    float outerWidth = 1.0f;
    float innerWidth = 1.0f;
    float outerAlpha = 0.0f;
    float innerAlpha = 0.0f;
    float coreWidth = 0.0f;
    float coreMinWidth = 0.0f;
    float coreWhiten = 0.0f;
};
// Yumuşak hale halkası: yazının 'samples' kopyası 'radius' uzaklıkta eşit aralıkla dizilir; 'phase' açıyı örnek
// adımı cinsinden kaydırır (halkalar üst üste binmesin diye).
struct NeonTextRing {
    float radius = 0.0f;
    float alpha = 0.0f;
    float phase = 0.0f;
};
// İki hale biçimi birlikte ya da ayrı kullanılabilir: keskin (glowAlpha > 0: dört yöne glowOffset kaydırılmış kopya)
// ve yumuşak (samples > 0: rings halkaları).
struct NeonTextStyle {
    int glowOffset = 0;
    float glowAlpha = 0.0f;
    std::array<NeonTextRing, 2> rings{};
    int samples = 0;
    float coreWhiten = 0.0f;
};
struct NeonPanelStyle {
    Color body = BLANK;
    Color shine = BLANK;
    float notch = 0.0f;
    float frameWidth = 0.0f;
    float glow = 0.0f;
    float shineOffset = 0.0f;
    float shineHeight = 0.0f;
    NeonFrameStyle frame;
};
struct NeonChipStyle {
    Color text = WHITE;
    int paddingX = 0;
    int paddingY = 0;
    float notch = 0.0f;
    float glowRadiusScale = 0.0f;
    float glowAlpha = 0.0f;
};
struct ArcadeButtonStyle {
    Color body = BLANK;
    Color shadow = BLANK;
    Color frameColor = WHITE;
    Color label = WHITE;
    Color shine = BLANK;
    Color pressColor = WHITE;
    float depth = 0.0f;
    float notch = 0.0f;
    float grow = 0.0f;
    float squash = 0.0f;
    float frameWidth = 0.0f;
    float appearDrop = 0.0f;
    float appearAlphaScale = 1.0f;
    float shakeAmount = 0.0f;
    float shakeSpeed = 0.0f;
    float pulseSpeed = 0.0f;
    float pulseAmount = 0.0f;
    float pressFlash = 0.0f;
    float darkAccent = 0.0f;
    float shadowAccent = 0.0f;
    float labelWhiten = 0.0f;
    float shineOffset = 0.0f;
    float shineHeight = 0.0f;
    float shineHighlight = 0.0f;
    int padding = 0;
    NeonFrameStyle frame;
    NeonTextStyle text;
};
struct RoundButtonStyle {
    Color body = BLANK;
    Color shadow = BLANK;
    float shadowOffset = 0.0f;
    float frameWidth = 0.0f;
    float glowInnerWidth = 0.0f;
    float glowOuterWidth = 0.0f;
    float glowAlpha = 0.0f;
    float labelWhiten = 0.0f;
    float textGlow = 0.0f;
    int segments = 32;
    int padding = 0;
    NeonTextStyle text;
};
// Arcade ilerleme çubuğu: koyu yuva, öncü şerit, parlayan dolum, üst parlaklık, blok aralıkları, neon çerçeve. Sıfır
// genişlik / saydamlık o katmanı kapatır.
struct ProgressBarStyle {
    Color track = BLACK;
    Color ghost = BLANK;       // Gelen ama dolumun henüz yetişmediği ilerleme
    float previewAlpha = 0.0f; // Önizleme dilimi dolum renginin bu saydamlığında
    Color shine = BLANK;       // Dolumun üst şeridi
    float shineHeight = 0.0f;  // Yüksekliğin oranı
    float glowAlpha = 0.0f;    // Dolumun eklemeli parıltısı
    float glowSpread = 0.0f;
    float segment = 0.0f; // Blok genişliği
    float segmentGap = 0.0f;
    Color flash = BLANK; // ProgressBarLook::flash 1 iken üstü kaplayan renk
    Color frame = BLANK;
    float frameOutset = 0.0f;
    float frameWidth = 0.0f;
    float frameGlow = 0.0f;
    NeonFrameStyle frameStyle;
};
struct ProgressBarLook {
    float fill = 0.0f;  // 0..1
    float ghost = 0.0f; // 0..1; fill'i aşan kısmı öncü şerit olarak görünür
    // 0..1; fill'i ve ghost'u aşan kısmı soluk önizleme olarak görünür (henüz gelmemiş bir kazancın götüreceği yer)
    float preview = 0.0f;
    Color color = WHITE;
    float flash = 0.0f; // 0..1
    // 0..1; dolumun üstüne soldan, kendi renginde çizilen ikinci dolum (ör. dolu barın üstünde akan süre)
    float overlay = 0.0f;
    Color overlayColor = BLANK;
    float alpha = 1.0f;
};
struct ButtonLook {
    ButtonAnimState animation;
    float appear = 1.0f;
    Vector2 offset{};
    Color accent = WHITE;
};

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
