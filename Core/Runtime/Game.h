#pragma once
#include <memory>
#include <optional>
#include "Modules/Persistence/Persistence.h"
#include "Audio/AudioSystem.h"
#include "Options/GameOptions.h"
#include "Fx/TimeScale.h"
#include "Graphics/Render/RenderStats.h"
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Runtime/GameSettings.h"
#include "Runtime/GraphicsSettings.h"
#include "Runtime/FrameExecutionSettings.h"
#include "Utilities/FrameObserver.h"
#include "Utilities/Vec2.h"

class Painter;
class Scene;
class SceneManager;
class Renderer;
class InputNormalizer;
struct Texture;
using Texture2D = Texture;

// Runtime owns the loop and services. A project's derived game supplies content and scenes.
class Game {
public:
    virtual ~Game();
    void Run();
    // Development hosts may borrow frame instrumentation and temporarily override execution without saving options.
    void ObserveFrames(FrameObserver* observer);
    [[nodiscard]] FrameObserver* FrameInstrumentation() const { return _frameObserver; }
    [[nodiscard]] FrameExecutionSettings FrameExecution() const { return _frameExecution; }
    void ConfigureFrameExecution(const FrameExecutionSettings& settings);
    // What the last presented frame cost the renderer (draw calls, uploads, glyphs ...).
    [[nodiscard]] const RenderStats& RenderStatistics() const;
    // Development hosts (profiler) may change the render quality for this session; options are not written.
    void OverrideRenderQuality(const RenderQuality& quality);
    // The graphics choices in effect, an override included.
    [[nodiscard]] GraphicsSettings AppliedGraphics() const;

protected:
    explicit Game(const GameSettings& settings);
    void RegisterScene(Scene& scene);
    void SetScene(Scene& scene, bool restart = false);
    // Cancel the current scene's input and resynchronize local devices before accepting another press.
    void CancelInput();
    void RequestQuit() { _quitRequested = true; }
    // Application pause supplements the current scene's GameTimePaused request.
    void SetPaused(bool paused) { _paused = paused; }
    [[nodiscard]] TimeScale& Time() { return _timeScale; }
    [[nodiscard]] AudioSystem& Audio() { return _audio; }
    [[nodiscard]] const AudioSystem& Audio() const { return _audio; }
    // The game's font (GameSettings::font) for Painter::Text; throws logic_error when the game has none.
    [[nodiscard]] const FontFace& Font() const;
    [[nodiscard]] bool HasFont() const { return _font.has_value(); }
    // FPS limit, display mode and window size are engine options: applied and persisted. 0: unlimited FPS.
    void SetFpsLimit(int fps);
    [[nodiscard]] int FpsLimit() const;
    void SetBorderlessFullscreen(bool enabled);
    [[nodiscard]] bool BorderlessFullscreen() const;
    // Windowed size; {0, 0}: maximized to fill the screen. Stored during borderless fullscreen and applied when
    // returning to windowed mode.
    void SetWindowSize(Vec2 size);
    [[nodiscard]] Vec2 WindowSize() const;
    // Resolution of the monitor containing the window.
    [[nodiscard]] Vec2 MonitorSize() const;
    // Graphics choices (render resolution, VSync, FPS counter, effect intensity) are engine options: applied and
    // persisted. Invalid saved values fall back to defaults.
    void SetGraphics(const GraphicsSettings& graphics);
    [[nodiscard]] GraphicsSettings Graphics() const;
    // The applied effect choice; scenes hand it to their Fx (FxStage::SetQuality and the Fx classes' SetQuality).
    [[nodiscard]] const FxQuality& Effects() const { return _effects; }

    // The options file (settings.json) loads at setup; audio, FPS, display and graphics belong to the engine.
    // The game writes its own keys to Options(); the engine saves changes on scene transitions and after Stop.
    [[nodiscard]] GameOptions& Options() { return _options; }
    [[nodiscard]] const GameOptions& Options() const { return _options; }
    // Writes without waiting for a save point (e.g. a choice that must survive shutdown).
    void SaveOptions();

    [[nodiscard]] std::filesystem::path SavePath(const std::filesystem::path& filename) const;

    [[nodiscard]] Persistence CreatePersistence(const std::filesystem::path& filename,
                                                std::optional<SaveSettings> settings = std::nullopt) const;

    virtual void Start();
    // Host commands and workload resets happen between frames, before instrumentation starts.
    virtual void PrepareFrame();
    [[nodiscard]] virtual DeviceInput ReadInput();
    virtual void HandleInput(const DeviceInput& input);
    virtual void Update(float realSeconds);
    // External hosts may supply simulation time while rendering and communication continue normally.
    [[nodiscard]] virtual float FrameSeconds(float realSeconds);
    // Source of Painter::Time, read before each frame is drawn. Defaults to real time; hosts supply their own
    // pausable clock.
    [[nodiscard]] virtual double DrawSeconds() const;
    // Draws over the current scene every frame.
    virtual void Draw(Painter& paint) const;
    // Completed canvas is borrowed for this call (frame capture).
    virtual void CanvasReady(const Texture2D& texture) const;
    // Draws over the whole window (device pixels) after the canvas was presented.
    virtual void DrawWindow(Painter& window) const;
    // Tools only: native ImGui (ToolUi) over the finished window, after DrawWindow's drawing.
    virtual void DrawToolOverlay() const;
    // Loading screen: after Start, the first scene draws without advancing (no input or Tick) while audio loads
    // in the background; this draws on top. 'seconds' is time since opening; 'outro' is closing progress (0..1).
    virtual void DrawLoading(Painter& paint, float seconds, float outro) const;
    // Loading screen closing duration after loading finishes; 'loadSeconds' is the loading duration.
    [[nodiscard]] virtual float LoadingOutroSeconds(float loadSeconds) const;
    virtual void Stop();

private:
    friend class EngineGodAccessor;
    void CommitScene();
    // Draws the loading screen until loading and its outro finish; false if the window is closed.
    bool ShowLoading();
    void RevealWindow();
    void Present() const;
    void ApplyOptions();
    void ApplyGraphics();
    void DrawFpsCounter(Painter& paint) const;

    // Context first, content/services afterwards; derived game members are destroyed before these resources.
    // Renderer and SceneManager are engine internals (Graphics/Render/Internals, Scene/Internals).
    std::unique_ptr<Renderer> _renderer;
    std::filesystem::path _assetRoot;
    std::optional<FontFace> _font;
    AudioSystem _audio;
    std::unique_ptr<InputNormalizer> _input;
    TimeScale _timeScale;
    std::filesystem::path _applicationId;
    SaveSettings _saveSettings;
    GameOptions _options;
    int _defaultFpsLimit;
    bool _persistentOptions;
    bool _fixedWindow;
    bool _showFps = false;
    FxQuality _effects;
    std::optional<RenderQuality> _renderOverride; // OverrideRenderQuality (development hosts)
    double _frameStart = 0.0;                     // Frame start (GetTime); origin for the FPS counter's work duration
    float _frameSeconds = 0.0f;                   // This frame's real duration (Painter::Delta)
    mutable float _frameWorkSeconds = 0.0f;       // Smoothed update + drawing duration (excluding waits)
    std::unique_ptr<SceneManager> _sceneManager;
    bool _quitRequested = false;
    bool _paused = false;
    bool _ran = false;
    bool _revealed = false; // Initially hidden window was shown after the first frame
    bool _inputFocused = true;
    FrameObserver* _frameObserver = nullptr;
    FrameExecutionSettings _frameExecution;

public:
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;
};
