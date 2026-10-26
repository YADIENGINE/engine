# Zortengine (C++20 / raylib 5.5)

Oyundan bağımsız, taşınabilir altyapı. Ayrı repodur; oyunlar onu kökünde `Engine/` submodule'ü olarak alır.
Kullanım belgeleri `Docs/` (giriş `Docs/README.md`, proje kurulumu `Docs/ProjectSetup.md`).

**Bu repo hiçbir oyunu bilmez.** Oyun adı, oyun içeriği, oyun yolu (`Game/...`) ya da oyuna özel kural yazılmaz;
örnekler `MyProject` / `MyGame` adlarıyla. Bir oyunun içinde çalışırken Engine değişikliği önce burada
(dalda) commit'lenir, sonra oyunun reposunda submodule pointer'ı commit'lenir.

## Build ve test
Build almak kullanıcıya bırakılır. Testler: tek başına `python Tools/build_cli.py --test [--label X | --filter X]`
(sonuç `build/tests/reports/<run-id>/results.json`; çıkış kodu 0 başarı, 1 test hatası, 2 altyapı hatası);
bir oyunun içinden `python p.py --test --label engine` (ya da modül etiketi: `json-io`, `persistence` ...).
Araçların kendi testleri derleyici istemez: `Tools` içinde `python -m unittest discover -s Tests`.
Test yazma kuralları `Docs/Contributing/Testing.md`; modül kuralları `Docs/Contributing/ModuleRules.md`.

## Yapı
```
Runtime/   Game, GameSettings, Scene, GameObject, ObjectList; ana döngü ve lifecycle; Main.cpp CreateGame() çağırır
Graphics/  Render/ Draw/ Anim/ Icons/ Style/ Ui/ (görünüşün tamamı; renkler/stiller parametre)
Input/ Audio/ Fx/ Options/ Platform/ Menus/ Gameplay/ Progress/ Tracking/ Utilities/
Modules/   Opt-in sistemler (JsonIO, Persistence, Localization, Version, Steam, ScoreBoard ...); Modules/X/Tests/
Editor/    EngineEditor (ImGui kabuğu), preview API'si ve runtime'ı; engine_add_editor_worker
Tests/     Engine testleri + Support/ (TempDirectory, Diagnostics)
Docs/      Kullanım ve katkı belgeleri
Tools/     buildenv.py (MSVC ortamı), build_cli.py (p.py), editor_cli.py (editor.py), test_runner.py, project_config.py
Templates/NewProject/  Yeni oyun iskeleti (CMake, preset'ler, Project.json, sarmalayıcılar, Game/, Editor/)
```

## Kurallar
- CMake: `EngineCore` (headless) / `Engine` (raylib'li) ve modül hedefleri; oyun `engine_add_game`, editör worker'ı
  `engine_add_editor_worker` ile kurulur. Yeni `.cpp` ilgili hedefin listesine.
- Bağımlılık yönleri (`Graphics/`): `Anim`, `Render` → `Utilities`; `Draw` → `Render` + `Anim`; `Icons` → `Draw`;
  `Fx` → `Draw` + `Render` + `Anim`; `Style` → `Draw` + `Fx` (sadece stil tipleri); `Ui` → `Style` + `Draw`.
  `Draw`, `Render` ve `Fx` temayı okumaz, stilleri ve saati parametre alır.
- Headless kod (`EngineCore`, `Utilities/`, mantık yardımcıları) raylib include etmez. `InitWindow`/`CloseWindow`
  sadece `Window`'da. Canvas koordinatları mantıksaldır; scissor `BeginClip` / `BeginUiClip`.
- Yeni yardımcı yazmadan önce mevcutları kullan: `Easing.h`, `MathUtils.h`, `DrawUtils.h`, `Neon.h`, `VectorMath.h`.
- Kod: C++20, `/W4` uyarısız; isimli namespace yok, `.cpp`'ye özel yardımcılar anonim namespace'te; her class kendi
  `.h` + `.cpp`'sinde; `#pragma once`; include'lar Engine köküne göre (`../` yok), sıra: kendi header → üçüncü
  parti → std → proje. `new`/`delete` ve C dizisi yok, RAII. `enum class` tek satır. Tip/fonksiyon `PascalCase`,
  yerel `camelCase`, private üye `_camelCase`, `constexpr` sabit `PascalCase`. Getter'lar `[[nodiscard]] const`.
  Global isimler raylib ile çakışmaz. Format `.clang-format` (4 boşluk, 120 sütun), satır sonu LF.
- Python araçları proje köküne ve `Project.json`'a göre çalışır; proje adı/yolu sabit yazılmaz.
- Commit mesajları kısa, emir kipinde, İngilizce.
