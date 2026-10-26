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
