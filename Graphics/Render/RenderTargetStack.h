#pragma once
#include <raylib.h>

// Canvas ve RenderLayer'ın ortak çizim hedefi yığını: bir hedef başka birinin içinde açılabilir, kapanınca bir
// öncekine dönülür (raylib'in BeginTextureMode'u iç içe açılmaz). Hedef, dokusundan farklı bir mantıksal boyutla
// açılabilir: çizim koordinatları mantıksal boyuttadır, projeksiyon onları dokunun piksellerine ölçekler.
// Push/Pop sadece Graphics/Render içinde kullanılır.
void PushRenderTarget(const RenderTexture2D& target, int width, int height);
// En üstteki hedefi kapatır; altta hedef varsa ona, yoksa pencereye döner.
void PopRenderTarget();

// Açık hedefte bir çizim biriminin kaç piksel boyunda olduğu (kamera ve rlgl dönüşümleri dahil); hedef yoksa 1.
[[nodiscard]] float PixelScale();
// Noktayı hedefin piksel ızgarasına oturtur; döndürülmüş ya da aynalanmış dönüşümde dokunmaz.
[[nodiscard]] Vector2 AlignToPixel(Vector2 point);
// Dikdörtgenin kenarlarını piksel ızgarasına oturtur (en az bir piksel); döndürülmüş dönüşümde dokunmaz.
[[nodiscard]] Rectangle AlignToPixels(Rectangle rect);
// Çizim koordinatındaki dikdörtgenle scissor açar (kamerayı yok sayar, raylib'in BeginScissorMode'u gibi); kenarlar
// dışa doğru piksele yuvarlanır.
void BeginTargetScissor(Rectangle rect);
