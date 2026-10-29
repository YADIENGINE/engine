# Engine documentation

Bu klasör C++20 / raylib tabanlı engine (YDArcadeEngine) ile oyun geliştirenler içindir. Engine ayrı bir repodur;
oyunlar onu `Engine/` submodule'ü olarak alır. Kaynak kod reponun kökünde, belgeler `Docs/`, build/editör/test
araçları ve yeni proje şablonu `Tools/` altındadır; yeni proje `EngineSetup.py` ile kurulur.
Oyun kuralları, içerik katalogları ve projeye özel editör yardımcıları oyunun kendi belgelerinde kalır.

## İlk adımlar

0. [Proje kurulumu](ProjectSetup.md): `EngineSetup.py`, `Project.json`, `p.py` / `editor.py`, submodule.
1. [İlk oyun](GettingStarted.md): CMake, fabrika, oyun ve ilk sahne.
2. [Mimari](Architecture.md) ve [runtime](Runtime.md): sahiplik, lifecycle, zaman ve sahne geçişleri.
3. [Grafik](Graphics/README.md), [input](Input.md), [ses](Audio.md) ve
   [asset/platformlar](AssetsAndPlatforms.md).
4. [Ayarlar ve kayıtlar](OptionsAndSaves.md), [modüller](Modules/README.md) ve
   [gameplay yardımcıları](GameplayHelpers.md).
5. [Editör](Editor/README.md): projeyi bağlama ve üretim çizimini önizleme.

## Konu dizini

| İhtiyaç | Belge |
| --- | --- |
| Pencere, canvas, katmanlar ve clipping | [Rendering](Graphics/Rendering.md) |
| Font, Unicode, ölçüm ve satır düzeni | [Text](Graphics/Text.md) |
| Tema, widget ve arka planlar | [Themes and UI](Graphics/ThemesAndUi.md) |
| Parçacık, kamera, geçiş ve overlay | [Effects](Graphics/Effects.md) |
| Genel JSON run arşivi | [RunTracking](Modules/RunTracking/README.md) |
| Python ile çevrimdışı ses üretimi | [Audio generation](AudioGeneration.md) |
| Engine/modül testi yazmak | [Testing](Contributing/Testing.md) |
| Yeni modül geliştirmek | [Module rules](Contributing/ModuleRules.md) |

Modül README'leri desteklenen API'yi, `Roadmap.md` dosyaları gelecekteki işleri anlatır.
Editörün [geliştirme notları](Editor/Development/Roadmap.md) ve
[tarihçesi](Editor/Development/History.md) kullanım rehberinden ayrıdır.
Belgelerdeki komutlar geliştiricinin çalıştırması içindir; belge değişikliği build gerektirmez.
