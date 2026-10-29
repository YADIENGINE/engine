# Proje kurulumu

Engine ayrı bir git reposudur (YDArcadeEngine) ve her oyun onu kökünde `Engine/` adıyla submodule olarak alır.
Oyunun kodu, editör önizlemeleri ve belgeleri oyunun reposunda kalır; Engine reposu hiçbir oyunu görmez.

```text
MyProject/                  (oyunun reposu)
  Engine/                   ← submodule: YDArcadeEngine
    EngineSetup.py  Docs/  Tools/  ...
  Game/                     oyun kaynakları + Assets/ (.cpp'ler kendiliğinden bulunur) + Distribution/ (oyuncuya
                            giden bildirim metinleri: GameNotices.txt, credits; bkz. Release.md)
  Editor/                   projenin preview worker'ı (EngineEditor önizlemeleri; .cpp'ler kendiliğinden bulunur)
  CMakeLists.txt  CMakePresets.json
  Project.json              Engine/Tools'un okuduğu proje ayarı
  p.py  editor.py           Engine/Tools'a giden ince sarmalayıcılar
```

Include kökü `Engine/` klasörüdür (`#include "Runtime/Game.h"`); oyun kendi kökünü (`Game/`) ayrıca ekler.

## Yeni proje

Gerekenler: Windows'ta VS 2022 Build Tools (C++ workload + "C++ CMake tools", Ninja onunla gelir), CMake 3.24+,
Python 3.10+. İlk configure raylib, JSON ve test bağımlılıklarını `FetchContent` ile indirir.

Proje klasörüne sadece Engine'i koyun; klasörün adı tam olarak `Engine` olmalı (GitHub zip'i `<repo>-main` adıyla
açılır, yeniden adlandırın). Sonra Engine'in içinde kurulumu çalıştırın:

```powershell
cd MyProject\Engine
python EngineSetup.py              # proje adını sorar (varsayılan: klasörün adı)
python EngineSetup.py --dry-run    # yazmadan ne oluşturacağını listeler
```

`EngineSetup.py` `Tools/ProjectTemplate/`'i bir üst klasöre yazar: `CMakeLists.txt`, `CMakePresets.json`,
`Project.json`, `p.py`, `editor.py`, `.gitignore`, `.gitattributes`, `Game/Assets/`, `Editor/`,
`Game/Distribution/GameNotices.txt`. Proje adı
`project(...)` ve `Project.json` `"name"` alanına yazılır; exe, Release paketi ve editör worker'ı (`<ad>Preview`) bu
addan türer. Ad harfle başlar, sadece harf/rakam/`_` içerir. Var olan dosyaların üzerine yazmaz (atladıklarını
listeler); kökte `Project.json` varsa proje kurulmuş sayılır ve hiçbir şey yapmaz. Sonunda eksik araçları
(Python, CMake, Visual Studio C++ araçları, Ninja) bildirir. Git'e dokunmaz; Engine'i submodule olarak tutmak için
aşağıdaki [git](#git) bölümüne bakın.

```powershell
cd ..
python p.py            # Debug derle + çalıştır
python editor.py       # EngineEditor + projenin worker'ı
python p.py --test     # headless testler (Engine testleri)
```

`Game/` ve `Editor/` boş gelir (`Game/Assets/` ve `.gitkeep`). Boşken `engine_add_game` ve
`engine_add_editor_worker` Engine'in starter oyununu ve starter worker'ını derler: oyun "Game/ has no sources yet"
ekranını, editör Engine → Starter önizlemesini açar. İlk `.cpp` eklendiğinde CMake yeniden yapılandırır ve projenin
kodu devreye girer: [İlk oyun](GettingStarted.md), [İlk editör önizlemesi](Editor/GettingStarted.md).

## Project.json

Engine/Tools'un projeyi tanıdığı tek yer. Yollar proje köküne göredir; preset adları projenin
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
| `game.target` | `p.py` | `engine_add_game` hedefi (varsayılan `name`); exe `buildDirectory/<Config>/<target>.exe` |
| `game.buildDirectory` | `p.py` | configure preset'inin `binaryDir`'ı ile aynı olmalı |
| `release` | `p.py --release` | `directory/name/` (varsayılan `Release/<name>/`) ve yanına ZIP: exe + `Assets` + üretilen `ThirdPartyNotices.txt` + `files`; [Release](Release.md) |
| `tests` | `p.py --test` | test preset'leri; sonuç `buildDirectory/reports/<run-id>/results.json` |
| `editor.worker` | `editor.py` | `engine_add_editor_worker` hedefi (varsayılan `<name>Preview`) |
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
| `build_cli.py` | `p.py`: derle, çalıştır, `--release` paketle (`Engine/ReleaseSystem/`, [Release](Release.md)), `--clean`, `--test` |
| `editor_cli.py` | `editor.py`: EngineEditor + worker derle ve aç |
| `test_runner.py` | CTest'i koşup yapılandırılmış rapor yazar; çıkış kodu 0 başarı, 1 test hatası, 2 altyapı/seçim hatası |
| `project_config.py` | `Project.json` ve hook yükleyici |
| `ProjectTemplate/` | yeni projenin dosyaları; `@PROJECT_NAME@` adla değişir, `gitignore`/`gitattributes` noktalı yazılır |

Araçların kendi testleri derleyici istemez: `Engine/Tools` içinde `python -m unittest discover -s Tests`.

## Git

`EngineSetup.py` git'e dokunmaz. Oyunu git'te tutup Engine'i submodule olarak bağlamak için, kurulumdan sonra proje
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
- Engine'i tek başına (oyun olmadan) test etmek: Engine reposunun kökünde `python Tools/build_cli.py --test`
  (kendi `Project.json` ve `CMakePresets.json`'ı vardır; ayrıntı [Testing](Contributing/Testing.md)).

## Android

Gradle projesi oyuna aittir. Engine'den iki yol gerekir: kök `CMakeLists.txt` (engine_add_game Android'de shared
library kurar) ve kayıt anahtarı köprüsünün Java kaynağı
`Engine/Modules/Persistence/Internals/Android/java` (`org.ydarcadeengine.persistence.SaveKeyBridge`).
Ayrıntı: [Asset ve platformlar](AssetsAndPlatforms.md).
