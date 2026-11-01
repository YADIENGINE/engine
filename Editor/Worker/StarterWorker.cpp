#include <memory>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Api/SceneServices.h"
#include "Editor/Worker/WorkerHost.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr int TitleSize = 40;
constexpr int HintSize = 22;
constexpr int LineGap = 56;
constexpr Color4 TitleColor{245, 245, 245, 255};
constexpr Color4 HintColor{130, 130, 130, 255};

// engine_add_editor_worker builds this worker while the project's Editor/ has no sources; its one entry shows the
// scene pipeline works end to end.
class StarterWorker final : public EditorScene {
private:
    void Draw(Painter& paint) const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        paint.Text("Editor/ has no sources yet", {CenterX, CenterY - LineGap}, CenteredText(TitleSize, TitleColor));
        paint.Text("Add scenes: Engine/Docs/Editor/GettingStarted.md", {CenterX, CenterY},
                   CenteredText(HintSize, HintColor));
    }
};

SceneDefinition StarterDefinition() {
    SceneDefinition definition;
    definition.id = "engine.starter";
    definition.name = "Starter";
    definition.category = "Engine";
    definition.description = "Placeholder until the project registers its own scenes.";
    definition.create = [](const SceneServices&, const SceneParameters&) { return std::make_unique<StarterWorker>(); };
    return definition;
}
} // namespace

int main(int argc, char** argv) {
    GameSettings settings;
    settings.applicationId = ENGINE_NAME "StarterWorker";
    settings.canvasWidth = CanvasWidth;
    settings.canvasHeight = CanvasHeight;
    settings.persistentOptions = false;
    settings.font = "Fonts/Starter.ttf";
    settings.assets = {settings.font};
    return RunEditorWorker(argc, argv, [](SceneRegistry& registry) { registry.Add(StarterDefinition()); }, settings);
}
