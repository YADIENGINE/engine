// Eski yol (raylib); Painter karşılığı Backdrop.cpp. Göç bitince silinir.
#include "Graphics/Ui/Backdrop.h"
#include "Graphics/Style/Theme.h"

void DrawBackdropGrid(BackdropRole role, const Rect& area, float time) {
    DrawBackdropGrid(ThemeBackdrop(role), area, time);
}
void DrawBackdropVignette(BackdropRole role, const Rect& area) { DrawBackdropVignette(ThemeBackdrop(role), area); }
void DrawBackdropScanlines(BackdropRole role, const Rect& area) {
    DrawBackdropScanlines(ThemeBackdrop(role), CurrentTheme().backdrop.scanlines, area);
}
void DrawScanlineOverlay(const Rect& area, float alpha) {
    DrawScanlines(area, CurrentTheme().backdrop.scanlines, alpha);
}
