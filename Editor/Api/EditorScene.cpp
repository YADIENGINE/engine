#include "Editor/Api/EditorScene.h"

EditorScene::EditorScene() = default;
EditorScene::~EditorScene() = default;
void EditorScene::HandleInput(const DeviceInput& input) { (void)input; }
std::string EditorScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    (void)value;
    return "Unsupported parameter: " + id;
}
std::string EditorScene::ExecuteAction(const std::string& id) { return "Unsupported action: " + id; }
SceneParameters EditorScene::CaptureParameters() const { return {}; }
SceneInspection EditorScene::Inspect() const { return {}; }
