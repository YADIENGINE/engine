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
