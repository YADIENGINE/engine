# Render sistemi (tasarım)

Durum: uygulandı (2026-10-31; çalışma zamanı doğrulaması bekliyor). Kullanım: [Painter](../Graphics/Painter.md). Karar kaydı: [README → D17](README.md#d17-render-sistemi-kendi-gpu-katmanı-üstünde-yeni-2d-çizim-sistemi).
Uygulandıkça kullanım belgesi `Docs/Graphics/`'e taşınır; burada sadece kararlar ve gerekçeleri kalır.

## Neden

Bugünkü çizim raylib'in anlık API'si üstünde ince bir canvas sarmalayıcısıdır; asıl çizimi Game raylib'i doğrudan
çağırarak yapar. Engine çizim durumunu (blend, clip, kamera, hedef), batch'lemeyi, kenar yumuşatmayı ve shader'ı
sahiplenmediği için ne kalite ne performans tek yerden iyileştirilebilir. İnceleme bulguları özetle:

| Alan | Bugün | Sonuç |
| --- | --- | --- |
| Durum | `EndBlendMode` her zaman alpha'ya döner, `EndScissorMode` clip'i kapatır, hedef değişimi kamerayı sıfırlar | Kurallar belgede, kodda değil; iç içe kapsamlar kırılgan |
| Batch | Her blend / scissor / kamera / hedef değişimi raylib batch'ini boşaltır; 266 `DrawGlow` noktası ikişer kez | `GlowBatch` gibi yerel yamalar |
| Kenar | Render texture'lar MSAA'sız; şekiller tırtıklı | Kalite sadece çözünürlükle gizleniyor |
| Konum | Parıltı, yumurta, metin konumu ve yazı boyu `int` | Hareket ve ölçek animasyonu basamaklı |
| Parıltı | Additive üst üste çizgi / 17 kopya yazı / 36 dilimli yelpaze | Köşelerde parlak noktalar, pahalı |
| Metin | `GetGlyphIndex` doğrusal arama, ölçüm her çizimde tekrar, atlas kare ortasında | CPU maliyeti, takılma |
| Katman | `RenderLayer` mantıksal boyutta, canvas pencere çözünürlüğünde | Pause / sinematikte bulanıklaşma |
| Shader | Yok | Bloom, post-process, SDF yok |
| Ölçüm | Sadece parıltı istekleri | Gerçek draw call bilinmiyor |

## Hedefler

1. **Kalite:** her şey float koordinatta, analitik kenar yumuşatmalı; parıltı ve bloom gerçek.
2. **Performans:** çizim durumu engine'de; batch'i sadece doku, hedef ve clip değişimi böler; kare başına tek yükleme,
   GPU'yu beklemeyen buffer'lar; kare içinde bellek tahsisi yok (ısındıktan sonra).
3. **API:** az sayıda, tahmin edilebilir kavram; raylib'siz; yanlış kullanım derlemede ya da kayıt anında yakalanır.
4. **Bakım:** arka uç tek bir arayüzün arkasında; çizim kaydının tamamı GPU'suz test edilir; global durum yok.
5. **Geçiş:** eski raylib çizimi aynı karede çalışmaya devam eder; Snake View View taşınır, sonra eski yol silinir.

## Kararlar

| # | Karar | Gerekçe |
| --- | --- | --- |
| R1 | Arka uç doğrudan OpenGL 3.3 core, Engine'e özel `GpuDevice` arayüzünün arkasında | Köşe biçimi, yükleme ve durum bizde; başka arka uç = yeni `GpuDevice` |
| R2 | Public API raylib'siz (`Float2`, `FloatRect`, `Color4`, `Affine2`) | View header'ları raylib bilmez, kayıt GPU'suz test edilir |
| R3 | Premultiplied alpha her yerde; additive = alfası 0 renk | Alpha ve additive aynı batch'te; parıltı batch bölmez |
| R4 | Şekiller SDF ile (dörtgen + fragment shader), çokgen/yol CPU'da 1 piksel AA saçağıyla | Çözünürlükten bağımsız yumuşak kenar; MSAA yok |
| R5 | Anlık kullanım, kare boyu kayıt: çağrılar DrawList'e yazılır, kare sonunda tek seferde gönderilir | Tek yükleme, birleştirme, istatistik, GPU'suz test |
| R6 | Tek "uber shader", şekil türü köşeden okunur; çok dokulu yuvalar (8) | Daire, çizgi, parıltı, metin ve resim aynı draw call'da |
| R7 | Durum değişimi RAII kapsamlarla, yığın halinde; dengesizlik kayıt anında hata | `End*` sorunu biter; iç içe her kapsam öncekine döner |
| R8 | Glifler ekrandaki piksel boyunda dinamik atlasa; hale için ayrıca SDF glif | Her boy keskin, tüm atlası birden kurma yok, hale tek çizim |
| R9 | Bloom emission kanalından (yarım çözünürlük), parlaklık eşiğinden değil | Neyin parlayacağını sanat seçer; UI yazısı istemeden parlamaz |
| R10 | Blending gamma (sRGB) uzayında, bugünkü gibi | Renk seçimleri ve görünüş korunur; doğrusal uzay bilinçli olarak yok |
| R11 | Global durum yok: saat, kalite ve hedef bilgisi Painter'ın bağlamında | Birden fazla Painter (editör, offscreen) mümkün; test edilebilir |
| R12 | Geçiş köprüsü: taşınmamış nesne `Draw()` yeni sistemde otomatik köprü kapsamında çalışır | Snake hiç değişmeden çalışır; köprü son View taşınınca silinir |

Bilerek yapılmayanlar (v1 dışı, yeri hazır): MSAA, doğrusal renk uzayı / HDR canvas, compute, çok iş parçacıklı kayıt,
oyunun kendi shader'ları, kendini kesen yol dolgusu (stencil), karmaşık metin şekillendirme (HarfBuzz), flipbook.

## Katmanlar

```
View / Engine yardımcıları
   │  Painter (public): şekil, metin, resim + kapsamlar (Transform, Clip, Opacity, Blend, Target, Group)
   ▼
DrawList (Engine-private, headless): köşe/indeks akışı + komut akışı; birleştirme; kapsam dengesi
   │  Tessellator (çokgen/yol/AA saçağı), GlyphCache (atlas paketleme, rasterize), SurfacePool
   ▼
FrameExecutor (Engine-private): DrawList → GpuDevice komutları; post-process zinciri; sunum
   ▼
GpuDevice (Engine-private arayüz): GlDevice (GL 3.3 core) | NullDevice (testler, komut kaydı)
```

Headless kısım (Painter, DrawList, Tessellator, metin yerleşimi, atlas paketleme, glif rasterize) `EngineHeadless`'a
girer: raylib ve GL include etmez, Engine testlerinde `NullDevice` ile denetlenir. `GlDevice`, pencere ve sunum
raylib'li `Engine` hedefindedir.

### Klasörler

```
Core/Utilities/        Float2 (+ işlemler), FloatRect, Affine2, Color4 (+ Mix, WithAlpha, Rgb) — headless matematik
Core/Graphics/
  Paint/               Painter.hpp (facade), ShapeStyle, LineStyle, Gradient, Path, PaintScopes
    Internals/         DrawList, Batcher, Tessellator, Vertex, StateStack
  Text/                FontFace.hpp, TextStyle, TextBlock (yerleşim sonucu)
    Internals/         GlyphCache, AtlasPacker, GlyphRasterizer (stb_truetype), Shaper
  Surface/             Surface.hpp (offscreen çizim hedefi), StaticTexture, PixelBuffer, TextureRef
  Post/                PostSettings (bloom, vinyet, tarama, renk kayması), RenderQuality
  Render/              (bugünkü) Window/Canvas/Renderer yeni sisteme geçer; RenderStats.hpp public
    Internals/         Renderer, FrameExecutor, SurfacePool, LegacyBridge
  Gpu/Internals/       GpuDevice, GpuTypes, GlDevice, NullDevice, Shaders/*.glsl (derlemede gömülür)
```

Yeni public header'lar `.hpp` (ApiBoundaries: yeni facade'lar). Eski `Draw/`, `RenderLayer`, `RenderTargetStack`,
`GlowBatch`, `RenderExecutionOptions`, `TextRenderer` geçiş bitince silinir.

