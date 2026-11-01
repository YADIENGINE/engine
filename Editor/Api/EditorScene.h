#pragma once
#include <string>
#include "Editor/Api/SceneParameter.h"
#include "Editor/Api/SceneInspection.h"
#include "GameObjects/GameObject.h"

struct DeviceInput;

// Start/Update/Draw/Stop are dispatched exclusively by the existing ObjectList lifecycle.
class EditorScene : public GameObject {
public:
    ~EditorScene() override;
    virtual void HandleInput(const DeviceInput& input);
    // Called for Immediate/NextAction values. NextAction setters stage values for the next action.
    // Return an explanation to reject a change; rejected setters must leave content unchanged.
    [[nodiscard]] virtual std::string ApplyParameter(const std::string& id, const SceneValue& value);
    [[nodiscard]] virtual std::string ExecuteAction(const std::string& id);
    // Report authored controls changed by an action or content interaction; omit transient animation state.
    [[nodiscard]] virtual SceneParameters CaptureParameters() const;
    [[nodiscard]] virtual SceneInspection Inspect() const;

protected:
    EditorScene();
};
