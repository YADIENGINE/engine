#include <memory>
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Api/PreviewServices.h"
#include "Editor/Preview/PreviewHost.h"
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
// preview pipeline works end to end.
class StarterPreview final : public EditorPreview {
private:
    void Draw(Painter& paint) const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        paint.Text("Editor/ has no sources yet", {CenterX, CenterY - LineGap}, CenteredText(TitleSize, TitleColor));
        paint.Text("Add previews: Engine/Docs/Editor/GettingStarted.md", {CenterX, CenterY},
                   CenteredText(HintSize, HintColor));
    }
};

PreviewDefinition StarterDefinition() {
    PreviewDefinition definition;
    definition.id = "engine.starter";
    definition.name = "Starter";
    definition.category = "Engine";
    definition.description = "Placeholder until the project registers its own previews.";
    definition.create = [](const PreviewServices&, const PreviewParameters&) {
        return std::make_unique<StarterPreview>();
    };
    return definition;
}
} // namespace

int main(int argc, char** argv) {
    GameSettings settings;
    settings.applicationId = ENGINE_NAME "StarterPreview";
    settings.canvasWidth = CanvasWidth;
    settings.canvasHeight = CanvasHeight;
    settings.persistentOptions = false;
    settings.font = "Fonts/Starter.ttf";
    settings.assets = {settings.font};
    return RunPreviewHost(argc, argv, [](PreviewRegistry& registry) { registry.Add(StarterDefinition()); }, settings);
}
