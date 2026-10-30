# Proje kurulumu

Engine ayrı bir git reposudur ve her oyun onu kökünde `Engine/` adıyla submodule olarak alır.
Oyunun kodu, editör önizlemeleri ve belgeleri oyunun reposunda kalır; Engine reposu hiçbir oyunu görmez.

```text
MyProject/                  (oyunun reposu)
  Engine/                   ← submodule: engine reposu
    Core/  Editor/  Docs/  Tools/  ThirdParty/  __main__.py  ...
  Game/                     oyun kaynakları + Assets/ (.cpp'ler kendiliğinden bulunur) + Distribution/ (oyuncuya
                            giden bildirim metinleri: GameNotices.txt, credits; bkz. Release.md)
  Editor/                   projenin preview worker'ı (EngineEditor önizlemeleri; .cpp'ler kendiliğinden bulunur)
  CMakeLists.txt  CMakePresets.json
  Project.json              engine araçlarının okuduğu proje ayarı
```

Projenin kökünde engine'e ait betik yoktur; bütün komutlar proje kökünden `python Engine <komut>` ile çalışır
(liste ve sözleşme: [ToolsContract](ToolsContract.md)).

Include kökü `Engine/Core/` klasörüdür (`#include "Runtime/Game.h"`); editör API'si engine köküne göredir
(`#include "Editor/Api/..."`). Oyun kendi kökünü (`Game/`) ayrıca ekler.

## Yeni proje

Gerekenler: Windows'ta VS 2022 Build Tools (C++ workload + "C++ CMake tools", Ninja onunla gelir), CMake 3.24+,
Python 3.10+. Bağımlılıklar (raylib, JSON, editör ve test kütüphaneleri) engine'in `ThirdParty/` klasöründe gelir;
derleme internet istemez.

Proje klasörüne sadece Engine'i koyun; klasörün adı tam olarak `Engine` olmalı (GitHub zip'i `<repo>-main` adıyla
açılır, yeniden adlandırın). Sonra proje klasöründen kurulumu çalıştırın:

```powershell
cd MyProject
python Engine new                  # proje adını sorar (varsayılan: klasörün adı)
python Engine new --name MyGame    # sormadan
python Engine new --dry-run        # yazmadan ne oluşturacağını listeler
```

