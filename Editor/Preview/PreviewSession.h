#pragma once
#include <raylib.h>
#include <chrono>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Editor/Preview/PreviewScene.h"
#include "Runtime/GameSettings.h"
#include "Editor/Preview/PreviewProfiler.h"

struct PreviewSessionActions {
    std::function<void()> quit;
    std::function<void()> reset;
    std::function<void(PreviewScene&)> activate;
    std::function<std::string(const std::string&, const PreviewParameters&)> application;
};

// Transport and preview lifecycle, reusable by a project's actual Game without another window or loop.
class PreviewSession {
public:
    PreviewSession(Game& game, const GameSettings& settings, const PreviewRegistry& registry, ControlChannel& channel,
                   const std::string& session, const FontFace* font, AudioSystem& audio, PreviewSessionActions actions);
    ~PreviewSession();
    [[nodiscard]] PreviewScene& Scene() { return _scene; }
    [[nodiscard]] const PreviewScene& Scene() const { return _scene; }
    [[nodiscard]] DeviceInput ReadInput();
    void PrepareFrame();
    void Capture();
    void CanvasReady(const Texture2D& texture) const;
    void Stop();
    [[nodiscard]] float Advance(float realSeconds);

private:
    void Command(const PreviewMessage& message);
    void Reply(const PreviewMessage& message, const std::string& type, nlohmann::json data);
    const PreviewRegistry& _registry;
    ControlChannel& _channel;
    Vec2 _canvas;
    mutable SharedFrames _frames;
    PreviewSessionActions _actions;
    PreviewScene _scene;
    mutable PreviewProfiler _profiler;
    DeviceInput _inputState;
    std::chrono::steady_clock::time_point _lastInput;
    std::uint64_t _generation = 0, _openRequest = 0;
    bool _paused = false, _step = false;
    float _speed = 1.0f;
    mutable bool _readyPending = false;
    mutable double _statsStartedAt = 0, _inspectionAt = -1;
    mutable std::uint64_t _publishedFrames = 0;
    mutable PreviewInspection _inspection;
};
