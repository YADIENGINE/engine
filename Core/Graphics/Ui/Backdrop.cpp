#include "Graphics/Ui/Backdrop.h"
#include "Graphics/Style/Theme.h"

const BackdropStyle& ThemeBackdrop(BackdropRole role) {
    const BackdropTheme& backdrop = CurrentTheme().backdrop;
    switch (role) {
    case BackdropRole::Menu   : return backdrop.menu;
    case BackdropRole::Play   : return backdrop.play;
    case BackdropRole::Gallery: return backdrop.gallery;
    }
    return backdrop.menu;
}
void DrawBackdropGrid(Painter& paint, BackdropRole role, const FloatRect& area) {
    DrawBackdropGrid(paint, ThemeBackdrop(role), area);
}
void DrawBackdropVignette(Painter& paint, BackdropRole role, const FloatRect& area) {
    DrawBackdropVignette(paint, ThemeBackdrop(role), area);
}
void DrawBackdropScanlines(Painter& paint, BackdropRole role, const FloatRect& area) {
    DrawBackdropScanlines(paint, ThemeBackdrop(role), CurrentTheme().backdrop.scanlines, area);
}
void DrawScanlineOverlay(Painter& paint, const FloatRect& area, float alpha) {
    DrawScanlines(paint, area, CurrentTheme().backdrop.scanlines, alpha);
}
