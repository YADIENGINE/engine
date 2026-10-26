#pragma once
#include <string>
#include "Editor/Api/PreviewParameter.h"
#include "Editor/Api/PreviewInspection.h"
#include "GameObjects/GameObject.h"

struct DeviceInput;

// Start/Update/Draw/Stop are dispatched exclusively by the existing ObjectList lifecycle.
class EditorPreview : public GameObject {
public:
    ~EditorPreview() override;
    virtual void HandleInput(const DeviceInput& input);
    // Called for Immediate/NextAction values. NextAction setters stage values for the next action.
    // Return an explanation to reject a change; rejected setters must leave content unchanged.
    [[nodiscard]] virtual std::string ApplyParameter(const std::string& id, const PreviewValue& value);
    [[nodiscard]] virtual std::string ExecuteAction(const std::string& id);
    // Report authored controls changed by an action or content interaction; omit transient animation state.
    [[nodiscard]] virtual PreviewParameters CaptureParameters() const;
    [[nodiscard]] virtual PreviewInspection Inspect() const;

protected:
    EditorPreview();
};