**Adlar:** raylib ile çakışan ad kullanılmaz (`Font`, `Image`, `Texture`, `Color`, `Rectangle`, `Camera2D`,
`Shader`, `Transform`, `BlendMode`, `Matrix`, `TextFormat` ...). Yeni sistem bir adı eski Engine tipinden alacaksa
(`TextStyle`, `TextLayout`, `TextAlignment`) eski tip geçiş boyunca `Legacy` önekiyle yeniden adlandırılır; kalıcı
adlar temiz kalır.

## Public API

### Temel tipler (headless)

```cpp
struct FloatRect { float x, y, width, height; /* FromCenter, Center, Inset, Contains, Right, Bottom ... */ };
struct Affine2 {   // 2x3; Translate, Rotate(radians, pivot), Scale(s, pivot), operator*, Apply, Inverse
    static Affine2 Camera(Float2 target, Float2 screenOffset, float zoom, float rotation);
};
// Color4: straight (normal) alpha, 0..255. Premultiply iç ayrıntıdır.
constexpr Color4 Rgb(std::uint32_t rgb, std::uint8_t alpha = 255);
Color4 WithAlpha(Color4 color, float alpha);   // rengin kendi alfasıyla çarpar
Color4 Mix(Color4 a, Color4 b, float t);
```

