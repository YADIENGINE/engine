# Proje kurulumu

Engine ayrı bir git reposudur (Zortengine) ve her oyun onu kökünde `Engine/` adıyla submodule olarak alır.
Oyunun kodu, editör önizlemeleri ve belgeleri oyunun reposunda kalır; Engine reposu hiçbir oyunu görmez.

```text
MyProject/                  (oyunun reposu)
  Engine/                   ← submodule: Zortengine
    Docs/  Tools/  Templates/NewProject/  ...
  Game/                     oyun kaynakları + Assets/
  Editor/                   projenin preview worker'ı (EngineEditor önizlemeleri)
  CMakeLists.txt  CMakePresets.json
  Project.json              Engine/Tools'un okuduğu proje ayarı
  p.py  editor.py           Engine/Tools'a giden ince sarmalayıcılar
```

Include kökü `Engine/` klasörüdür (`#include "Runtime/Game.h"`); oyun kendi kökünü (`Game/`) ayrıca ekler.

## Yeni proje

Gerekenler: Windows'ta VS 2022 Build Tools (C++ workload + "C++ CMake tools", Ninja onunla gelir), CMake 3.24+,
Python 3.10+. İlk configure raylib, JSON ve test bağımlılıklarını `FetchContent` ile indirir.

```powershell
git init MyProject
cd MyProject
git submodule add <zortengine-url> Engine
git config -f .gitmodules submodule.Engine.branch main
```

Sonra `Engine/Templates/NewProject/` içeriğini (gizli `.gitignore` dahil) proje köküne kopyalayın ve
`MyProject` / `MYPROJECT_` adlarını kendi projenizinkiyle değiştirin: `CMakeLists.txt`, `CMakePresets.json`,
`Project.json`, `Game/MyGame.cpp` (`applicationId`, pencere başlığı) ve `Editor/Main.cpp`.

```powershell
python p.py            # Debug derle + çalıştır
python editor.py       # EngineEditor + projenin worker'ı
python p.py --test     # headless testler
```

Şablonun kodu [İlk oyun](GettingStarted.md) ve [İlk editör önizlemesi](Editor/GettingStarted.md) rehberlerinde
adım adım anlatılır.

## Project.json

Engine/Tools'un projeyi tanıdığı tek yer. Yollar proje köküne göredir; preset adları projenin
`CMakePresets.json`'ındaki adlardır. Sadece kullanılan bölümler gerekir (ör. editörü olmayan projede `editor` yok).

```json
{
  "version": 1,
  "name": "MyProject",
  "hooks": "ProjectHooks.py",
  "game": {
    "target": "MyProject",
    "buildDirectory": "build",
    "configurePreset": "desktop",
    "buildPresets": { "Debug": "debug", "Release": "release" }
  },
  "package": { "directory": "Final", "name": "MyProject", "files": ["Distribution/ThirdPartyNotices.txt"] },
  "tests": {
    "buildDirectory": "build/tests",
    "configurePreset": "tests", "buildPreset": "tests-debug", "testPreset": "tests-debug"
  },
  "editor": {
    "buildDirectory": "build/editor",
    "worker": "MyProjectPreview",
    "cacheVariables": { "MYPROJECT_BUILD_GAME": "OFF", "MYPROJECT_BUILD_EDITOR": "ON" }
  }
}
```

| Alan | Kullanan | Anlamı |
| --- | --- | --- |
| `game.target` | `p.py` | `engine_add_game` hedefi; exe `buildDirectory/<Config>/<target>.exe` |
| `game.buildDirectory` | `p.py` | configure preset'inin `binaryDir`'ı ile aynı olmalı |
| `package` | `p.py --release` | `directory/name/` klasörü ve yanına ZIP: exe + `Assets` + `files` |
| `tests` | `p.py --test` | test preset'leri; sonuç `buildDirectory/reports/<run-id>/results.json` |
| `editor.worker` | `editor.py` | `engine_add_editor_worker` hedefi |
| `editor.cacheVariables` | `editor.py` | editör build'ine eklenen proje seçenekleri (oyunu kapatıp worker'ı açmak gibi) |
| `hooks` | `p.py` | isteğe bağlı Python dosyası (aşağıda) |

`editor.py` `ENGINE_BUILD_RUNTIME/EDITOR=ON`, `ENGINE_BUILD_TESTS/EDITOR_TESTS=OFF` ve
`CMAKE_CONFIGURATION_TYPES=Debug;Release`'i kendisi verir. Editör ayrı build klasöründe yapılandırılır ve
oyunun build'ine dokunmaz.

### Hook'lar

JSON'a sığmayan projeye özel iş `hooks` dosyasındaki bilinen fonksiyonlardadır:

| Fonksiyon | Ne zaman | Dönüş |
| --- | --- | --- |
| `package_assets(root: Path) -> list[str]` | `p.py --release` | pakete girecek `Assets/`'e göre yollar (klasör ya da dosya); yoksa `Assets/` olduğu gibi kopyalanır |

Örneğin bir oyun ses kataloğunu kaynak dosyasından okuyup sadece kullanılan sesleri pakete koyabilir.

## Araçlar (Engine/Tools)

| Dosya | İş |
| --- | --- |
| `buildenv.py` | MSVC ortamı (vcvars64); `build/.msvc-env.json`'da önbelleğe alınır, VS güncellenince yenilenir |
| `build_cli.py` | `p.py`: derle, çalıştır, `--release` paketle, `--clean`, `--test` |
| `editor_cli.py` | `editor.py`: EngineEditor + worker derle ve aç |
| `test_runner.py` | CTest'i koşup yapılandırılmış rapor yazar; çıkış kodu 0 başarı, 1 test hatası, 2 altyapı/seçim hatası |
| `project_config.py` | `Project.json` ve hook yükleyici |

Araçların kendi testleri derleyici istemez: `Engine/Tools` içinde `python -m unittest discover -s Tests`.

## Engine'i güncellemek ve Engine'de çalışmak

- Güncel Engine'i almak: `git submodule update --remote Engine`, ardından oyunun reposunda `Engine` pointer'ını
  commit'leyin. Klonlarken `git clone --recurse-submodules` ya da sonradan `git submodule update --init`.
- Engine'de değişiklik: önce `Engine/` içinde dalda (`git -C Engine switch main`) commit ve push, sonra oyunun
  reposunda pointer commit'i. Tek commit'te iki repoya birden yazılamaz; oyunun commit'i her zaman Engine'in
  push'lanmış bir commit'ini göstermelidir.
- Engine'i tek başına (oyun olmadan) test etmek: Engine reposunun kökünde `python Tools/build_cli.py --test`
  (kendi `Project.json` ve `CMakePresets.json`'ı vardır; ayrıntı [Testing](Contributing/Testing.md)).

## Android

Gradle projesi oyuna aittir. Engine'den iki yol gerekir: kök `CMakeLists.txt` (engine_add_game Android'de shared
library kurar) ve kayıt anahtarı köprüsünün Java kaynağı
`Engine/Modules/Persistence/Internals/Android/java` (`org.zortengine.persistence.SaveKeyBridge`).
Ayrıntı: [Asset ve platformlar](AssetsAndPlatforms.md).
