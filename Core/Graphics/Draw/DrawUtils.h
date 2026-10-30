#pragma once
#include <raylib.h>
#include <span>
#include "Utilities/Color4.h"
#include "Utilities/Rect.h"

// View'ların ortak kullandığı küçük çizim yardımcıları (canvas koordinatlarında).
[[nodiscard]] constexpr Color ToColor(Color4 color) { return {color.r, color.g, color.b, color.a}; }
void FillRect(const Rect& rect, Color color);
// Canvas koordinatında scissor (raylib'in BeginScissorMode'u piksel ister; canvas pencere çözünürlüğünde çizilir).
// Kamerayı yok sayar; EndScissorMode ile kapanır.
void BeginClip(int x, int y, int width, int height);
[[nodiscard]] Vector2 RectCenter(const Rect& rect);
[[nodiscard]] Vector2 RectCenter(Rectangle rect);
[[nodiscard]] Rectangle ToRectangle(const Rect& rect);
// Her kenardan 'amount' içeri (eksi: dışarı); boyutlar sıfırın altına inmez.
[[nodiscard]] Rectangle Inset(Rectangle rect, float amount);
// Sekme kenarlığı: üst ve iki yan; alt kenar açık kalır (sekme panelle birleşir).
void DrawTabOutline(Rectangle tab, float thickness, Color color);
// Panelin dört kenarı; üst kenar etkin sekmenin altında açık kalır.
void DrawTabbedPanelOutline(Rectangle panel, Rectangle activeTab, float thickness, Color color);
// Rengin kendi saydamlığını 'alpha' (0..1'e kırpılır) ile çarpar.
[[nodiscard]] Color Alpha(Color color, float alpha);
// a'dan b'ye 't' oranında doğrusal geçiş (raymath'in Vector2Lerp'i gibi, raymath'siz).
[[nodiscard]] Vector2 Mix(Vector2 a, Vector2 b, float t);
// Merkezi 'center' olan dolu beş köşeli yıldız.
void DrawStar(Vector2 center, float outerRadius, Color color);
// Altı üstünden geniş yumurta; 'height' toplam yükseklik. Sol üstte 'shine' renginde parlama.
void DrawEgg(Vector2 center, float height, Color color, Color shine);
// Merkezden dışa sönen yumuşak ışık (additive blend). Parıltı kapalıysa (EffectQuality) çizilmez.
// Araya başka çizim girmeyen çoklu parıltı için Graphics/Draw/GlowBatch.h kullanılır.
void DrawGlow(Vector2 center, float radius, Color color);
// Üçgen ve dörtgen (a-b-c-d sırayla, a-c köşegeninden iki üçgen); sarım yönü fark etmez (raylib sadece saat yönünün
// tersini çizer).
void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color);
void FillQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color);
// Köşeleri merkezin üstünde, altında ve iki yanında duran eşkenar dörtgen ('halfWidth' / 'halfHeight' yarı ölçüler).
void DrawDiamond(Vector2 center, float halfWidth, float halfHeight, Color color);
void DrawDiamondLines(Vector2 center, float halfWidth, float halfHeight, float thickness, Color color);
// 'count' öğelik, 'center' etrafında 'gap' aralıkla yatay dizilmiş sıranın 'index'. öğesinin merkezi (pip, nokta,
// rozet dizileri).
[[nodiscard]] Vector2 RowSlot(Vector2 center, int count, int index, float gap);
// Zikzak yıldırım: 'from'dan 'to'ya kırılarak iner; şekli 'seed' ile değişir. Uçta 'glowRadius' parıltı.
struct LightningStyle {
    int segments = 8;
    float jitter = 0.0f; // Ortada en büyük yana sapma
    float width = 1.0f;  // 'color' renkli dış çizgi
    float coreWidth = 0.0f;
    Color core = WHITE; // İç çizgi
    float glowRadius = 0.0f;
    float glowAlpha = 0.0f;
};
void DrawLightning(Vector2 from, Vector2 to, int seed, Color color, float alpha, const LightningStyle& style);
// Dışbükey çokgen (köşeler sırayla); sarım yönü fark etmez, aynalanmış (ters dönmüş) çokgen de çizilir.
void FillPolygon(std::span<const Vector2> points, Color color);
// Dokulu dışbükey çokgen: her köşenin dokudaki UV'si 'uvs'te (aynı sırada). Sarım yönü fark etmez.
void DrawTexturedPolygon(const Texture2D& texture, std::span<const Vector2> points, std::span<const Vector2> uvs,
                         Color tint);