Açılar her yerde radyan.

### Painter

Her çizim bir `Painter&` alır. Painter kopyalanamaz; karenin ya da bir hedefin süresince yaşar.

```cpp
void EatableView::Draw(Painter& paint, const Eatable& food) const {
    // Şekiller: bir geometri + bir stil. Stil alanları birlikte kullanılabilir.
    paint.Circle(pos, 10.0f, {.fill = red, .stroke = {2.0f, white}, .glow = {24.0f, red}});
    paint.RoundRect(box, 6.0f, {.fill = panel, .stroke = {2.0f, accent}});
    paint.ChamferRect(box, 8.0f, Fill(panel));              // köşesi kesik (neon panel)
    paint.Ellipse(pos, {12.0f, 16.0f}, Fill(color));
    paint.Arc(center, 40.0f, start, sweep, {.stroke = {4.0f, accent, StrokeCap::Round}});
    paint.Line(a, b, {.width = 3.0f, .color = c, .cap = StrokeCap::Round});
    paint.Polyline(points, {.width = 4.0f, .color = c, .join = StrokeJoin::Round});
    paint.Polygon(points, Fill(c));                          // basit (kendini kesmeyen) çokgen
    paint.Glow(pos, 36.0f, red, 0.6f);                       // yumuşak radyal ışık, additive

    Path star;                                               // yol: MoveTo/LineTo/QuadTo/CubicTo/ArcTo/Close
    paint.FillPath(star, Fill(gold));
    paint.StrokePath(star, {.width = 2.0f, .color = white});

    // Doku: Surface, StaticTexture ya da pişirilmiş parça (D10)
    paint.Image(surface, dest, {.rotation = angle, .tint = tint, .source = region});
    paint.NineSlice(panelTexture, insets, dest);

    // Metin
    paint.Text(font, "SCORE", at, {.size = 24.0f, .color = white, .align = TextAlign::Center});
    paint.TextBox(font, description, area, {.size = 18.0f, .color = muted, .wrap = true});
}
```

Stil:

```cpp
struct ShapeStyle {
    Color4 fill{0, 0, 0, 0};       // alfa 0: dolgu yok
    Gradient gradient{};           // varsa fill yerine: Linear(from, to, a, b) / Radial(center, radius, a, b)
    StrokeStyle stroke{};          // width 0: çerçeve yok; align: Center / Inside / Outside
    GlowStyle glow{};              // radius 0: hale yok; şeklin SDF'inden dışa sönen ışık, tek dörtgende
    float emission = 0.0f;         // 0..1: bloom'a giden pay (R9)
};
ShapeStyle Fill(Color4 color);     // kısa yol; stil alanları designated initializer ile de verilir
```

Kurallar:
- Geometri parametre, görünüş stil. Aynı stil her şekle aynı anlamla uygulanır (daire, dikdörtgen, yay, çizgi).
- Hale (`glow`) ve `Glow` kalite ayarını kendisi okur (Off: çizilmez, Reduced: yarıçap ve yoğunluk azalır).
- Sıfır ya da negatif boyut, NaN koordinat: çizilmez (sessiz); geçersiz stil (negatif kalınlık): `invalid_argument`.

### Kapsamlar

Durum her zaman kapsamla değişir; kapsam nesnesi `[[nodiscard]]`, kapanınca önceki duruma döner.

