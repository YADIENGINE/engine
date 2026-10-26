#include "Editor/Api/EditorPreview.h"

EditorPreview::EditorPreview() = default;
EditorPreview::~EditorPreview() = default;
void EditorPreview::HandleInput(const DeviceInput& input) { (void)input; }
std::string EditorPreview::ApplyParameter(const std::string& id, const PreviewValue& value) {
    (void)value;
    return "Unsupported parameter: " + id;
}
std::string EditorPreview::ExecuteAction(const std::string& id) { return "Unsupported action: " + id; }
PreviewParameters EditorPreview::CaptureParameters() const { return {}; }
PreviewInspection EditorPreview::Inspect() const { return {}; }
