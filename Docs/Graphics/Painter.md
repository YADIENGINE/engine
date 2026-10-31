# Painter

Engine'in çizim API'si. Raylib'siz, GPU'dan bağımsız kaydedilir; kare sonunda Engine'in OpenGL 3.3 cihazına
gönderilir. Tasarım ve kararlar: [Render sistemi](../Design/Rendering.md). Eski raylib tabanlı yardımcılar
(`Graphics/Draw`, `Neon`, `TextRenderer`, `RenderLayer` ...) geçiş bitene kadar [köprüden](#geçiş-köprüsü) çalışır.

## Nereden gelir

Her `GameObject` çizimini `Draw(Painter& paint) const` içinde yapar; `Scene::Render`, `Game::Draw(Painter&)` ve
`Game::DrawWindow(Painter&)` de aynı Painter'ı geçirir. Painter kopyalanmaz, saklanmaz; sadece çağrı süresince
kullanılır.

```cpp
#include "Graphics/Paint/Painter.hpp"

void ScoreView::Draw(Painter& paint) const {
    paint.RoundRect(_panel, 8.0f, {.fill = Rgb(0x141826), .stroke = {2.0f, Rgb(0x30e0ff)}});
    paint.Text(_font, "SCORE", _panel.Center(), {.size = 24.0f, .align = TextAlign::Center});
}
```

Koordinatlar mantıksal canvas birimidir (y aşağı), açılar radyan ve ekranda saat yönündedir. Her şey float konumda,
kenarı yumuşatılmış çizilir; çizim kare sonuna kadar gönderilmez ama çağrı sırası her zaman korunur.

## Temel tipler

`Utilities/` altında, headless: `Float2` (işlemleri, `Length`, `Normalized`, `Rotated`, `Float2::FromAngle` ...),
`FloatRect` (`FromCenter`, `Center`, `Inset`, `Intersect`, `Union` ...), `Affine2` (`Translate`, `Rotate`, `Scale`,
`Camera`, `*`, `Inverse`), `Color4` (straight alpha; `Rgb(0xRRGGBB)`, `Hsv(hue, s, v)`, `WithAlpha`, `ScaleAlpha`,
`Mix`).

## Şekiller

| Çağrı | Not |
| --- | --- |
| `Rect`, `RoundRect(rect, radius)`, `ChamferRect(rect, chamfer)` | Köşesi kesik dikdörtgen: neon paneller |
| `Circle(center, radius)`, `Ellipse(center, radii)` | |
| `Arc(center, radius, start, sweep, LineStyle)` | `Butt` / `Round` uç; tam tur halka çizer |
| `Line(from, to, LineStyle)` | `Butt` / `Round` / `Square` uç |
| `Polyline(points, LineStyle, closed)` | `Miter` (sınırı aşınca `Bevel`) / `Bevel` / `Round` birleşim |
| `Polygon(points, ShapeStyle)`, `Triangle` | Kendini kesmeyen çokgen (iki sarım yönü de olur) |
| `FillPath(path, ShapeStyle)`, `StrokePath(path, LineStyle)` | `Path`: `MoveTo`, `LineTo`, `QuadTo`, `CubicTo`, `ArcTo`, `Close` |
| `Glow(center, radius, LightStyle)` | Yumuşak, eklemeli ışık |

`ShapeStyle` parçaları birlikte çizilir, sırayla: hale (`glow`), dolgu (`fill` ya da `gradient`), çerçeve (`stroke`,
`Inside` / `Center` / `Outside`). Kısa yollar: `Fill(color)`, `Outline(width, color)`. `Gradient::Linear(start, end,
from, to)` şeklin merkezine göre yereldir; `Gradient::Radial(radius, from, to)` merkezden; `Gradient::Vertical(height,
top, bottom)`. Çokgen ve path dolgusu gradyan yerine `gradient.from`'u kullanır.

Bir pikselden ince çizgiler incelmez, soluklaşır. Sıfır boyut ya da NaN çizilmez; negatif kalınlık
`invalid_argument` atar.

## Resimler ve yüzeyler

`Surface` ekran dışı çizim hedefidir: boyutu mantıksaldır, çözünürlüğü canvas'ı izler. İçine `paint.Target(surface)`
ile çizilir, `paint.Image(surface, dest, style)` ile çizilir. Ölçek değişince içerik kaybolur; pahalı ve durağan
çizimleri bir kez yapmak için:

```cpp
if (paint.Stale(_background)) {
    auto into = paint.Target(_background);
    DrawMapFloor(paint);
}
paint.Image(_background, field);
```

`ImageStyle`: kaynak bölge, merkez etrafında dönüş, renk tonu. `NineSlice(texture, dest, style)` kenarları sabit
kalan panel çizer. Surface, oyunun renderer'ından uzun yaşamaz (sahibi sahne / View olur).

## Metin

Font verilmeyen metin oyunun fontuyla çizilir: `paint.Font()` (`GameSettings::font`, aynı nesne `Game::Font()`);
`FontFace::FromFile` başka fontlar yükler, onlar `paint.Text(font, ...)` ile çizilir. Glifler ekranda kapladıkları
piksel boyunda, ilk kullanımda rasterize edilir: her boy keskindir, fontun bütün karakterleri çizilebilir (fontta
olmayan karakter `?` olur).

```cpp
paint.Text("Ready", {800.0f, 120.0f}, {.size = 48.0f, .align = TextAlign::Center, .glow = {10.0f, cyan}});
paint.TextBox(description, area, {.size = 18.0f, .wrap = TextWrap::Word});
const TextBlock block = paint.Font().Layout(longText, style); // Bir kez yerleşip her kare çizmek için
```

Konum ilk satırın tepesidir; yatayda `align`'a göre satırın sol ucu, ortası ya da sağ ucu. `TextStyle`: boy, renk,
hizalama, `maxWidth` + `TextWrap::Word` / `TextFit::Shrink`, satır aralığı (`lineSpacing` fontun satır yüksekliğinin
katı ya da `lineHeight` birim olarak), harf aralığı, `glow`, `outline`, `shadow`, `emission`. Sık etiketler için
kısaltmalar: `CenteredText(size, color, maxWidth)`, `FittedText(size, color, maxWidth, align)`. `Text` / `TextBox`
çizilen bloğun boyutunu döndürür; alt alta paragraflar onunla ilerler. `TextFit::Shrink` ile küçülen yazı istenen boydaki satırın dikey ortasında kalır. `font.Measure(text, size)`
sarmasız ölçüdür; `font.FittedSize(text, size, maxWidth)` Shrink'in vereceği boydur (hit-test ve yan yana yazılar için).
Aynı metin her kare yeniden ölçülmez (yerleşim önbelleği). UTF-8 karakterlerini tek tek gezmek: `Utilities/Utf8.h`
(`NextCodepoint`).

## Kapsamlar

Durum değişikliği her zaman kapsamdır; kapsam nesnesi yok olunca önceki durum döner:

```cpp
{
    auto moved = paint.Transform(Affine2::Rotate(angle, center)); // Mevcut dönüşümle çarpılır
    auto clip = paint.Clip(area);       // Mevcut clip'le kesişir; dönmüş dönüşümde invalid_argument
    auto faded = paint.Opacity(0.5f);   // Ucuz; üst üste binen yerler koyulaşır
    auto light = paint.Blending(Blend::Add);
    auto group = paint.Group({.opacity = 0.5f}); // Önce geçici resme çizer, tek parça birleştirir
    auto into = paint.Target(surface);  // İçeride dönüşüm / clip / opaklık sıfırdan
    auto mark = paint.Marker("HUD");    // RenderDoc'ta bölge adı (literal)
}
```

`Blend::Normal` ve `Blend::Add` aynı draw call'u paylaşır (premultiplied alpha); `Multiply` / `Screen` batch'i böler.

## Post-process ve kalite

`GameSettings::post` oyunun varsayılanıdır; `paint.Post()` o karede değiştirir (sonraki kare varsayılana döner):
bloom (`intensity`, `radius`), `vignette`, `scanlines`, `aberration`. Bloom sadece `emission` > 0 olan çizimden gelir
(`ShapeStyle::emission`, `LineStyle::emission`, `TextStyle::emission` ...). Bloom'u bir kare ilk kez isterse
canvas bir sonraki kareden itibaren bloom katmanını tutar.

`paint.Quality()` oyuncunun seçimidir (`GlowQuality`, `BloomQuality`, `backgroundEffects`). Painter haleleri ve
bloom'u kendisi uygular; çağıran sadece tamamen süs olan işi atlamak için okur. `paint.Time()` / `paint.Delta()` sunum
saatidir (editör Pause'u dondurur); `paint.PixelScale()` bir birimin kaç cihaz pikseli olduğunu verir.

## Yardımcılar ve tema

Ortak şekiller ve tema widget'ları Painter'ın üstündedir; hepsi ilk parametre olarak `Painter&` alır, kaliteyi ve
saati Painter'dan okur, yazıyı oyunun fontuyla çizer:

| Başlık | İçerik |
| --- | --- |
| `Graphics/Draw/DrawUtils.hpp` | `DrawStar`, `DrawEgg`, `DrawDiamond`, `DrawLightning`, `DrawTabOutline`, `DrawTabbedPanelOutline`, `RowSlot` |
| `Graphics/Draw/Neon.hpp` | Açık stilli neon widget'lar ve stilleri: `DrawNeonFrame/Text/Panel/Chip/Ring`, `DrawArcadeButton(Body)`, `DrawRoundButton`, `DrawKeyCap`, `DrawProgressBar` |
| `Graphics/Ui/UiDraw.hpp` | Temalı widget'lar (`CurrentTheme().ui`): `DrawUiFrame`, `DrawUiText`, `DrawUiPanel`, `DrawUiChip`, `DrawUiButton(Body)`, `DrawUiRoundButton`, `DrawUiKeyCap`, `DrawUiProgressBar` |
| `Graphics/Draw/BackgroundDraw.hpp` | `DrawGrid2D`, `DrawScanlines`, `DrawVignette`, `BackdropStyle` ile `DrawBackdrop*` |
| `Graphics/Ui/Backdrop.hpp` | Temanın arka plan rolleri: `DrawBackdropGrid(paint, BackdropRole::Menu, area)` ... |
| `Graphics/Icons/Icons.hpp` | Ortak ikon seti: `DrawIcon(paint, Icon::Star, center, size, {.color = gold})` |
| `Fx/*` | Efektler (parçacık, uçan yazı, halka, şok dalgası, overlay, kamera, `FxStage`): [Effects](Effects.md) |

Kamera `Utilities/CameraView.h`'tır (`offset`, `target`, `rotation` radyan, `zoom`; `Transform()`, `ToScreen`,
`ToWorld`); dünya `auto view = paint.Transform(camera.Transform())` kapsamında çizilir.

```cpp
DrawUiPanel(paint, _area, accent);
DrawUiButton(paint, _play, "PLAY", 30.0f, _playButton.Look(accent)); // UiButton: View'ın değer üyesi
DrawUiText(paint, "SETTINGS", {_area.Center().x, 40.0f}, {.size = 36.0f, .color = text, .align = TextAlign::Center});
```

Tek seferlik varyant: `CurrentTheme().ui.X` kopyalanır, değiştirilir, açık stilli fonksiyona verilir. Kendi saatini
oynatan çizim (ör. önizleme kartı) de açık stilli fonksiyonu `time` ile çağırır.

### Eski yoldan taşırken

Aynı ada sahip eski (raylib) fonksiyonlar `.h` başlıklarındadır ve göç bitince silinir; Painter sürümleri aynı adın
`.hpp`'sindedir. Stil tipleri (`NeonPanelStyle`, `ButtonLook`, `BackdropStyle` ...) ve tema ikisinde ortaktır,
renkler `Color4`'tür. Göç süresince tek yönlü örtük dönüşümler vardır: `Color4` → raylib `Color` (yeni sabitler eski
çizime), raylib `Vector2` → `Float2` ve `Rectangle` → `FloatRect` (eski değerler Painter'a); karşı yönler
`ToColor4(color)` / `ToVector2(point)` ile açıktır. İkonlarda eski `DrawIcon` raylib renkli `LegacyIconStyle` alır,
Painter'lı `DrawIcon` `IconStyle` alır. Taşınmamış kod bir `Surface`'in görüntüsünü raylib dokusu olarak
`LegacyTexture(paint, surface)` ile alır (`Graphics/Render/LegacyTexture.h`, `Legacy()` kapsamında).

| Eski | Painter |
| --- | --- |
| `TextRenderer& text`, `text.DrawCentered(...)` | `paint.Text(label, position, {.align = TextAlign::Center ...})` |
| `DrawGlow`, `GlowBatch` | `paint.Glow` (batch kendiliğinden) |
| `FillNotched` | `paint.ChamferRect` |
| `FillTriangle`, `FillQuad`, `FillPolygon` | `paint.Triangle`, `paint.Polygon` |
| `BeginClip`, `BeginUiClip` | `auto clip = paint.Clip(rect)` |
| `UiRegion::Draw` | `auto region = paint.Transform(UiTransform(source, target).Affine())` |
| `RenderLayer` | `Surface` + `paint.Target` (const çizimdeki önbellek: `mutable Surface`) |
| `Camera2D`, `BeginMode2D` | `CameraView` + `paint.Transform(camera.Transform())` |
| `DrawMergedShadow` | `auto group = paint.Group({.opacity = darkness})` |
| `DrawTime()` | `paint.Time()` |

## Ölçüm

`Game::RenderStatistics()` son karenin maliyetini verir: draw call, batch, köşe, yüklenen bayt, durum değişimleri,
köprü geçişleri, rasterize edilen glif, yeniden ayrılan yüzey. Editörde **Engine > Render Gallery** bütün özellikleri
tek ekranda gösterir.

## Geçiş köprüsü

Painter'a taşınmamış kod raylib ile çizmeye devam eder: `GameObject::Draw()` (parametresiz) ve `Game::Draw()`
varsayılan olarak `paint.Legacy()` kapsamında çağrılır. Bir nesnenin içinde kısmi geçiş:

```cpp
void MenuView::Draw(Painter& paint) const {
    paint.RoundRect(_frame, 8.0f, Fill(panel)); // Yeni
    auto legacy = paint.Legacy();                // Bu kapsamda raylib (DrawUiButton, TextRenderer ...)
    DrawOldButtons();
}
```

Köprü raylib'e o anki hedefi, dönüşümü ve clip'i verir. Painter ile raylib arasındaki her geçiş bir batch boşaltması
demektir (`RenderStats::legacySwitches`); ardışık eski nesneler tek geçişte çizilir. Son eski çizim taşınınca köprü
ve eski yol silinir.
