# Assets and platforms

## Asset kökü

Masaüstünde runtime exe yanındaki `Assets/` klasörünü kullanır; çalışma dizinine güvenmeyin.
`engine_add_game(... ASSETS ".../Game/Assets")` bu klasörü build sonrasında kopyalar.
Android'de Gradle aynı asset ağacını APK'ye paketler; Engine manifestteki dosyaları
uygulamanın özel `Assets/` dizinine çıkarır.

`GameSettings::assets` kullanılan dosyaların göreli yol listesidir.
Audio katalog yolları otomatik eklenir; özel font yolunu açıkça listeleyin:

```cpp
GameSettings settings;
settings.font = "Fonts/MyFont.ttf";
settings.assets = {settings.font, "Images/player.png"};
settings.audio.effects = {{SoundId{0}, "Audio/click.wav"}};
```

Boş, mutlak veya `..` içeren manifest yolları reddedilir. Eksik dosya başlangıç hatasıdır.
`font` boşsa yerleşik varsayılan font kullanılır.
Engine manifest hazırlama, font ve ses yükleme sağlar; genel texture asset manager sağlamaz.
Dışarıdan gelen (indirilen, lisanslı) her asset'in bildirimi `Game/Distribution/`'a yazılır ve lisans dosyası
pakete girer: [Release](Release.md).
Texture'ı kullanan View kaynağı GL bağlamı yaşarken yükleyip bırakır.

## GameSettings referansı

| Alan | Varsayılan / anlam |
| --- | --- |
| `window` | 1280×720, başlık Game, 60 FPS; minimum 640×360, resize açık |
| `window.visible`, `alwaysRun` | true / false; gizli context ve minimize iken çalışma |
| `window.maximized` | false |
| `canvasWidth/Height` | 1280 / 720 mantıksal koordinat |
| `background`, `letterbox` | BLACK / BLACK |
| `canvasTextureFilter`, `fontTextureFilter` | bilinear |
| `applicationId` | Game; kullanıcı verisinin kalıcı kimliği |
| `font`, `fontCodepoints` | boş yol varsayılan font; özel glyph kapsamı |
| `fontAtlasSizes` | 40,48,56,64,80,96,120,144; [font rehberi](Graphics/Text.md) |
| `assets`, `audio` | boş manifest / katalog |
| `audioVolume` | [ses adımları ve grupları](Audio.md) |
| `save` | [SaveSettings](Modules/Persistence/README.md); dosya bazında override destekli |
| `theme` | NeonArcadeTheme |
| `persistentOptions` | true; false iken oyuncu tercihleri okunup yazılmaz |
| `fixedWindow` | false; host true seçerse seçenekler capture/pencere boyutunu değiştirmez |

Kaynak sözleşmeleri: [GameSettings.h](../Runtime/GameSettings.h),
[WindowSettings.h](../Graphics/Render/WindowSettings.h).
Pencere başlığı `const char*` olduğundan kalıcı string literal kullanmak en basitidir.
Engine kullanıcı seçeneklerini yüklediğinde verilen başlangıç pencere ayarları değişebilir.

## Platform sorumlulukları

Windows masaüstü oyun ve Windows editörü farklı hedeflerdir; editör bağımlılıkları normal oyuna eklenmez.
Headless kullanım `ENGINE_BUILD_RUNTIME=OFF` ile raylib'i kapatır; JSON/Catch2 gibi
etkin hedeflerin kendi bağımlılıkları devam eder.

Android'de `engine_add_game` native shared library üretir. NativeActivity, Gradle/NDK,
uygulama kimliği, manifest, launcher ikonları, SDK düzeyleri ve imzalama tüketici projede kurulur.
Persistence Android Keystore köprüsünün Java kaynağı ve manifest entegrasyonu da paketlenmelidir;
[Persistence](Modules/Persistence/README.md) platform sınırlarını açıklar.
Engine tek başına mağaza paketi/signing projesi sağlamaz.

`SavePath(filename)` applicationId altında platform kullanıcı verisi yolunu üretir. Asset hazırlama, kayıt yolu,
dosya saklama ve güvenli rastgelelik `Platform/Internals/`'tadır (Engine-private); oyun bunlara `Game` ve
Persistence/JsonIO üzerinden ulaşır.
Android özel veri dizini uygulama kaldırılınca silinir. Kimlik veya şifreleme anahtarını
değiştirmek mevcut kayda erişimi etkiler. Oyunun paketleme tercihleri Engine API'si değildir.
