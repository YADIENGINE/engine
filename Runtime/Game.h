#pragma once
#include <optional>
#include <vector>
#include "Modules/Persistence/Persistence.hpp"
#include "Audio/AudioSystem.h"
#include "Options/GameOptions.h"
#include "Fx/TimeScale.h"
#include "Graphics/Render/Renderer.h"
#include "Graphics/Draw/TextRenderer.h"
#include "Input/InputReader.h"
#include "Runtime/GameSettings.h"
#include "Runtime/GraphicsSettings.h"

class Scene;

// Runtime owns the loop and services. A project's derived game supplies content and scenes.
class Game {
public:
    virtual ~Game();
    void Run();

protected:
    explicit Game(const GameSettings& settings);
    void RegisterScene(Scene& scene);
    void SetScene(Scene& scene, bool restart = false);
    void RequestQuit() { _quitRequested = true; }
    void SetPaused(bool paused) { _paused = paused; }
    [[nodiscard]] TimeScale& Time() { return _timeScale; }
    [[nodiscard]] AudioSystem& Audio() { return _audio; }
    [[nodiscard]] const AudioSystem& Audio() const { return _audio; }
    [[nodiscard]] const TextRenderer& Text() const { return _text; }
    // FPS sınırı, ekran modu ve pencere boyutu engine tercihidir: uygulanır ve kalıcı olur. 0: sınırsız FPS.
    void SetFpsLimit(int fps);
    [[nodiscard]] int FpsLimit() const;
    void SetBorderlessFullscreen(bool enabled);
    [[nodiscard]] bool BorderlessFullscreen() const;
    // Pencere modundaki boyut; {0, 0}: ekranı kaplayacak şekilde büyütülmüş. Kenarlıksız tam ekranda saklanır,
    // pencereye dönünce uygulanır.
    void SetWindowSize(Vec2 size);
    [[nodiscard]] Vec2 WindowSize() const;
    // Pencerenin bulunduğu monitörün çözünürlüğü.
    [[nodiscard]] Vec2 MonitorSize() const;
    // Grafik tercihleri (çizim çözünürlüğü, VSync, FPS sayacı, efekt yoğunluğu) engine tercihidir: uygulanır ve
    // kalıcı olur. Kayıttaki geçersiz değerler varsayılana döner.
    void SetGraphics(const GraphicsSettings& graphics);
    [[nodiscard]] GraphicsSettings Graphics() const;

    // Tercih dosyası (settings.json) kurulumda yüklenir; ses, FPS, ekran ve grafik engine'indir. Oyun kendi
    // anahtarlarını Options()'a yazar; engine sahne geçişinde, Suspend ve Stop'tan sonra değişiklikleri kaydeder.
    [[nodiscard]] GameOptions& Options() { return _options; }
    [[nodiscard]] const GameOptions& Options() const { return _options; }
    // Kayıt noktasını beklemeden yazar (ör. kapanmaya dayanması gereken bir seçim).
    void SaveOptions();

    [[nodiscard]] std::filesystem::path SavePath(const std::filesystem::path& filename) const;

    [[nodiscard]] Persistence CreatePersistence(const std::filesystem::path& filename,
                                                std::optional<SaveSettings> settings = std::nullopt) const;

    virtual void Start();
    [[nodiscard]] virtual DeviceInput ReadInput();
    virtual void HandleInput(const DeviceInput& input);
    virtual void Update(float realSeconds);
    // External hosts may supply simulation time while rendering and communication continue normally.
    [[nodiscard]] virtual float FrameSeconds(float realSeconds);
    // Sunum saati (DrawTime) kaynağı; her kare çizilmeden önce okunur. Varsayılan gerçek zaman; host'lar
    // duraklatılabilir kendi saatini verir.
    [[nodiscard]] virtual double DrawSeconds() const;
    virtual void Draw() const;
    // Completed canvas is borrowed for this call; window drawing runs outside the canvas projection.
    virtual void CanvasReady(const Texture2D& texture) const;
    virtual void DrawWindow() const;
    // Yükleme ekranı: Start'tan sonra sesler arka planda yüklenirken ilk sahne ilerlemeden (input ve Tick yok) çizilir,
    // bu onun üstüne çizilir. 'seconds' ekranın açılışından beri; 'outro' yükleme bittikten sonraki kapanış (0..1).
    virtual void DrawLoading(float seconds, float outro) const;
    // Yükleme bittikten sonra yükleme ekranının kapanma süresi; 'loadSeconds' yüklemenin sürdüğü süre.
    [[nodiscard]] virtual float LoadingOutroSeconds(float loadSeconds) const;
    virtual void Stop();
    virtual void Suspend();
    virtual void Resume();

private:
    friend class EngineGodAccessor;
    void CommitScene();
    // Yükleme bitip kapanış süresi dolana kadar yükleme ekranını çizer; pencere kapatılırsa false.
    bool ShowLoading();
    void RevealWindow();
    void Present() const;
    void Shutdown();
    void ApplyOptions();
    void ApplyGraphics();
    void DrawFpsCounter() const;

    // Context first, content/services afterwards; derived game members are destroyed before these resources.
    Renderer _renderer;
    std::filesystem::path _assetRoot;
    TextRenderer _text;
    AudioSystem _audio;
    InputReader _input;
    TimeScale _timeScale;
    std::filesystem::path _applicationId;
    SaveSettings _saveSettings;
    GameOptions _options;
    int _defaultFpsLimit;
    bool _persistentOptions;
    bool _fixedWindow;
    bool _showFps = false;
    double _frameStart = 0.0;               // Karenin başı (GetTime); FPS sayacının iş süresi buradan ölçülür
    mutable float _frameWorkSeconds = 0.0f; // Güncelleme + çizim süresi (bekleme hariç), yumuşatılmış
    std::vector<Scene*> _scenes;
    Scene* _scene = nullptr;
    Scene* _pendingScene = nullptr;
    bool _restartScene = false;
    bool _quitRequested = false;
    bool _paused = false;
    bool _ran = false;
    bool _revealed = false; // Gizli açılan pencere ilk kareden sonra gösterildi

public:
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;
};