```cpp
{
    auto moved = paint.Transform(Affine2::Rotate(angle, center));  // mevcut dönüşümle çarpılır
    auto clip  = paint.Clip(area);        // mevcut clip'le kesişir (dönüşümden geçer; dönmüş dönüşümde hata)
    auto faded = paint.Opacity(0.5f);     // her çizimin alfası çarpılır (ucuz; üst üste binen yerler koyulaşır)
    auto add   = paint.Blend(Blend::Add); // Normal | Add | Multiply | Screen
    auto group = paint.Group({.opacity = 0.5f});  // önce geçici yüzeye çizer, sonra tek parça birleştirir
    auto into  = paint.Target(fieldSurface);      // hedef: yüzey; içeride dönüşüm/clip/opaklık sıfırdan
    auto mark  = paint.Marker("HUD");             // RenderDoc / Profiler bölge adı
}
```

- Kapsamlar açıldığı sıranın tersinde kapanmazsa DrawList kayıt anında `logic_error` atar.
- `Opacity` ucuzdur ama üst üste binen çizimler koyulaşır; tek parça solma için `Group` (geçici yüzey havuzu).
- `Blend::Normal` ve `Blend::Add` aynı batch'tedir (R3); `Multiply` / `Screen` batch'i böler.

### Bilgi

```cpp
paint.Time();          // sunum saati (editör Pause'u dondurur; bugünkü DrawTime'ın yerine)
paint.Delta();
paint.Quality();       // RenderQuality: glow, bloom, arka plan efektleri
paint.PixelScale();    // mevcut dönüşümde bir birimin kaç cihaz pikseli olduğu (LOD kararları)
paint.Bounds();        // hedefin mantıksal boyutu
paint.Post();          // karenin post-process ayarları (canvas'a uygulanır; her kare oyunun varsayılanına döner)
```

### Metin

```cpp
FontFace& font = fonts.Main();                     // Game ayarlarından yüklenir; TTF + kod noktası listesi
TextMetrics m = font.Measure("SCORE", 24.0f);      // float genişlik, yükseklik, ascent
TextBlock block = font.Layout(text, {.size = 18.0f, .maxWidth = 320.0f, .wrap = true, .fit = TextFit::Shrink});
paint.Text(font, block, at, {.color = white, .glow = {6.0f, cyan}, .shadow = {{0.0f, 2.0f}, black}});
```

- Konum ve boy float. Kerning fontun `kern` tablosundan. Satır kırma, sığdırma (`Shrink`), hizalama
  (Left/Center/Right) yerleşimde.
- `TextBlock` bir kez kurulup her kare çizilebilir; `paint.Text(font, string, ...)` de sınırlı bir yerleşim
  önbelleğinden geçer, aynı metin her kare yeniden ölçülmez.
- Hale ve gölge tek çizimdir: çekirdek bitmap glif, hale aynı glifin SDF'inden (R8).

### Yüzeyler ve dokular

```cpp
Surface field({1600.0f, 900.0f});                  // mantıksal boyut; piksel boyutu canvas ölçeğini izler
if (field.Stale()) { auto into = paint.Target(field); ... }  // ölçek değişince içerik silinir, sahibi yeniden çizer
StaticTexture noise = StaticTexture::FromPixels(PixelBuffer::Generate(...));  // koddan üretilen doku (D1)
```

- Görseller koddan üretilir (D1): dosyadan resim yükleme yok; doku kaynakları yüzeyler, koddan üretilen piksel
  tamponları, glif atlasları ve pişirme önbelleği (D10).
- Dokular yalnız Window kurulduktan sonra yaratılır; RAII, kopyalanamaz, taşınabilir. `TextureRef` sahip olmayan
  tutamaçtır; tutamaçlar nesil sayılıdır, silinmiş dokuya erişim hata olarak yakalanır.

### Oyun yaşam döngüsü

```cpp
class GameObject { virtual void Draw(Painter& paint) const; };   // yeni
// Varsayılan uygulama: { auto legacy = paint.Legacy(); Draw(); }  — taşınmamış nesne hiç değişmeden çalışır
```

`Scene::Render`, `Game::Draw` ve `DrawLoading` aynı şekilde `Painter&` alır. Bir nesnenin içinde kısmi geçiş için
`auto legacy = paint.Legacy();` açıkça yazılır.

## İç yapı

### Köşe biçimi

Tek köşe biçimi, hedef ~36–40 bayt (kesin düzen uygulamada ölçülür):

