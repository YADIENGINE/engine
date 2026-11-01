# First game

First set up the project with `python Engine new` as described in [Project setup](ProjectSetup.md). The `Game/` folder
created by setup is empty; `python Engine run` in that state opens the engine's starter game ("Game/ has no sources
yet"). The starter copies the Noto Sans font and its license beside the executable for Painter text. This guide writes
the first small game under `Game/`.

## File structure

```text
MyProject/
  CMakeLists.txt  CMakePresets.json  Project.json
  Engine/                   (submodule)
  Game/
    Assets/
    CreateGame.cpp
    MyGame.h  MyGame.cpp
    MyScene.h  MyScene.cpp
  Editor/                   (editor scenes; see Editor/GettingStarted.md)
```

## CMakeLists.txt

Leave the template's `CMakeLists.txt` unchanged: it automatically finds every `.cpp` under `Game/` and creates the game
with `engine_add_game(${PROJECT_NAME} SOURCES ... ASSETS Game/Assets)`. After adding a file, the next build reconfigures
CMake; the project's code replaces the starter game when the first `.cpp` is added.

The same file connects `*.tuning.json` settings files under `Game/` to the build with `engine_add_tuning(${PROJECT_NAME}
ROOT Game)` ([Tuning](Tuning.md)): keeping manually adjusted numbers such as radius, speed, and color there instead of
in code lets you change them while the game runs or from the editor's Tuning panel without building.

`engine_add_game` adds the engine's `Main.cpp` and Engine link; the project owns the `CreateGame()` factory. Supported
arguments: `SOURCES`, `LIBRARIES` for extra targets (e.g. `EngineLocalization`), and `ASSETS` for the folder to copy
beside the desktop executable. Do not add your own `main` function.

## First scene

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
    void Draw(Painter& paint) const override;

    float _x = 640.0f;
    int _direction = 0;
};
```

`Game/MyScene.cpp`:

```cpp
#include "MyScene.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Input/InputNavigation.h"

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
void MyScene::Draw(Painter& paint) const { paint.Circle({_x, CenterY}, Radius, Fill(Rgb(0xffa500))); }
```

`Horizontal()` reports A/D or arrow keys pressed this frame; it demonstrates movement on a single press without key
repeat. For movement while held, use `DeviceInput::Down` queries. This small example keeps state and drawing in the same
scene to demonstrate lifecycle. As the game grows, separate a pure model and a View that reads it as const; keep visual
constants in the View.

## Game and entry factory

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
#include "Input/InputNavigation.h"

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

`applicationId` is the user-data folder name (options, saves); choose a persistent, project-specific value. The scene is
a value member of the game. Call `ShutdownNoThrow()` in the concrete scene's destructor; the same ownership rule applies
when adding child objects.

## Building and trying it as a developer

```powershell
python Engine run
```

`python Engine run` sets up the MSVC environment, configures with the `desktop` preset, builds Debug, and opens
`build/Debug/MyProject.exe`. Manually: `cmake --preset desktop` and `cmake --build --preset debug` in Developer
PowerShell.

Expected: an orange circle in the canvas center; A/D or left/right presses move it, Escape closes it.

Follow the [asset guide](AssetsAndPlatforms.md) when adding custom fonts/audio, and the [options and saves
guide](OptionsAndSaves.md) when adding persistent data. Editor integration has a [separate getting-started
guide](Editor/GettingStarted.md).