`new` `Engine/Tools/ProjectTemplate/`'i proje klasörüne yazar: `CMakeLists.txt`, `CMakePresets.json`,
`Project.json`, `.gitignore`, `.gitattributes`, `Game/Assets/`, `Editor/`, `Game/Distribution/GameNotices.txt`.
Proje adı
`project(...)` ve `Project.json` `"name"` alanına yazılır; exe, Release paketi ve editör worker'ı (`<ad>Preview`) bu
addan türer. Ad harfle başlar, sadece harf/rakam/`_` içerir. Var olan dosyaların üzerine yazmaz (atladıklarını
listeler); kökte `Project.json` varsa proje kurulmuş sayılır ve hiçbir şey yapmaz. Sonunda eksik araçları
(Python, CMake, Visual Studio C++ araçları, Ninja) bildirir. Git'e dokunmaz; Engine'i submodule olarak tutmak için
aşağıdaki [git](#git) bölümüne bakın.

```powershell
python Engine run       # Debug derle + çalıştır
python Engine editor    # EngineEditor + projenin worker'ı
python Engine test      # headless testler (Engine testleri)
```

`Game/` ve `Editor/` boş gelir (`Game/Assets/` ve `.gitkeep`). Boşken `engine_add_game` ve
`engine_add_editor_worker` Engine'in starter oyununu ve starter worker'ını derler: oyun "Game/ has no sources yet"
ekranını, editör Engine → Starter önizlemesini açar. İlk `.cpp` eklendiğinde CMake yeniden yapılandırır ve projenin
kodu devreye girer: [İlk oyun](GettingStarted.md), [İlk editör önizlemesi](Editor/GettingStarted.md).

## Project.json

Engine araçlarının projeyi tanıdığı tek yer. Yollar proje köküne göredir; preset adları projenin
`CMakePresets.json`'ındaki adlardır. Sadece kullanılan bölümler gerekir (ör. editörü olmayan projede `editor` yok).
Aşağıdaki örnek bütün alanları gösterir; şablon `target`, `release.name` ve `editor.worker`'ı vermez, varsayılanlar
`name`'den gelir.

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
  "release": {
    "directory": "Release", "name": "MyProject",
    "notices": "Game/Distribution/GameNotices.txt", "files": ["Game/Distribution/AudioCredits.txt"]
  },
  "tests": {
    "buildDirectory": "build/tests",
    "configurePreset": "tests", "buildPreset": "tests-debug", "testPreset": "tests-debug"
  },
  "editor": {
    "buildDirectory": "build/editor",
    "worker": "MyProjectPreview",
    "cacheVariables": { "APP_BUILD_GAME": "OFF", "APP_BUILD_EDITOR": "ON" }
  }
}
```

| Alan | Kullanan | Anlamı |
| --- | --- | --- |
| `game.target` | `build`, `run` | `engine_add_game` hedefi (varsayılan `name`); exe `buildDirectory/<Config>/<target>.exe` |
| `game.buildDirectory` | `build`, `run` | configure preset'inin `binaryDir`'ı ile aynı olmalı |
| `release` | `release` | `directory/name/` (varsayılan `Release/<name>/`) ve yanına ZIP: exe + `Assets` + üretilen `ThirdPartyNotices.txt` + `files`; [Release](Release.md) |
| `tests` | `test` | test preset'leri; sonuç `buildDirectory/reports/<run-id>/results.json` |
| `editor.worker` | `editor` | `engine_add_editor_worker` hedefi (varsayılan `<name>Preview`) |
| `editor.cacheVariables` | `editor` | editör build'ine eklenen proje seçenekleri (oyunu kapatıp worker'ı açmak gibi) |
| `hooks` | `release` | isteğe bağlı Python dosyası (aşağıda) |

`editor` `ENGINE_BUILD_RUNTIME/EDITOR=ON`, `ENGINE_BUILD_TESTS/EDITOR_TESTS=OFF` ve
`CMAKE_CONFIGURATION_TYPES=Debug;Release`'i kendisi verir. Editör ayrı build klasöründe yapılandırılır ve
oyunun build'ine dokunmaz.

### Hook'lar

JSON'a sığmayan projeye özel iş `hooks` dosyasındaki bilinen fonksiyonlardadır:

| Fonksiyon | Ne zaman | Dönüş |
| --- | --- | --- |
| `package_assets(root: Path) -> list[str]` | `python Engine release` | pakete girecek `Assets/`'e göre yollar (klasör ya da dosya); yoksa `Assets/` olduğu gibi kopyalanır |

Örneğin bir oyun ses kataloğunu kaynak dosyasından okuyup sadece kullanılan sesleri pakete koyabilir.

## Araçlar (Engine/Tools)

Komutlar ve seçenekleri: [ToolsContract](ToolsContract.md). Python paketi, sadece standart kütüphane:

| Modül | İş |
| --- | --- |
| `cli.py` | `python Engine <komut>`: argümanlar, proje kökü, hata → çıkış kodu |
| `project.py`, `manifest.py` | `Project.json` (hook yükleyici dahil), `EngineManifest.json` |
| `msvc.py` | MSVC ortamı (vcvars64); `build/.msvc-env.json`'da önbelleğe alınır, VS güncellenince yenilenir |
| `cmake.py` | komut çalıştırma, CMake önbelleği, build klasörünü güvenle silme |
| `commands/` | `build.py` (build, run), `test.py` (yapılandırılmış rapor), `editor.py`, `release.py` ([Release](Release.md)), `new.py` |
| `include_boundaries.py` | `check`: Internals sınırı ([API boundaries](Contributing/ApiBoundaries.md)) |
| `ProjectTemplate/` | yeni projenin dosyaları; `@PROJECT_NAME@` adla değişir, `gitignore`/`gitattributes` noktalı yazılır |

Araçların kendi testleri derleyici istemez: Engine kökünde `python -m unittest discover -s Tools/Tests -t .`.

## Git

`new` git'e dokunmaz. Oyunu git'te tutup Engine'i submodule olarak bağlamak için, kurulumdan sonra proje
kökünde:

```powershell
git init
git submodule add <ydarcadeengine-url> Engine   # Engine/ zaten bu URL'nin klonuysa onu kaydeder
git config -f .gitmodules submodule.Engine.branch main
```

Engine zip'ten geldiyse önce `Engine/`'i silip yukarıdaki `submodule add` ile yeniden alın; ya da Engine'i düz klasör
olarak oyunla birlikte commit'leyin (o zaman güncellemeler elle kopyalanır).

## Engine'i güncellemek ve Engine'de çalışmak

- Güncel Engine'i almak: `git submodule update --remote Engine`, ardından oyunun reposunda `Engine` pointer'ını
  commit'leyin. Klonlarken `git clone --recurse-submodules` ya da sonradan `git submodule update --init`.
- Engine'de değişiklik: önce `Engine/` içinde dalda (`git -C Engine switch main`) commit ve push, sonra oyunun
  reposunda pointer commit'i. Tek commit'te iki repoya birden yazılamaz; oyunun commit'i her zaman Engine'in
  push'lanmış bir commit'ini göstermelidir.
- Engine'i tek başına (oyun olmadan) test etmek: Engine reposunun kökünde `python . test`
  (kendi `Project.json` ve `CMakePresets.json`'ı vardır; ayrıntı [Testing](Contributing/Testing.md)).
