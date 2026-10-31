#pragma once
#include "Fx/FxStyle.h"
#include "Graphics/Anim/ButtonAnim.h"
#include "Graphics/Draw/BackgroundDraw.hpp"
#include "Graphics/Draw/Neon.hpp"
#include "Graphics/Style/ThemePalette.h"

// Arayüz widget'larının stilleri (Ui/UiDraw, Ui/UiButton bunları okur).
struct UiStyle {
    NeonFrameStyle frame;
    NeonTextStyle text;
    NeonPanelStyle panel;
    NeonChipStyle chip;
    ArcadeButtonStyle button;
    RoundButtonStyle roundButton;
    KeyCapStyle keyCap;
    ProgressBarStyle progressBar;
    ButtonAnimSettings buttonAnimation;
};

// Ekran arka planlarının rolleri (Ui/Backdrop bunları okur): menü ekranları, oyun alanının çevresi, önizleme /
// galeri ekranları; tarama çizgilerinin geometrisi hepsinde ortak.
struct BackdropTheme {
    BackdropStyle menu;
    BackdropStyle play;
    BackdropStyle gallery;
    ScanlineStyle scanlines;
};

// Oyunun görsel kimliği. Game GameSettings::theme ile seçer (varsayılan NeonArcadeTheme); engine sahneler
// kurulmadan önce etkinleştirir. Temalı çizim (DrawUi*) her çağrıda etkin temayı okur; tek seferlik bir varyant
// için stil kopyalanıp açık stil alan çizim fonksiyonuna (DrawNeon*, DrawArcadeButton) verilir. Fx temayı okumaz:
// efekt sahnesi (FxStage) ve ayrı efekt sistemleri stillerini 'fx'ten açıkça alır.
struct Theme {
    ThemePalette palette;
    UiStyle ui;
    FxStyle fx;
    BackdropTheme backdrop;
};

[[nodiscard]] const Theme& CurrentTheme();
void SetTheme(const Theme& theme);
