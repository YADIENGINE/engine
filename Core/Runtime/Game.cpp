#include "Runtime/Game.h"
#include <raylib.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include "Platform/Internals/ApplicationFiles.h"
#include "Graphics/Render/Internals/Renderer.h"
#include "Scene/Internals/SceneManager.h"
#include "Scene/Scene.h"
#include "Runtime/Internals/StartupTimer.h"
#include "Input/Internals/InputNormalizer.h"
#include "Input/Internals/InputReader.h"
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/DrawTime.h"
#include "Graphics/Style/Theme.h"
#include "Tuning/TuningLive.h"

namespace {
std::vector<std::filesystem::path> AssetManifest(const GameSettings& settings) {
    AudioSystem::ValidateCatalog(settings.audio);
    auto assets = settings.assets;
    for (const auto& effect : settings.audio.effects)
        assets.push_back(effect.file);
    for (const auto& music : settings.audio.music)
        assets.push_back(music.file);
    return assets;
}
constexpr const char* OptionsFile = "settings.json";
constexpr std::int64_t MaxWindowSide = 1 << 15;

// FPS sayacı: sol üst köşede koyu zemin üstünde küçük yazı.
constexpr int FpsCounterX = 8;
constexpr int FpsCounterY = 8;
constexpr int FpsCounterFontSize = 20;
constexpr int FpsCounterPadding = 6;
constexpr float FpsCounterSmoothing = 0.1f; // Her karede iş süresinin yeni ölçüme yaklaşma oranı
constexpr Color FpsCounterBackground{0, 0, 0, 170};
constexpr Color FpsCounterText{120, 255, 160, 255};

EffectLevel StoredLevel(std::int64_t stored) {
    return stored >= 0 && stored <= static_cast<std::int64_t>(EffectLevel::Off) ? static_cast<EffectLevel>(stored)
                                                                                : EffectLevel::Full;
}
} // namespace

Game::Game(const GameSettings& settings)
    : _renderer(std::make_unique<Renderer>(settings.window, settings.canvasWidth, settings.canvasHeight,
                                           settings.background, settings.canvasTextureFilter, settings.letterbox))
    , _assetRoot(PrepareAssets(AssetManifest(settings)))
    , _text(settings.font.empty() ? std::filesystem::path{} : _assetRoot / settings.font, settings.fontAtlasSizes,
            settings.fontCodepoints, settings.fontTextureFilter)
    , _audio(_assetRoot, settings.audioVolume)
    , _input(std::make_unique<InputNormalizer>())
    , _applicationId(settings.applicationId)
    , _saveSettings(settings.save)
    , _options(SaveFilePath(settings.applicationId, OptionsFile))
    , _defaultFpsLimit(settings.window.targetFps)
    , _persistentOptions(settings.persistentOptions)
    , _fixedWindow(settings.fixedWindow)
    , _sceneManager(std::make_unique<SceneManager>()) {
    _frameExecution.fpsLimit = settings.window.targetFps;
    SetTheme(settings.theme);            // Türeyen oyunun View üyeleri (UiButton) kurulurken tema etkin olmalı
    _audio.BeginLoading(settings.audio); // Run'daki yükleme ekranı bitişini bekler
    const StartupTimer timer(_persistentOptions ? "Options (load + apply)" : "Transient settings");
    if (_persistentOptions) {
        _options.Load();
        ApplyOptions();
    } else {
        SetEffectQuality({});
    }
}
Game::~Game() = default;
void Game::ObserveFrames(FrameObserver* observer) {
    _frameObserver = observer;
    _renderer->SetObserver(observer);
}
void Game::Start() {}
void Game::PrepareFrame() {}
DeviceInput Game::ReadInput() {
    // External hosts override this method; their hidden windows do not own input focus.
    const bool focused = IsWindowFocused();
    if (!focused) {
        if (_inputFocused) CancelInput();
        _input->Cancel();
    }
    _inputFocused = focused;
    return _input->Read(ReadInputDevices(), [this](Float2 point) {
        return _renderer->ToCanvas({static_cast<int>(point.x), static_cast<int>(point.y)});
    });
}
void Game::CanvasReady(const Texture2D& texture) const { (void)texture; }
void Game::DrawWindow() const {}
void Game::HandleInput(const DeviceInput& input) {
    if (Scene* scene = _sceneManager->Current()) scene->ReadInput(input);
}
void Game::Update(float realSeconds) { (void)realSeconds; }
float Game::FrameSeconds(float realSeconds) { return realSeconds; }
double Game::DrawSeconds() const { return GetTime(); }
void Game::Draw() const {}
void Game::DrawLoading(float seconds, float outro) const {
    (void)seconds;
    (void)outro;
}
float Game::LoadingOutroSeconds(float loadSeconds) const {
    (void)loadSeconds;
    return 0.0f;
}
void Game::Stop() {}

