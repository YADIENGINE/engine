# First editor scene

Complete [project setup](../ProjectSetup.md) first; the [first game](../GettingStarted.md) is not required. The editor
runs on Windows desktop. The project's `Editor/` folder is a sibling of Engine and Game; `Editor/Api/...` includes refer
to engine public headers. Setup creates `Editor/Work/` for [task scenes](../../AgentDocs/Recipes/WorkScene.md); until
worker sources are added, `python Engine editor` opens with the
engine's starter worker (Engine → Starter scene).

## CMake target

With `APP_BUILD_EDITOR` enabled (`python Engine editor` supplies it automatically), the project's `CMakeLists.txt` finds
every `.cpp` under `Editor/` and creates the worker:

```cmake
engine_add_editor_worker(${PROJECT_NAME}EditorWorker SOURCES ${APP_EDITOR_SOURCES} SCENE_DIRS Editor/Work)
target_include_directories(${PROJECT_NAME}EditorWorker PRIVATE Editor)
```

The helper places the worker executable beside EngineEditor (`<build>/Editor/<Config>/`), links `EngineWorkerRuntime`,
and generates the `EngineEditor.project.json` recipe in the same folder: the worker path and the `cmake --build` command
for Build & Refresh. Arguments: `NAME` (catalog title; default `<project> scenes`), `SOURCES` (starter worker if empty),
`LIBRARIES`, `ASSETS` (folder to copy beside the worker, also the Assets panel's project root), `SCENE_DIRS`
(directories of auto-registered `*.cpp` scenes, linked as OBJECT libraries; excluded from `SOURCES`), `GRAPHIC_SOURCES`
(drawing code under `Assets/Graphics` that no library in `LIBRARIES` holds). Paths are relative to the calling CMake directory.
One worker per build folder. Build & Refresh also picks up newly added scene files.

If scenes draw game Views, move game code to a separate library (`add_library(... Game/...)`) and pass it to both game
and worker through `LIBRARIES`; the worker does not compile the game's `CreateGame.cpp`.

EngineEditor has its own `main`; the worker also uses its own `main` calling `RunEditorWorker`. Do not create the worker
with `engine_add_game`, because that helper adds the game's entry point.

## Editor/FirstScene.h

```cpp
#pragma once
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneRegistry.h"

class FirstScene final : public EditorScene {
public:
    explicit FirstScene(const SceneParameters& parameters);
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;

private:
    void Draw(Painter& paint) const override;
    float _radius;
};

[[nodiscard]] SceneDefinition FirstSceneDefinition();
```

## Editor/FirstScene.cpp

```cpp
#include "FirstScene.h"
#include <cmath>
#include <memory>
#include <variant>
#include "Graphics/Paint/Painter.h"

FirstScene::FirstScene(const SceneParameters& parameters)
    : _radius(static_cast<float>(std::get<double>(parameters.at("radius")))) {}

std::string FirstScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id != "radius") return "Unknown parameter";
    _radius = static_cast<float>(std::get<double>(value));
    return {};
}

void FirstScene::Draw(Painter& paint) const {
    const float phase = static_cast<float>(paint.Time());
    paint.Circle({640.0f, 360.0f}, _radius, Fill(WithAlpha(Rgb(0xffa500), 0.6f + 0.4f * std::sin(phase))));
}

SceneDefinition FirstSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.first";
    definition.name = "First scene";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", SceneParameterKind::Float, 80.0, 8.0, 200.0}};
    definition.create = [](const SceneServices&, const SceneParameters& parameters) {
        return std::make_unique<FirstScene>(parameters);
    };
    return definition;
}
```

You can call the real game's View instead of the example's fixed center and colors. Metadata registration defines only a
factory; it does not draw or load assets.

## Editor/Main.cpp

```cpp
#include "FirstScene.h"
#include "Editor/Worker/WorkerHost.h"

int main(int argc, char** argv) {
    GameSettings settings;
    settings.applicationId = "MyProjectWorker";
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    settings.persistentOptions = false;
    return RunEditorWorker(
        argc, argv, [](SceneRegistry& registry) { registry.Add(FirstSceneDefinition()); }, settings);
}
```

`RunEditorWorker` creates one Game loop with the default `WorkerGame`. The window is hidden/fixed; dimensions come from
the canvas, and the worker targets 30 FPS. The canvas must be positive and at most 1920×1080. If examples use real
fonts/audio, supply a GameSettings manifest/catalog and copy the asset folder beside the worker executable with
`ASSETS`. This copy is independent of the game target.

## Running as a developer

```powershell
python Engine editor
```

`python Engine editor` configures the separate build folder in the `Project.json` "editor" section (template:
`build/editor`), builds `EngineEditor` and the worker, and opens the editor with `EngineEditor.project.json`.

MyProject → Drawing → First scene opens; Radius changes; Pause freezes the glow clock; Step advances one step; Reset
resets the clock.

Do not start the worker directly: the editor supplies session/IPC arguments. For missing executables, build both targets
first; Build & Refresh errors appear in Output and the worker does not open. If production scenes require a custom Game
adapter, follow the WorkerSession contract in [CreatingScenes](CreatingScenes.md) and [API](Api.md).
