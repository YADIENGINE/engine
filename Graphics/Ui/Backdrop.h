#pragma once
#include "Graphics/Draw/BackgroundDraw.h"
#include "Utilities/Rect.h"

// Temanın arka plan rolleri (CurrentTheme().backdrop). Menu: menü ekranları; Play: oyun alanının çevresi; Gallery:
// önizleme ve galeri ekranları.
enum class BackdropRole { Menu, Play, Gallery };

// Grid, vinyet ve tarama çizgisi ayrı çağrılır; katman sırasını View belirler. Temaya ait olmayan bir arka plan
// (ör. oyunun harita stili) BackdropStyle ile çizilir; grid ve vinyet için Graphics/Draw/BackgroundDraw.h'ın
// fonksiyonları doğrudan kullanılır.
[[nodiscard]] const BackdropStyle& ThemeBackdrop(BackdropRole role);
// 'time': çağıranın saniye cinsinden zamanı; çizici animasyon durumu tutmaz.
void DrawBackdropGrid(BackdropRole role, const Rect& area, float time);
// Vinyet tanımlanmayan rollerde çizim yapmaz.
void DrawBackdropVignette(BackdropRole role, const Rect& area);
void DrawBackdropScanlines(BackdropRole role, const Rect& area);
// Temanın tarama çizgileri, rolsüz koyulukla ('alpha'); sahne üstü katmanlar (kart ekranı gibi) için.
void DrawScanlineOverlay(const Rect& area, float alpha);