void Game::SetFpsLimit(int fps) {
    if (!_fixedWindow) {
        _renderer->SetTargetFps(fps);
        _frameExecution.fpsLimit = fps;
    }
    _options.Set(EngineOption::FpsLimit, fps);
}
int Game::FpsLimit() const {
    const auto fps = _options.Get(EngineOption::FpsLimit, _defaultFpsLimit);
    return fps < 0 || fps > std::numeric_limits<int>::max() ? _defaultFpsLimit : static_cast<int>(fps);
}
void Game::SetBorderlessFullscreen(bool enabled) {
    if (!_fixedWindow) _renderer->SetBorderlessFullscreen(enabled);
    _options.Set(EngineOption::BorderlessFullscreen, enabled ? 1 : 0);
    // Pencereye dönünce seçili boyut yeniden uygulanır.
    if (!enabled && !_fixedWindow) _renderer->SetWindowSize(WindowSize());
}
bool Game::BorderlessFullscreen() const { return _options.Get(EngineOption::BorderlessFullscreen, 0) == 1; }
void Game::SetWindowSize(Vec2 size) {
    if (size.x <= 0 || size.y <= 0) size = {};
    if (!BorderlessFullscreen() && !_fixedWindow) _renderer->SetWindowSize(size);
    _options.Set(EngineOption::WindowWidth, size.x);
    _options.Set(EngineOption::WindowHeight, size.y);
}
Vec2 Game::WindowSize() const {
    const auto width = _options.Get(EngineOption::WindowWidth, 0);
    const auto height = _options.Get(EngineOption::WindowHeight, 0);
    if (width <= 0 || height <= 0 || width > MaxWindowSide || height > MaxWindowSide) return {};
    return {static_cast<int>(width), static_cast<int>(height)};
}
Vec2 Game::MonitorSize() const { return _renderer->MonitorSize(); }
void Game::SetGraphics(const GraphicsSettings& graphics) {
    _options.Set(EngineOption::RenderHeight, graphics.renderHeight);
    _options.Set(EngineOption::VSync, graphics.vsync ? 1 : 0);
    _options.Set(EngineOption::ShowFps, graphics.showFps ? 1 : 0);
    _options.Set(EngineOption::Glow, static_cast<std::int64_t>(graphics.effects.glow));
    _options.Set(EngineOption::ParticlePercent, graphics.effects.particlePercent);
    _options.Set(EngineOption::Shake, static_cast<std::int64_t>(graphics.effects.shake));
    _options.Set(EngineOption::SprintEffects, graphics.effects.sprintEffects ? 1 : 0);
    _options.Set(EngineOption::Flashes, graphics.effects.flashes ? 1 : 0);
    _options.Set(EngineOption::BackgroundEffects, graphics.effects.backgroundEffects ? 1 : 0);
    ApplyGraphics();
}
GraphicsSettings Game::Graphics() const {
    const auto renderHeight = _options.Get(EngineOption::RenderHeight, 0);
    const auto particles = _options.Get(EngineOption::ParticlePercent, 100);
    return {.renderHeight = renderHeight > 0 && renderHeight <= MaxWindowSide ? static_cast<int>(renderHeight) : 0,
            .vsync = _options.Get(EngineOption::VSync, 0) == 1,
            .showFps = _options.Get(EngineOption::ShowFps, 0) == 1,
            .effects = {.glow = StoredLevel(_options.Get(EngineOption::Glow, 0)),
                        .particlePercent = particles >= 0 && particles <= 100 ? static_cast<int>(particles) : 100,
                        .shake = StoredLevel(_options.Get(EngineOption::Shake, 0)),
                        .sprintEffects = _options.Get(EngineOption::SprintEffects, 1) == 1,
                        .flashes = _options.Get(EngineOption::Flashes, 1) == 1,
                        .backgroundEffects = _options.Get(EngineOption::BackgroundEffects, 1) == 1}};
}
void Game::ApplyGraphics() {
    const GraphicsSettings graphics = Graphics();
    _renderer->SetMaxRenderHeight(_fixedWindow ? 0 : graphics.renderHeight);
    _renderer->SetVSync(graphics.vsync);
    _frameExecution.vsync = graphics.vsync;
    SetEffectQuality(graphics.effects);
    _showFps = graphics.showFps;
}

