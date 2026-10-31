# İlk oyun

Projeyi önce [Proje kurulumu](ProjectSetup.md)'ndaki gibi `python Engine new` ile kurun. Kurulumun açtığı
`Game/` klasörü boştur; `python Engine run` o hâlde Engine'in starter oyununu ("Game/ has no sources yet") açar.
Bu rehber `Game/` altına ilk küçük oyunu yazar.

## Dosya yapısı

```text
MyProject/
  CMakeLists.txt  CMakePresets.json  Project.json
  Engine/                   (submodule)
  Game/
    Assets/
    CreateGame.cpp
    MyGame.h  MyGame.cpp
    MyScene.h  MyScene.cpp
  Editor/                   (önizlemeler; bkz. Editor/GettingStarted.md)
```

## CMakeLists.txt

Şablonun `CMakeLists.txt`'i değiştirilmez: `Game/` altındaki her `.cpp`'yi kendiliğinden bulur ve
`engine_add_game(${PROJECT_NAME} SOURCES ... ASSETS Game/Assets)` ile oyunu kurar. Yeni dosya ekleyince bir sonraki
build CMake'i yeniden yapılandırır; ilk `.cpp` geldiğinde starter oyunun yerini projenin kodu alır.

Aynı dosya `engine_add_tuning(${PROJECT_NAME} ROOT Game)` ile `Game/` altındaki `*.tuning.json` ayar dosyalarını
derlemeye bağlar ([Tuning](Tuning.md)): yarıçap, hız, renk gibi elle ayarlanan sayıları kod yerine orada tutmak,
onları oyun açıkken ya da editörün Tuning panelinden derlemeden değiştirmeyi sağlar.

`engine_add_game` Engine'in `Main.cpp` dosyasını ve Engine link'ini ekler; `CreateGame()` fabrikası projenindir.
Desteklenen argümanlar: `SOURCES`, ek hedefler için `LIBRARIES` (ör. `EngineLocalization`), masaüstünde exe
yanına kopyalanacak klasör için `ASSETS`. Kendi `main` fonksiyonunuzu eklemeyin.

## İlk sahne

`Game/MyScene.h`:

```cpp
#pragma once
#include "Scene/Scene.h"

class MyScene final : public Scene {
public:
    MyScene();
    ~MyScene() override;

private:
    void Enter() override;
    void HandleInput(const DeviceInput& input) override;
    void Update(float seconds) override;
    void Draw() const override;

    float _x = 640.0f;
    int _direction = 0;
};
```

`Game/MyScene.cpp`:

```cpp
#include "MyScene.h"
#include <raylib.h>
#include <algorithm>
#include "Input/InputNavigation.hpp"

namespace {
constexpr float StartX = 640.0f;
constexpr float CenterY = 360.0f;
constexpr float Speed = 240.0f;
constexpr float Radius = 24.0f;
constexpr float CanvasWidth = 1280.0f;
} // namespace

MyScene::MyScene()
    : Scene(TimeDomain::Game) {}
MyScene::~MyScene() { ShutdownNoThrow(); }
void MyScene::Enter() { _x = StartX; }
void MyScene::HandleInput(const DeviceInput& input) { _direction = InputNavigation(input).Horizontal(); }
void MyScene::Update(float seconds) {
    _x = std::clamp(_x + static_cast<float>(_direction) * Speed * seconds, Radius, CanvasWidth - Radius);
    _direction = 0;
}
void MyScene::Draw() const { DrawCircleV({_x, CenterY}, Radius, ORANGE); }
```

`Horizontal()` bu frame basılan A/D veya ok tuşunu bildirir; tuş tekrarı olmayan tek basış
hareketi gösterir. Basılı tutarak hareket için `DeviceInput::Down` sorgularını kullanın.
Bu küçük örnek lifecycle'ı göstermek için state ve çizimi aynı sahnede tutar.
Büyüyen oyunda saf model ve onu const okuyan View ayırın; görsel sabitleri View'da tutun.

## Oyun ve giriş fabrikası

`Game/MyGame.h`:

```cpp
#pragma once
#include "MyScene.h"
#include "Runtime/Game.h"

class MyGame final : public Game {
public:
    MyGame();

private:
    void Start() override;
    void HandleInput(const DeviceInput& input) override;
    MyScene _scene;
};
```

`Game/MyGame.cpp`:

```cpp
#include "MyGame.h"
#include "Input/InputNavigation.hpp"

namespace {
GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.window.title = "My first game";
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    return settings;
}
} // namespace

MyGame::MyGame()
    : Game(Settings()) {
    RegisterScene(_scene);
}
void MyGame::Start() { SetScene(_scene); }
void MyGame::HandleInput(const DeviceInput& input) {
    if (InputNavigation(input).Back())
        RequestQuit();
    else
        Game::HandleInput(input);
}
```

`Game/CreateGame.cpp`:

```cpp
#include <memory>
#include "MyGame.h"
#include "Runtime/EntryPoint.h"

std::unique_ptr<Game> CreateGame() { return std::make_unique<MyGame>(); }
```

`applicationId` kullanıcı verisi klasörünün adıdır (ayarlar, kayıtlar); projeye özgü ve kalıcı seçin.
Sahne oyunun değer üyesidir. `ShutdownNoThrow()` somut sahne destructor'ında çağrılır;
çocuk nesneler eklediğinizde de aynı sahiplik kuralı geçerlidir.

## Geliştiricinin derleyip denemesi

```powershell
python Engine run
```

`python Engine run` MSVC ortamını kurar, `desktop` preset'iyle yapılandırır, Debug derler ve `build/Debug/MyProject.exe`'yi
açar. Elle: Developer PowerShell'de `cmake --preset desktop` ve `cmake --build --preset debug`.

Beklenen: canvas ortasında turuncu daire; A/D veya sol/sağ basışı hareket ettirir, Escape kapatır.

Özel font/ses eklerken [asset rehberini](AssetsAndPlatforms.md), kalıcı veri eklerken
[ayar ve kayıt rehberini](OptionsAndSaves.md) izleyin. Editör entegrasyonu
[ayrı başlangıç rehberindedir](Editor/GettingStarted.md).