| Alan | Tip | Anlam |
| --- | --- | --- |
| position | 2 × f32 | Hedefin mantıksal koordinatı; dönüşüm CPU'da uygulanmış |
| local | 2 × f32 | Şeklin kendi koordinatı (SDF) ya da UV (doku, glif) |
| color | 4 × u8 | Premultiplied renk |
| shape | 4 × f16 | Türe göre: yarı boyut, köşe yarıçapı, çerçeve kalınlığı, hale yarıçapı ... |
| extra | 2 × f16 | Türe göre: gradyan / yay açıları |
| kind | u8 | Solid, Coverage (AA saçağı), Texture, Glyph, GlyphSdf, Circle, Ellipse, RoundRect, ChamferRect, Capsule, Arc, Glow ... |
| slot | u8 | Doku yuvası (0–7) |
| emission | u8 | Bloom payı |
| flags | u8 | Çerçeve hizası, gradyan türü |

Dönüşüm CPU'da uygulandığı için farklı dönüşümlü çizimler aynı batch'e girer. SDF kenar yumuşatma ekran türevinden
(`fwidth`) hesaplanır; dönmüş ve ölçeklenmiş şekiller de bir cihaz pikseli genişliğinde yumuşar.

### Batch kuralı

Ardışık çizimler şu dördü aynıysa tek draw call olur: hedef, clip dikdörtgeni, pipeline (blend sınıfı), doku yuvaları
(beyaz doku ve glif atlası her zaman bağlı; batch'te en fazla 8 farklı doku). Çizim sırası hiçbir zaman değişmez.
İndeksler 32 bit; dörtgenler indeksli.

### Yükleme

Kare sonunda bütün köşe ve indeksler tek seferde, karelere bölünmüş bir halka buffer'a yazılır
(`glMapBufferRange` + `UNSYNCHRONIZED`, kare başına `glFenceSync`; GPU'nun henüz okuduğu bölgeye yazılmaz). Buffer
yetmezse büyütülür, küçültülmez. DrawList dizileri kareler arasında yeniden kullanılır.

### GpuDevice

Engine-private arayüz; oyun görmez.

- Kaynaklar: buffer, doku (R8, RGBA8, RGBA16F; filtre; mip), render target (1–2 renk eki), program (gömülü shader
  tablosundan), pipeline durumu (program + blend + yazma maskesi).
- Komutlar: `BeginPass(target, clear)`, `SetPipeline`, `SetScissor`, `BindTextures`, `SetUniforms`, `DrawIndexed`,
  `EndPass`, `Upload`, `ReadbackAsync` (PBO), `Present`, GPU zaman sorguları, hata ayıklama grupları.
- Tutamaçlar indeks + nesil. `GlDevice` GL durumunun tek sahibidir ve önbellekler; `NullDevice` komutları kaydeder.
- GL fonksiyonları raylib'in yüklediği `glad` işaretçilerinden (`external/glad.h` sadece header olarak). Debug
  derlemede `KHR_debug` varsa açılır, nesnelere ad verilir.
- Shader'lar `Gpu/Internals/Shaders/*.glsl`; CMake derlemede C++ dizisine gömer (çalışma zamanında dosya yok).

### Çözünürlük

Mantıksal canvas modeli aynen kalır: koordinatlar mantıksal, raster pencere çözünürlüğünde (`renderHeight` sınırıyla).
Yüzeyler aynı piksel ölçeğinde açılır; ölçek değişince yeniden açılır ve `Stale()` olur. Pencere boyutu değişirken
gereksiz yeniden açmayı önlemek için yüzey havuzu boyutları kademelendirir.

### Metin

- **Glif önbelleği:** (font, kod noktası, piksel boyu) → atlas bölgesi. Atlas sayfaları R8, 1024², skyline paketleme;
  sayfa dolunca yeni sayfa, bütçe aşılınca en uzun süredir kullanılmayan sayfa boşaltılır.
- **Boy kademeleri:** küçük boylar tam piksel boyunda; büyük boylar ~%6'lık kademelerde rasterize edilip ölçeklenir.
  Ölçek animasyonu her kare yeni glif üretmez.
- **Hale:** her glifin bir kez, sabit boyda (ör. 48 px, 8 px yayılım) SDF'i üretilir; hale ve dış çizgi her boyda
  bundan çizilir.
- **Ön ısıtma:** `FontFace::Warm(sizes, text)` açılışta sık boyları hazırlar; kare ortasında rasterize ölçülür
  (`RenderStats::glyphsRasterized`).
- Rasterize `stb_truetype` (raylib içinde statik derlenir; kendi kopyamız `Text/Internals`'ta derlenir, sembol
  çakışması yok). Lisans bildirimi `ThirdParty/Notices` üzerinden denetlenir.

### Post-process

Canvas'ın iki eki vardır: renk (RGBA8) ve emission (RGBA8; bloom kapalıyken eklenmez). Zincir:

```
canvas renk + emission ─► bloom (emission'dan, yarım çözünürlük, aşağı/yukarı örnekleme zinciri)
                       ─► son birleştirme: renk + bloom, vinyet, tarama çizgisi, renk kayması, ton
                       ─► pencereye sunum (letterbox)
```

Etkiler `PostSettings` alanlarıdır (oyunun varsayılanı + `paint.Post()` ile karelik değişiklik). Kalite: Bloom
Off / Half / Full. Oyunun kendi post shader'ı v1 dışı; zincir yeni aşama eklemeye göre kurulur.

### Geçiş köprüsü (R12)

Yeni sistemin batch'i rlgl'ninkinden ayrıdır. `paint.Legacy()` kapsamı:
1. Açılırken DrawList'in o ana kadarki kaydını gönderir, sonra canvas'ı (ya da etkin yüzeyi) rlgl'ye hedef olarak
   bağlar: framebuffer, viewport, mantıksal projeksiyon, alpha blend, yüz kırpma açık (rlgl'nin beklediği durum).
2. Kapanırken rlgl batch'ini boşaltır (`rlDrawRenderBatchActive`) ve GL durumunu `GlDevice`'ın önbelleğiyle
   eşitler.

Eski kod straight alpha ile çizer; canvas opak olduğu için görünür sonuç aynıdır. Editör ImGui'si (rlImGui) sunumdan
sonra pencere aşamasında aynı köprüyle çizer. Snake'in son View'ı taşınınca köprü, `Draw()` varsayılanı ve eski
`Draw/` silinir; raylib sadece pencere, input ve ses için kalır.

### Ölçüm ve hata ayıklama

`RenderStats` (public, kare başına): draw call, batch, köşe, indeks, yüklenen bayt, hedef / clip / doku / pipeline
değişimi, köprü geçişi, rasterize edilen glif, atlas doluluğu, yüzey havuzu, pass başına GPU süresi (çift tamponlu
zaman sorgusu). Profiler bunları `FrameObserver` üzerinden alır; bugünkü `RenderCounter::Glows`/`GlowBlendRequests`
kalkar. `paint.Marker` RenderDoc'ta grup olur. Editör önizlemesi canvas'ı beklemesiz okur (PBO, bir kare gecikmeli).

### Testler

Engine testleri (headless, `NullDevice`):
- Kapsamlar: iç içe dönüşüm / clip kesişimi / opaklık çarpımı; dengesiz kapanış hata.
- Batch: alpha ve additive karışık çizimler tek draw call; doku, clip, hedef değişimi böler; 9. doku böler; sıra korunur.
- Tessellator: çokgen sarımı, kendini kesmeyen içbükey çokgen, birleşim türleri, AA saçağı piksel ölçeğine göre.
- Metin: ölçüm float ve pencere boyundan bağımsız; satır kırma; atlas paketleme ve LRU; boy kademeleri.
- Yüzey: ölçek değişince `Stale()`; havuz yeniden kullanımı; silinmiş tutamaç hata.
- Halka buffer: sarma ve fence'ler (NullDevice fence'i simüle eder).
Görsel doğrulama: Engine'in editör önizlemesi **Render Gallery** (bütün şekiller, stiller, metin, kapsamlar, post;
Snake'e bağımlılık yok) ve Profiler'da yeni senaryolar (çok şekil, çok metin, çok parıltı).

## Uygulama sırası

Her adım derlenir ve tek başına çalışır; Snake'in görünüşü değişmez (hepsi köprüden geçer).

1. **Temel:** headless matematik tipleri, `GpuDevice` + `NullDevice`, DrawList + batch + kapsamlar, testler.
2. **GL ve kare:** `GlDevice`, gömülü shader'lar, yeni Renderer (canvas, sunum), köprü, `Draw(Painter&)` yaşam döngüsü.
   Kabul: Snake ve editör aynen çalışır.
3. **Şekiller:** SDF türleri, çizgi / polyline / çokgen / yol, hale, gradyan.
4. **Metin:** `FontFace`, glif önbelleği, yerleşim, hale / gölge.
5. **Yüzey ve post:** `Surface`, `Group`, havuz, bloom ve son birleştirme, kalite ayarları.
6. **Ölçüm ve araçlar:** `RenderStats`, Profiler, PBO okuma, Render Gallery, `Docs/Graphics/` yeni API'ye göre.

Sonraki oturumlar: Engine yardımcılarının (Neon, Ui, Icons, Backdrop, Fx) yeni API'deki karşılıkları; Snake'in View
View göçü; köprünün ve eski yolun silinmesi; D10 pişirme önbelleği (pişirilmiş parçalar atlas sayfalarına paketlenir,
batch'i bölmez).

## Uygulamada netleşenler

- **Köşe biçimi 52 bayt, hepsi f32** (konum, yerel koordinat, 4 şekil + 2 stil değeri, iki renk, tür / yuva /
  emission / bayraklar). f16, büyük dikdörtgenlerde kenar hassasiyetini bozduğu için kullanılmadı. Doğrusal gradyan
  köşe başına `t` olarak taşınır (üçgen üstünde tam doğrusal), radyal gradyan şeklin merkezindendir.
- **Yay dörtgeni yalnız kendi dilimini kaplar** (çeyrek yay çemberin karesinin yarısından azını boyar); mesafe
  fonksiyonunun orijini şeklin merkezinde kalır, dörtgen `ShapeGeometry::offset` kadar kayar.
- **Şekil başına katmanlar:** hale, dolgu ve çerçeve ayrı dörtgenlerdir (aynı batch). Shader mesafeyi türe göre,
  kaplamayı moda göre (Fill / Stroke / Halo) hesaplar; `fwidth` dallanmanın dışındadır.
- **Akış buffer'ı:** fence yerine "ekle + taşınca orphan": yalnız son orphan'dan beri hiç yazılmamış baytlara
  `UNSYNCHRONIZED` yazılır, GPU'yu beklemek gerekmez. İndeksler her yüklemede segmentin ilk köşesine göre
  yeniden tabanlanır (negatif base vertex yok).
- **Kare boru hattı GPU'dan bağımsızdır** (`RenderPipeline`: canvas boyutu, post, sunum, istatistik) ve
  `NullDevice` ile test edilir; raylib'e bağlı kalan sadece pencere, `GlDevice` ve köprüdür.
- **Pencere aşaması da Painter'dır:** canvas görüntüsü arka tampona Painter ile letterbox'lanır; `DrawWindow`
  (editör ImGui'si) aynı Painter'ın köprüsünden çizer.
- **Bloom katmanı tembel:** canvas emission ekini yalnız varsayılan ayar ya da bir önceki kare bloom isterken tutar
  (`Painter::Post` ile açılan bloom bir kare sonra başlar); bloom yoksa MRT maliyeti yoktur.
- **Glif önbelleği:** R8 sayfalar (1024², skyline, 2 px boşluk), 48 px'e kadar tam piksel boyu, sonra ~%6 kademe,
  192 px üstü ölçeklenir. Uzaklık glifleri 32 px'te 16 px alanla üretilir: hale yazı boyunun yarısına kadar uzanır.
  Bütçe dolunca bu karede kullanılmamış en eski sayfa geri alınır.
- **Yerleşim önbelleği:** `Painter::Text(string)` aynı metni (font + boy + sarma ayarları) 120 kare boyunca tutar.
- **Kapsam dengesi:** `PaintScope` taşınamaz, bu yüzden kullanıcı kodu kapsamları sırasız kapatamaz; kayıt yine de
  denetler ve kare sonunda `logic_error` atar.
- **Köprü tembeldir:** Painter ile raylib arasında gerçek bir geçiş olmadıkça boşaltma yapılmaz; taşınmamış ardışık
  nesneler tek geçişte çizilir. Headless bağlamda (testler) `Legacy()` boş bir kapsamdır.
- **Eski `TextStyle`** `LegacyTextStyle` oldu; yeni `TextStyle` Painter'ındır.
- **Çözünürlük:** eski `Canvas` mantığı (`renderHeight` sınırı, küçültmede mantıksal boyut) `RenderPipeline`'a
  taşındı; arka tampon yüksek DPI'da piksel boyutundadır, input dönüşümü ekran boyutundadır.

## Göç (R13)

Eski yardımcıların (Draw, Neon, Ui, Backdrop, Icons, Fx) Painter karşılıkları ve oyunların göçü. Kararlar:

- **Yukarıdan aşağı.** Painter sahneden View'a parametreyle akar; eski bir fonksiyon Painter'ı göremez. Bir View
  bütünüyle taşınır; yarım kalan kısım `paint.Legacy()` kapsamında kalır. Ters köprü (eski koddan Painter'a) yok:
  her küçük çağrı bir boşaltma olurdu (yüzlerce parıltı = yüzlerce draw call).
- **Aynı ad, `Painter&` ilk parametre.** `DrawUiPanel(paint, rect, accent)` eskisinin (`DrawUiPanel(rect, accent)`)
  overload'udur; View taşınırken çağrı adı değişmez. Painter sürümleri raylib'siz `.hpp` başlıklarında ve
  `EngineHeadless`'tadır (NullDevice ile test edilir); eski `.h` başlıkları ve raylib uygulamaları göç bitince silinir.
- **Stil tipleri ortak ve raylib'siz** (`Color4`, `Float2`): tek tema iki yolu da besler, Painter'lı ve eski View
  aynı ayarla çizer. Yalnız eski yolun öykünmesi olan alanlar (yazı halesinin kaydırılmış kopyaları) "eski yol"
  diye işaretlidir; Painter karşılığı yerel haledir (`TextStyle::glow`). Göç bitince silinir. İstisna: oyunlar ikon
  stilini çoğunlukla hesaplanmış raylib renkleriyle kurar; eski `DrawIcon` bu yüzden raylib renkli
  `LegacyIconStyle` alır (paylaşılan `IconStyle` `Color4`'tür).
- **Geçici örtük dönüşümler, hepsi tek yönlü** (raylib'siz şablonlarla, köprüyle birlikte silinir): `Color4` →
  raylib `Color` (yeni sabitler, ör. oyunun `Color4` paleti, eski çizime gider); raylib `Vector2` → `Float2` ve
  `Rectangle` → `FloatRect` (taşınan kod eski hesapların değerlerini Painter'a verir). Ters yönler açıktır
  (`ToColor4`, `ToVector2`), böylece `koşul ? Color4 : Color` ve karışık vektör aritmetiği belirsizleşmez.
  `Rect` → `FloatRect` örtük dönüşümü kalıcıdır (kayıpsız genişletme).
- **Fx eski yolsuz taşındı:** efekt sınıfları ikili yol tutmaz, doğrudan Painter'lıdır. Onları çizen sahne kökleri
  (oyun alanı, mağaza, açılış, tanıtım, ekran geçişi) `Draw(Painter&)`'e geçer; içlerindeki taşınmamış View'lar
  `paint.Legacy()` kapsamında kalır. Sahnenin `RenderLayer` katmanları `Surface` olur; katmanı raylib dokusu olarak
  okuyan eski kod `LegacyTexture(paint, surface)` kullanır (geçici, `Painter::LegacyTextureName`).
- **Varsayılan font:** `paint.Font()` oyunun fontudur (`GameSettings::font`); `paint.Text(text, ...)` ve
  `paint.TextBox(text, ...)` onu kullanır. Widget'lar font parametresi almaz; başka font `paint.Text(font, ...)`.
  View'ların `const TextRenderer&` taşıması biter.
- **Zaman ve kalite Painter'dan:** Painter'lı yardımcılar sunum saatini `paint.Time()`'dan (`DrawTime` karşılığı),
  parıltı / arka plan kalitesini `paint.Quality()`'den okur. Parçacık adedi, sarsıntı ve flaş `EffectQuality`'de
  kalır (Fx simülasyonu; çizim değil).

Eski → yeni karşılıklar: `RenderLayer` → `Surface` + `paint.Target`; `UiRegion` → `paint.Transform`;
`BeginClip` / `BeginUiClip` → `paint.Clip`; `DrawMergedShadow` → `paint.Group({.opacity})`; `GlowBatch` / `DrawGlow`
→ `paint.Glow` (batch kendiliğinden); `FillNotched` → `ChamferRect`; `Camera2D` → `Affine2::Camera`;
`TextRenderer` → `FontFace` (`paint.Font()`), `TextLayout` → `TextBlock`.

Sıra: (1) dönüşümler + varsayılan font; (2) Draw şekilleri, Neon, Ui, Backdrop; (3) Icons; (4) Fx; (5) oyun:
sahne kökleri, sonra View View; (6) eski yolun, köprünün ve geçici dönüşümün silinmesi.

## Sonraya kalanlar

- GPU zaman sorguları (`RenderStats::gpuMilliseconds` şimdilik 0) ve editör önizlemesinin PBO ile beklemesiz okunması.
- Clip'in shader'da yapılması (batch bölmez) ve alt piksel metin konumu, ölçüme göre.
- `NullDevice`'ın kaynak tablolarının `HandlePool`'a taşınması (şimdi iki ayrı uygulama).