void Game::ApplyOptions() {
    _renderer->SetTargetFps(FpsLimit());
    _frameExecution.fpsLimit = FpsLimit();
    _renderer->SetBorderlessFullscreen(BorderlessFullscreen());
    if (!BorderlessFullscreen()) _renderer->SetWindowSize(WindowSize());
    ApplyGraphics();
    _audio.LoadVolumeSettings(_options);
}
void Game::SaveOptions() {
    if (!_persistentOptions) return;
    _audio.StoreVolumeSettings(_options);
    _options.Save();
}

std::filesystem::path Game::SavePath(const std::filesystem::path& filename) const {
    return SaveFilePath(_applicationId, filename);
}
Persistence Game::CreatePersistence(const std::filesystem::path& filename, std::optional<SaveSettings> settings) const {
    auto options = settings.value_or(_saveSettings);
    if (options.encryption == SaveEncryptionMode::Encrypted) {
        if (options.keys.file.empty()) options.keys.file = SavePath("keys/save.key");
    }
    return Persistence(SavePath(filename), std::move(options));
}

void Game::RegisterScene(Scene& scene) { _sceneManager->Register(scene); }
void Game::SetScene(Scene& scene, bool restart) { _sceneManager->Request(scene, restart); }
void Game::CancelInput() {
    _input->Cancel();
    if (Scene* scene = _sceneManager->Current()) scene->CancelInput();
}
void Game::CommitScene() {
    if (_sceneManager->Commit([this] { _input->Cancel(); }))
        SaveOptions(); // Sahne geçişi tercihlerin kayıt noktasıdır (ör. Options ekranından çıkış)
}
void Game::Present() const {
    SetDrawTime(DrawSeconds());
    auto frame = _renderer->BeginFrame();
    if (const Scene* scene = _sceneManager->Current()) scene->Render();
    Draw();
    if (_showFps) DrawFpsCounter();
    frame.Complete([this](const Texture2D& texture) { CanvasReady(texture); }, [this] { DrawWindow(); });
}
void Game::ConfigureFrameExecution(const FrameExecutionSettings& settings) {
    if (settings.fpsLimit < 0) throw std::invalid_argument("Invalid frame limit");
    _frameExecution = settings;
    _renderer->SetTargetFps(settings.fpsLimit);
    _renderer->SetVSync(settings.vsync);
    _renderer->SetExecutionOptions(settings.drawing);
}
void Game::DrawFpsCounter() const {
    // Bekleme (FPS sınırı, VSync) hariç bu karenin işi; sayaç kendi çizimini ve kare sonu GPU teslimini saymaz.
    const auto work = static_cast<float>(GetTime() - _frameStart);
    _frameWorkSeconds += (work - _frameWorkSeconds) * FpsCounterSmoothing;
    std::array<char, 48> label{};
    std::snprintf(label.data(), label.size(), "%d FPS  %.1f ms", GetFPS(), _frameWorkSeconds * 1000.0f);
    const int width = _text.Measure(label.data(), FpsCounterFontSize);
    DrawRectangle(FpsCounterX - FpsCounterPadding, FpsCounterY - FpsCounterPadding, width + 2 * FpsCounterPadding,
                  FpsCounterFontSize + 2 * FpsCounterPadding, FpsCounterBackground);
    _text.Draw(label.data(), FpsCounterX, FpsCounterY, FpsCounterFontSize, FpsCounterText);
}
bool Game::ShowLoading() {
    const StartupTimer timer("Loading screen");
    const double start = GetTime();
    std::optional<double> loadedAt;
    while (!_quitRequested && !_renderer->ShouldClose()) {
        const double now = GetTime();
        if (!loadedAt && _audio.FinishLoading()) loadedAt = now;
        float outro = 0.0f;
        if (loadedAt) {
            const float outroSeconds = LoadingOutroSeconds(static_cast<float>(*loadedAt - start));
            const auto since = static_cast<float>(now - *loadedAt);
            if (since >= outroSeconds) return true;
            outro = Progress01(since, 0.0f, outroSeconds);
        }
        {
            SetDrawTime(DrawSeconds());
            auto frame = _renderer->BeginFrame();
            if (const Scene* scene = _sceneManager->Current()) scene->Render();
            DrawLoading(static_cast<float>(now - start), outro);
            frame.Complete({}, [this] { DrawWindow(); });
        }
        RevealWindow();
    }
    return false;
}
void Game::RevealWindow() {
    if (_revealed) return;
    _revealed = true;
    _renderer->RevealWindow(); // Pencere gizli açıldı; ilk kare hazır olunca görünür
    StartupTimer::Mark("First frame");
}
void Game::Run() {
    if (_ran) throw std::logic_error("A game can only run once");
    _ran = true;
    try {
        StartupTimer::Mark("Game constructed");
        {
            const StartupTimer timer("Start + first scene");
            Start();
            CommitScene();
        }
        const bool loaded = ShowLoading();
        while (loaded && !_quitRequested && !_renderer->ShouldClose()) {
            PrepareFrame();
            if (_quitRequested) break;
#if ENGINE_TUNING_LIVE
            PollTuning(); // Değişen tuning dosyaları karenin başında, ya hep ya hiç uygulanır
#endif
            _frameStart = GetTime();
            FrameObserver* observer = _frameObserver;
            if (observer) {
                observer->BeginFrame();
                observer->BeginRegion(FrameRegion::Input);
            }
            const DeviceInput input = ReadInput();
            if (input.cancelled) {
                if (Scene* scene = _sceneManager->Current()) scene->ReadInput(input);
            } else {
                HandleInput(input);
            }
            if (observer) observer->EndRegion(FrameRegion::Input);
            float realSeconds = GetFrameTime();
            if (observer) observer->BeginRegion(FrameRegion::Audio);
            _audio.Update();
            if (observer) {
                observer->EndRegion(FrameRegion::Audio);
                observer->BeginRegion(FrameRegion::Application);
            }
            realSeconds = FrameSeconds(realSeconds);
            Update(realSeconds);
            CommitScene();
            if (observer) {
                observer->EndRegion(FrameRegion::Application);
                observer->BeginRegion(FrameRegion::Scene);
            }
            Scene* scene = _sceneManager->Current();
            if (scene) scene->TickReal(realSeconds);
            if (scene && scene->Clock() == TimeDomain::Real) scene->Tick(realSeconds);
            const bool paused = _paused || (scene && scene->Enabled() && scene->GameTimePaused());
            const float gameSeconds = paused ? 0.0f : _timeScale.Advance(realSeconds);
            if (scene && scene->Clock() == TimeDomain::Game) scene->Tick(gameSeconds);
            if (observer) observer->EndRegion(FrameRegion::Scene);
            Present();
            if (observer) observer->EndFrame();
            RevealWindow();
        }
        Stop(); // The project settles its run while scene data and services are still alive.
        SaveOptions();
    } catch (...) {
        try {
            _sceneManager->Shutdown();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Game shutdown failed while handling another error: %s\n", error.what());
        } catch (...) {
            std::fputs("Game shutdown failed while handling another error\n", stderr);
        }
        throw;
    }
    _sceneManager->Shutdown();
}
