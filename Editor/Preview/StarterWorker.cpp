#include <raylib.h>
#include <memory>
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Api/PreviewServices.h"
#include "Editor/Preview/PreviewHost.h"
#include "Graphics/Draw/TextRenderer.h"

namespace {
constexpr int CanvasWidth = 1280;
constexpr int CanvasHeight = 720;
constexpr int TitleSize = 40;
constexpr int HintSize = 22;
constexpr int LineGap = 56;

// engine_add_editor_worker builds this worker while the project's Editor/ has no sources; its one entry shows the
// preview pipeline works end to end.
class StarterPreview final : public EditorPreview {
public:
    explicit StarterPreview(const TextRenderer& text)
        : _text(text) {}

private:
    void Draw() const override {
        constexpr int CenterX = CanvasWidth / 2;
        constexpr int CenterY = CanvasHeight / 2;
        _text.DrawCentered("Editor/ has no sources yet", CenterX, CenterY - LineGap, TitleSize, RAYWHITE);
        _text.DrawCentered("Add previews: Engine/Docs/Editor/GettingStarted.md", CenterX, CenterY, HintSize, GRAY);
    }

    const TextRenderer& _text;
};

PreviewDefinition StarterDefinition() {
    PreviewDefinition definition;
    definition.id = "engine.starter";
    definition.name = "Starter";
    definition.category = "Engine";
    definition.description = "Placeholder until the project registers its own previews.";
    definition.create = [](const PreviewServices& services, const PreviewParameters&) {
        return std::make_unique<StarterPreview>(services.text);
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
    return RunPreviewHost(argc, argv, [](PreviewRegistry& registry) { registry.Add(StarterDefinition()); }, settings);
}
