# Engine documentation

Bu klasör C++20 / raylib tabanlı engine ile oyun geliştirenler içindir. Engine'in adı ve sürümü
`EngineManifest.json`'dadır; belgeler adı yazmaz. Engine ayrı bir repodur;
oyunlar onu `Engine/` submodule'ü olarak alır. Oyuna giren engine kodu `Core/`, editör `Editor/`, dış bağımlılıklar
`ThirdParty/`, belgeler `Docs/`, build/editör/test araçları ve yeni proje şablonu `Tools/` altındadır; komutlar `python Engine <komut>`
([ToolsContract](ToolsContract.md)), yeni proje `python Engine new` ile kurulur.
Oyun kuralları, içerik katalogları ve projeye özel editör yardımcıları oyunun kendi belgelerinde kalır.

## İlk adımlar

0. [Proje kurulumu](ProjectSetup.md): `python Engine new`, `Project.json`, komutlar, submodule.
1. [İlk oyun](GettingStarted.md): CMake, fabrika, oyun ve ilk sahne.
2. [Mimari](Architecture.md) ve [runtime](Runtime.md): sahiplik, lifecycle, zaman ve sahne geçişleri.
3. [Grafik](Graphics/README.md), [input](Input.md), [ses](Audio.md) ve
   [asset/platformlar](AssetsAndPlatforms.md).
4. [Ayarlar ve kayıtlar](OptionsAndSaves.md), [modüller](Modules/README.md) ve
   [gameplay yardımcıları](GameplayHelpers.md).
5. [Editör](Editor/README.md): projeyi bağlama ve üretim çizimini önizleme.
6. [Release paketi](Release.md): `python Engine release`, `Release/` düzeni ve üçüncü taraf bildirimleri. Oyuna dış
   asset ya da kütüphane ekleyen her değişiklik `Game/Distribution/` bildirimini de günceller.

## Konu dizini

| İhtiyaç | Belge |
| --- | --- |
| Pencere, canvas, katmanlar ve clipping | [Rendering](Graphics/Rendering.md) |
| Font, Unicode, ölçüm ve satır düzeni | [Text](Graphics/Text.md) |
| Tema, widget ve arka planlar | [Themes and UI](Graphics/ThemesAndUi.md) |
| Parçacık, kamera, geçiş ve overlay | [Effects](Graphics/Effects.md) |
| Derlemeden ayarlanan değerler (`*.tuning.json`) | [Tuning](Tuning.md) |
| Genel JSON run arşivi | [RunTracking](Modules/RunTracking/README.md) |
| Python ile çevrimdışı ses üretimi | [Audio generation](AudioGeneration.md) |
| Release paketi, lisans/credits bildirimleri | [Release](Release.md) |
| Engine/modül testi yazmak | [Testing](Contributing/Testing.md) |
| Yeni modül geliştirmek | [Module rules](Contributing/ModuleRules.md) |
| Public API / `Internals/` ayrımı | [API boundaries](Contributing/ApiBoundaries.md) |

Modül README'leri desteklenen API'yi, `Roadmap.md` dosyaları gelecekteki işleri anlatır.
Engine'in exe-editöre dönüşme planı ve alınan mimari kararlar engine reposunda `Docs/Design/` altındadır (launcher'ın
proje kopyasına girmez).
Editörün [geliştirme notları](Editor/Development/Roadmap.md) ve
[tarihçesi](Editor/Development/History.md) kullanım rehberinden ayrıdır.
Belgelerdeki komutlar geliştiricinin çalıştırması içindir; belge değişikliği build gerektirmez.
