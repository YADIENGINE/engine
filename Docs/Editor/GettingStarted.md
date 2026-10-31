# First editor preview

Complete [project setup](../ProjectSetup.md) first; the [first game](../GettingStarted.md) is not required. The editor
runs on Windows desktop. The project's `Editor/` folder is a sibling of Engine and Game; `Editor/Api/...` includes refer
to engine public headers. The `Editor/` folder created by setup is empty; `python Engine editor` then opens with the
engine's starter worker (Engine → Starter preview).

## CMake target

With `APP_BUILD_EDITOR` enabled (`python Engine editor` supplies it automatically), the project's `CMakeLists.txt` finds
every `.cpp` under `Editor/` and creates the worker:

```cmake
engine_add_editor_worker(${PROJECT_NAME}Preview SOURCES ${APP_EDITOR_SOURCES})
target_include_directories(${PROJECT_NAME}Preview PRIVATE Editor)
```

The helper places the worker executable beside EngineEditor (`<build>/Editor/<Config>/`), links `EnginePreviewRuntime`,
and generates the `EngineEditor.project.json` recipe in the same folder: the worker path and the `cmake --build` command
for Build & Refresh. Arguments: `NAME` (catalog title; default `<project> previews`), `SOURCES` (starter worker if empty),
`LIBRARIES`, `ASSETS` (folder to copy beside the worker). One worker per build folder.

If previews draw game Views, move game code to a separate library (`add_library(... Game/...)`) and pass it to both game
and worker through `LIBRARIES`; the worker does not compile the game's `CreateGame.cpp`.

EngineEditor has its own `main`; the worker also uses its own `main` calling `RunPreviewHost`. Do not create the worker
with `engine_add_game`, because that helper adds the game's entry point.

## Editor/FirstPreview.h

```cpp
#pragma once
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewRegistry.h"

class FirstPreview final : public EditorPreview {
public:
    explicit FirstPreview(const PreviewParameters& parameters);
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const PreviewValue& value) override;

private:
    void Draw(Painter& paint) const override;
    float _radius;
};

[[nodiscard]] PreviewDefinition FirstPreviewDefinition();
```

## Editor/FirstPreview.cpp

```cpp
#include "FirstPreview.h"
#include <cmath>
#include <memory>
#include <variant>
#include "Graphics/Paint/Painter.h"

FirstPreview::FirstPreview(const PreviewParameters& parameters)
    : _radius(static_cast<float>(std::get<double>(parameters.at("radius")))) {}

std::string FirstPreview::ApplyParameter(const std::string& id, const PreviewValue& value) {
    if (id != "radius") return "Unknown parameter";
    _radius = static_cast<float>(std::get<double>(value));
    return {};
}

void FirstPreview::Draw(Painter& paint) const {
    const float phase = static_cast<float>(paint.Time());
    paint.Circle({640.0f, 360.0f}, _radius, Fill(WithAlpha(Rgb(0xffa500), 0.6f + 0.4f * std::sin(phase))));
}

PreviewDefinition FirstPreviewDefinition() {
    PreviewDefinition definition;
    definition.id = "myproject.first";
    definition.name = "First preview";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", PreviewParameterKind::Float, 80.0, 8.0, 200.0}};
    definition.create = [](const PreviewServices&, const PreviewParameters& parameters) {
        return std::make_unique<FirstPreview>(parameters);
    };
    return definition;
}
```

You can call the real game's View instead of the example's fixed center and colors. Metadata registration defines only a
factory; it does not draw or load assets.

## Editor/Main.cpp

```cpp
#include "FirstPreview.h"
#include "Editor/Preview/PreviewHost.h"

int main(int argc, char** argv) {
    GameSettings settings;
    settings.applicationId = "MyProjectPreview";
    settings.canvasWidth = 1280;
    settings.canvasHeight = 720;
    settings.persistentOptions = false;
    return RunPreviewHost(
        argc, argv, [](PreviewRegistry& registry) { registry.Add(FirstPreviewDefinition()); }, settings);
}
```

`RunPreviewHost` creates one Game loop with the default `PreviewGame`. The window is hidden/fixed; dimensions come from
the canvas, and the worker targets 30 FPS. The canvas must be positive and at most 1920×1080. If examples use real
fonts/audio, supply a GameSettings manifest/catalog and copy the asset folder beside the worker executable with
`ASSETS`. This copy is independent of the game target.

## Running as a developer

```powershell
python Engine editor
```

`python Engine editor` configures the separate build folder in the `Project.json` "editor" section (template:
`build/editor`), builds `EngineEditor` and the worker, and opens the editor with `EngineEditor.project.json`.

MyProject → Drawing → First preview opens; Radius changes; Pause freezes the glow clock; Step advances one step; Reset
resets the clock.

Do not start the worker directly: the editor supplies session/IPC arguments. For missing executables, build both targets
first; Build & Refresh errors appear in Output and the worker does not open. If production scenes require a custom Game
adapter, follow the PreviewSession contract in [CreatingPreviews](CreatingPreviews.md) and [API](Api.md).
