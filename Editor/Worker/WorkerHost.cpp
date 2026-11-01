#include "Editor/Worker/WorkerHost.h"
#include <raylib.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Editor/Internals/Session/SceneAutoRegistration.h"
#include "Editor/Worker/AssetThumbnailGame.h"
#include "Editor/Worker/AudioReport.h"
#include "Editor/Worker/WorkerGame.h"
#include "Editor/Worker/GlowBenchmark.h"
#include "Editor/Worker/RenderGallery.h"
#include "Tuning/TuningLive.h"

#if ENGINE_TUNING_LIVE
namespace {
WorkerTuningState StateOf(TuningOutcome outcome) {
    switch (outcome) {
    case TuningOutcome::Applied   : return WorkerTuningState::Applied;
    case TuningOutcome::Invalid   : return WorkerTuningState::Invalid;
    case TuningOutcome::NeedsBuild: return WorkerTuningState::NeedsBuild;
    case TuningOutcome::Unreadable: return WorkerTuningState::Unreadable;
    }
    return WorkerTuningState::Unreadable;
}

// Canlı tuning raporları (Docs/Tuning.md) editörün Tuning paneline ve Output'una gider; kanal kapanmadan önce
// varsayılan rapor yeri geri gelir.
class TuningForwarding {
public:
    explicit TuningForwarding(ControlChannel& channel) {
        SetTuningReporter([&channel](const TuningReport& report) {
            if (channel.Closed()) return;
            try {
                channel.Send({{},
                              0,
                              0,
                              "tuning",
                              EncodeWorkerTuning(
                                  {report.source, StateOf(report.outcome), report.line, report.text, report.reload})});
            } catch (const std::exception&) { // Bağlantı koptuysa önizleme zaten kapanıyor
            }
        });
    }
    ~TuningForwarding() { SetTuningReporter({}); }

    TuningForwarding(const TuningForwarding&) = delete;
    TuningForwarding& operator=(const TuningForwarding&) = delete;
};
} // namespace
#endif

namespace {
void PrepareHiddenWindow(GameSettings& settings) {
    settings.window.visible = false;
    settings.window.alwaysRun = true;
    settings.window.maximized = false;
    settings.window.resizable = false;
    settings.window.width = settings.canvasWidth;
    settings.window.height = settings.canvasHeight;
    settings.window.targetFps = 30;
    settings.persistentOptions = false;
    settings.fixedWindow = true;
}
// RRGGBB
Color4 ParseThumbnailBackground(const std::string& hex) {
    const auto value = std::stoul(hex, nullptr, 16);
    return {static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value), 255};
}
} // namespace

int RunEditorWorker(int argc, char** argv, const std::function<void(SceneRegistry&)>& registerScenes,
                    GameSettings settings, WorkerApplicationFactory createApplication) {
    std::unique_ptr<ControlChannel> channel;
    try {
        std::string session;
        bool catalogOnly = false;
        std::optional<std::filesystem::path> thumbnails;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "--session" && i + 1 < argc)
                session = argv[++i];
            else if (argument == "--catalog")
                catalogOnly = true;
            else if (argument == "--audio-report" && i + 1 < argc) {
                WriteAudioReport(argv[++i], settings.audio); // A tool's question; no window, device or editor
                return 0;
            } else if (argument == "--thumbnails" && i + 2 < argc) {
                thumbnails = argv[++i];
                settings.background = ParseThumbnailBackground(argv[++i]);
            } else
                throw std::runtime_error("Unsupported worker argument: " + argument);
        }
        SceneRegistry registry;
        registry.Add(GlowBenchmarkDefinition());
        registry.Add(RenderGalleryDefinition());
        AddRegisteredEditorScenes(registry);
        AddRegisteredGraphicAssets(registry);
        registerScenes(registry); // Metadata and factories only: no GL/assets/instances.
        if (thumbnails) {
            SetTraceLogLevel(LOG_WARNING);
            PrepareHiddenWindow(settings);
            settings.audio = {}; // Drawing only: no reason to load the game's sounds
            AssetThumbnailGame game(settings, *thumbnails);
            game.Run();
            return 0;
        }
        channel = std::make_unique<ControlChannel>(session, false);
        channel->Send({{}, 0, 0, "audio", EncodeAudioCatalog(settings.audio)});
        channel->Send({{}, 0, 0, "catalog", EncodeSceneCatalog(registry.Entries())});
        if (catalogOnly) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (true) {
                for (const auto& message : channel->Poll()) {
                    if (message.type == "catalog-ack" || message.type == "shutdown") return 0;
                }
                if (channel->Closed() || std::chrono::steady_clock::now() >= deadline)
                    throw std::runtime_error("Catalog acknowledgement failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
#if ENGINE_TUNING_LIVE
        const TuningForwarding tuning(*channel); // Oyun kurulurken kaydolan dosyaların ilk raporu da gitsin
#endif
        // Readback's per-frame INFO messages must not displace useful diagnostics in the output panel.
        SetTraceLogLevel(LOG_WARNING);
        PrepareHiddenWindow(settings);
        if (settings.canvasWidth <= 0 || settings.canvasHeight <= 0 || settings.canvasWidth > WorkerMaxFrameWidth ||
            settings.canvasHeight > WorkerMaxFrameHeight)
            throw std::runtime_error("Unsupported scene canvas size");
        std::unique_ptr<Game> game = createApplication
                                         ? createApplication(settings, registry, *channel, session)
                                         : std::make_unique<WorkerGame>(settings, registry, *channel, session);
        if (!game) throw std::runtime_error("Worker application factory returned no Game");
        game->Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "WorkerHost: %s\n", error.what());
        if (channel && !channel->Closed()) {
            try {
                channel->Send({{}, 0, 0, "fatal", {{"text", error.what()}}});
                (void)channel->Poll();
            } catch (...) {
            }
        }
        return 1;
    }
}
