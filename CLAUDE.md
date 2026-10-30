# Engine (C++20 / raylib 5.5)

Oyundan bağımsız, taşınabilir altyapı. Ayrı repodur; oyunlar onu kökünde `Engine/` submodule'ü olarak alır.
Kullanım belgeleri `Docs/` (giriş `Docs/README.md`, proje kurulumu `Docs/ProjectSetup.md`).

**Engine'in adı sabit yazılmaz.** Ad ve sürümün tek kaynağı `EngineManifest.json`'dur (ad değişebilir). Belgelerde
"engine" yazılır; komut / exe / klasör adlarında `<engine>` yer tutucusu kullanılır. Kod adı ve sürümü CMake'in
`EngineHeadless`'a verdiği `ENGINE_NAME` / `ENGINE_VERSION` tanımlarından, Python araçları manifest'ten okur.

**Bu repo hiçbir oyunu bilmez.** Oyun adı, oyun içeriği, oyun yolu (`Game/...`) ya da oyuna özel kural yazılmaz;
örnekler `MyProject` / `MyGame` adlarıyla. Bir oyunun içinde çalışırken Engine değişikliği önce burada
(dalda) commit'lenir, sonra oyunun reposunda submodule pointer'ı commit'lenir.

## Build ve test
Build almak kullanıcıya bırakılır. Bütün araçlar tek komut satırındadır: proje kökünden `python Engine <komut>`
(`build`, `run`, `release`, `test`, `editor`, `new`, `info`, `check`); engine reposunun kökünden `python . <komut>`.
Komutlar, seçenekler ve çıkış kodları (0 başarı, 1 derleme/test hatası, 2 kullanım/altyapı) launcher ile sözleşmedir:
`Docs/ToolsContract.md`; bozan değişiklik `EngineManifest.json` `toolsContract`'ı artırır.
Testler: tek başına `python . test [--label X | --filter X]` (sonuç `build/tests/reports/<run-id>/results.json`);
bir oyunun içinden `python Engine test --label engine` (ya da modül etiketi: `json-io`, `persistence` ...).
Python testleri derleyici istemez, Engine kökünde: araçlar `python -m unittest discover -s Tools/Tests -t .`, lisans
bildirimleri `python -m unittest discover -s ThirdParty/Tests` (ikisi de `engine` etiketiyle CTest'te).
Test yazma kuralları `Docs/Contributing/Testing.md`; modül kuralları `Docs/Contributing/ModuleRules.md`.
Include sınırı denetimi derleyicisiz: `python Engine check` (CTest `Engine.IncludeBoundaries`).

## Yapı
```
Core/      Oyuna giren engine kodu; include kökü (#include "Runtime/Game.h"). Hedefler Core/CMakeLists.txt'te:
           EngineHeadless (raylib'siz), Engine (raylib'li), modüller; engine_add_game.
  Runtime/   Game, GameSettings; ana döngü ve servisler; Main.cpp CreateGame() çağırır
  GameObjects/ GameObject, ObjectList; headless nesne lifecycle'ı ve isteğe bağlı sahiplik
  Scene/     Scene, Internals/SceneManager; headless sahne lifecycle'ı, seçimi ve kapanışı
  Graphics/  Render/ (Window/Canvas/Renderer Internals/'da) Draw/ Anim/ Icons/ Style/ Ui/ (görünüşün tamamı; renkler/stiller parametre)
  Input/ Audio/ Fx/ Options/ Platform/ Menus/ Gameplay/ Progress/ Utilities/
  Modules/   Opt-in sistemler (JsonIO, Persistence, Localization, Version, Steam, ScoreBoard, RunTracking ...); Modules/X/Tests/
  Tests/     Engine testleri + Support/ (TempDirectory, Diagnostics)
Editor/    EngineEditor (ImGui kabuğu), preview API'si ve runtime'ı; engine_add_editor_worker. Oyuna girmez;
           include'ları engine köküne göre (#include "Editor/Api/...").
Docs/      Kullanım ve katkı belgeleri
ThirdParty/ Bütün dış bağımlılıkların değiştirilmemiş kaynakları (raylib, nlohmann_json, Monocypher, imgui, rlImGui,
           Catch2; FetchContent yok) + oyuna girenlerin lisans bildirimi: Notices.json, Notices/*.txt, notices.py, Tests/.
           Tarif ThirdParty/README.md. Catch2'yi test CMake'i engine_use_catch2() ile alır.
Tools/     Python paketi, sadece standart kütüphane (Docs/ToolsContract.md):
  cli.py       argparse alt komutları; sözleşmenin tek uygulaması
  project.py manifest.py msvc.py cmake.py errors.py   Project.json, EngineManifest.json, MSVC ortamı, ortak CMake işleri,
               çıkış kodları (ToolError)
  commands/    build.py (build, run), test.py, editor.py, release.py (paket; büyürse alt paket), new.py (proje kurulumu)
  include_boundaries.py   Internals sınırı (check)
  AudioGeneration/  Çevrimdışı Python ses üretimi (NumPy / SciPy); tek public import AudioGeneration.SoundGenerator,
               tarifler ve asset yolları çağırandadır. API ve genişletme: Docs/AudioGeneration.md.
  ProjectTemplate/  `new`'in kopyaladığı proje dosyaları; @PROJECT_NAME@, gitignore/gitattributes noktasız saklanır.
               Kurulan proje olduğu gibi derlenmeli: boş Game/ → Runtime/Starter/StarterGame.cpp, boş Editor/ →
               Editor/Preview/StarterWorker.cpp. Örnek kod şablonda değil Docs/GettingStarted rehberlerinde.
  Tests/       unittest (python -m unittest discover -s Tools/Tests -t .)
__main__.py  `python Engine <komut>` → Tools.cli
EngineManifest.json  Ad, sürüm, toolsContract, notlar
```

## Kurallar
- CMake: `EngineHeadless` (headless) / `Engine` (raylib'li) ve modül hedefleri; oyun `engine_add_game`, editör worker'ı
  `engine_add_editor_worker` ile kurulur. Yeni `.cpp` ilgili hedefin listesine.
- Bağımlılık yönleri (`Graphics/`): `Anim`, `Render` → `Utilities`; `Draw` → `Render` + `Anim`; `Icons` → `Draw`;
  `Fx` → `Draw` + `Render` + `Anim`; `Style` → `Draw` + `Fx` (sadece stil tipleri); `Ui` → `Style` + `Draw`.
  `Draw`, `Render` ve `Fx` temayı okumaz, stilleri ve saati parametre alır.
- Headless kod (`EngineHeadless`, `Utilities/`, mantık yardımcıları) raylib include etmez. `InitWindow`/`CloseWindow`
  sadece `Window`'da. Canvas koordinatları mantıksaldır; scissor `BeginClip` / `BeginUiClip`.
- Yeni yardımcı yazmadan önce mevcutları kullan: `Easing.h`, `MathUtils.h`, `DrawUtils.h`, `Neon.h`, `VectorMath.h`.
- Kod: C++20, `/W4` uyarısız; isimli namespace yok, `.cpp`'ye özel yardımcılar anonim namespace'te; her class kendi
  `.h` + `.cpp`'sinde; `#pragma once`; include'lar `Core/`'a göre (editör: engine köküne göre; `../` yok), sıra: kendi header → üçüncü
  parti → std → proje. `new`/`delete` ve C dizisi yok, RAII. `enum class` tek satır. Tip/fonksiyon `PascalCase`,
  yerel `camelCase`, private üye `_camelCase`, `constexpr` sabit `PascalCase`. Getter'lar `[[nodiscard]] const`.
  Global isimler raylib ile çakışmaz. Format `.clang-format` (4 boşluk, 120 sütun), satır sonu LF.
- Python araçları proje köküne ve `Project.json`'a göre çalışır; proje adı/yolu sabit yazılmaz.
- Dış bağımlılık sadece `ThirdParty/`'ye, değiştirilmeden girer; `FetchContent` kullanılmaz. Oyuna giren bağımlılığı
  eklemek ya da sürümünü değiştirmek `ThirdParty/Notices.json` + `Notices/*.txt` güncellemesiyle aynı committe olur
  (`ThirdParty/README.md`); yoksa her oyunun Release'i durur. Editör/test bağımlılıkları listelenmez.
- Public / `Internals/` ayrımı (`Docs/Contributing/ApiBoundaries.md`): özel dosya kendi sisteminin `X/Internals/`'ında,
  kökte `Internals/` yok. Çekirdek `X/Internals/` Engine-private (oyun include etmez); `Modules/X/Internals/` ve
  `Editor/Internals/` sadece sahibine. Public header'ın include ettiği dosya Internals'a taşınmadan önce gizlenir.
- Commit mesajları kısa, emir kipinde, İngilizce.
