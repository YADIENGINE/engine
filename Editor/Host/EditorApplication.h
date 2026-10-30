#pragma once
#include <optional>
#include "Editor/Application/EditorControlService.h"
#include "Editor/Application/EditorController.h"
#include "Editor/Host/PreviewDisplay.h"
#include "Editor/Host/EditorUiContext.h"
#include "Editor/Internals/Settings/EditorSettingsStore.h"
#include "Editor/Internals/Viewport/PreviewInputRouter.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "Input/Internals/InputSample.h"
#include "Runtime/Game.h"

class EditorApplication : public Game {
public:
    explicit EditorApplication(const std::filesystem::path& projectFile);
    ~EditorApplication() override;

protected:
    [[nodiscard]] DeviceInput ReadInput() override;
    void Update(float realSeconds) override;
    void DrawWindow() const override;
    void Stop() override;

private:
    void SavePreferences(bool force);
    // All GPU/UI resources are owned after the base Game has created its window.
    mutable EditorController _controller;
    std::optional<EditorControlService> _control; // Tools reach this editor through it (`python Engine refresh`)
    mutable EditorUiContext _ui;
    mutable PreviewDisplay _display;
    mutable EditorWorkspace _workspace;
    mutable EditorPreferences _preferences;
    std::optional<EditorSettingsStore> _settings;
    EditorPreferences _observedPreferences;
    std::string _projectKey;
    InputSample _windowInput;
    mutable PreviewInputRouter _router;
    mutable int _fontSize = 100;
    int _observedFontSize = 100;
    double _saveAt = 0;
    bool _dirty = false;
    mutable bool _quit = false;
    bool _stopped = false;
    std::optional<FrameExecutionSettings> _beforeBenchmark;
};
