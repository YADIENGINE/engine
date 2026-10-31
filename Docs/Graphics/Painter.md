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
`Camera`, `*`, `Inverse`), `Color4` (straight alpha; `Rgb(0xRRGGBB)`, `WithAlpha`, `ScaleAlpha`, `Mix`).

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

`Game::Font()` oyunun fontudur (`GameSettings::font`); `FontFace::FromFile` başka fontlar yükler. Glifler ekranda
kapladıkları piksel boyunda, ilk kullanımda rasterize edilir: her boy keskindir, fontun bütün karakterleri çizilebilir
(fontta olmayan karakter `?` olur).

```cpp
paint.Text(font, "Ready", {800.0f, 120.0f}, {.size = 48.0f, .align = TextAlign::Center, .glow = {10.0f, cyan}});
paint.TextBox(font, description, area, {.size = 18.0f, .wrap = TextWrap::Word});
const TextBlock block = font.Layout(longText, style); // Bir kez yerleşip her kare çizmek için
```

Konum ilk satırın tepesidir; yatayda `align`'a göre satırın sol ucu, ortası ya da sağ ucu. `TextStyle`: boy, renk,
hizalama, `maxWidth` + `TextWrap::Word` / `TextFit::Shrink`, satır ve harf aralığı, `glow`, `outline`, `shadow`,
`emission`. `font.Measure(text, size)` sarmasız ölçüdür. Aynı metin her kare yeniden ölçülmez (yerleşim önbelleği).

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
