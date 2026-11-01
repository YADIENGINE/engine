#pragma once
#include <optional>
#include "Audio/AudioFiles.h"
#include "Editor/Application/EditorControlService.h"
#include "Editor/Application/EditorController.h"
#include "Editor/Host/AssetCommandRunner.h"
#include "Editor/Host/AssetThumbnailStore.h"
#include "Editor/Host/ViewportDisplay.h"
#include "Editor/Internals/Settings/EditorSettingsStore.h"
#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include "Input/Internals/InputSample.h"
#include "Runtime/Game.h"
#include "ToolUi/ToolUiContext.h"

class EditorApplication : public Game {
public:
    explicit EditorApplication(const std::filesystem::path& projectFile);
    ~EditorApplication() override;

protected:
    [[nodiscard]] DeviceInput ReadInput() override;
    void Update(float realSeconds) override;
    void DrawToolOverlay() const override;
    void Stop() override;

private:
    void SavePreferences(bool force);
    // Saves the marked region of the latest frame and pastes its context line into the Console.
    void PasteMark(const FloatRect& region);
    void PasteWindowMark(const FloatRect& region, const ViewportPixels& window);
    // Agent → Send scene: saves the latest frame and sends the scene's context line to the agent session.
    void SendScene();
    void HandleAudioPreview(const AudioPreviewRequest& request);
    // Handled in the next Update; returns the AudioPreviewState::handled count it brings.
    std::uint64_t QueueAudio(const AudioPreviewRequest& request) const;
    // Window captures: requested for the end of the next drawn frame, then kept as the latest.
    [[nodiscard]] std::uint64_t RequestWindow() const;
    [[nodiscard]] std::shared_ptr<const ViewportPixels> Window(std::uint64_t sequence) const;
    // All GPU/UI resources are owned after the base Game has created its window.
    mutable EditorController _controller;
    std::optional<EditorControlService> _control; // Tools reach this editor through it (`python Engine refresh`)
    mutable ToolUiContext _ui;
    mutable ViewportDisplay _display;
    mutable AssetThumbnailStore _thumbnails;
    mutable AssetCommandRunner _assetCommands;
    mutable EditorWorkspace _workspace;
    mutable EditorPreferences _preferences;
    std::optional<EditorSettingsStore> _settings;
    EditorPreferences _observedPreferences;
    std::string _projectKey;
    std::filesystem::path _launcher; // "Back to project list"; empty without a launcher
    InputSample _windowInput;
    mutable ViewportInputRouter _router;
    mutable int _fontSize = 100;
    int _observedFontSize = 100;
    double _saveAt = 0;
    bool _dirty = false;
    mutable bool _quit = false;
    mutable std::optional<FloatRect> _mark; // Drawn this frame, handled in the next Update
    mutable std::optional<FloatRect> _windowMark;
    mutable bool _sendScene = false; // Requested this frame, handled in the next Update
    mutable std::string _agentProblem;
    mutable std::vector<AudioPreviewRequest> _audioRequests; // Made while drawing, handled in the next Update
    mutable std::uint64_t _audioQueued = 0;
    AudioPreviewState _audio;
    AudioWaveformCache _waveforms{DecodeAudioFile};
    std::uint64_t _windowMarkSequence = 0; // The capture it waits for; 0 until requested
    mutable bool _windowWanted = false;
    mutable std::uint64_t _windowSequence = 0;
    mutable std::shared_ptr<const ViewportPixels> _window;
    bool _stopped = false;
    std::optional<FrameExecutionSettings> _beforeBenchmark;
};
