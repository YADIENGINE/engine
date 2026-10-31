#pragma once
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Ui/Backdrop.hpp"
#include "Utilities/Rect.h"

// Eski yol (raylib); Painter karşılıkları Graphics/Ui/Backdrop.hpp'de (BackdropRole ve ThemeBackdrop da orada). Bu
// başlık göç bitince silinir.
// 'time': çağıranın saniye cinsinden zamanı; çizici animasyon durumu tutmaz.
void DrawBackdropGrid(BackdropRole role, const Rect& area, float time);
// Vinyet tanımlanmayan rollerde çizim yapmaz.
void DrawBackdropVignette(BackdropRole role, const Rect& area);
void DrawBackdropScanlines(BackdropRole role, const Rect& area);
// Temanın tarama çizgileri, rolsüz koyulukla ('alpha'); sahne üstü katmanlar (kart ekranı gibi) için.
void DrawScanlineOverlay(const Rect& area, float alpha);
