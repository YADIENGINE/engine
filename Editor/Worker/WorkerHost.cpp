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
#include <utility>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Editor/Internals/Session/SceneAutoRegistration.h"
#include "Editor/Worker/AssetThumbnailGame.h"
#include "Editor/Worker/AudioReport.h"
#include "Editor/Worker/WorkerGame.h"
#include "Editor/Worker/GlowBenchmark.h"
#include "Editor/Worker/LightGallery.h"
#include "Editor/Worker/PostGallery.h"
#include "Editor/Worker/RenderGallery.h"
#include "Tuning/TuningLive.h"
#include "Diagnostics/Internals/CrashSession.h"

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

namespace {
void PrepareHiddenWindow(GameSettings& settings) {
    settings.windowVisible = false;
    settings.alwaysRun = true;
    settings.project.maximized = false;
    settings.project.resizable = false;
    settings.project.windowWidth = settings.project.canvasWidth;
    settings.project.windowHeight = settings.project.canvasHeight;
    settings.project.targetFps = 30;
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
    const CrashSession crash("worker");
    return RunCrashBoundary([&]() -> int {
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
                // A tool's question; no window, device or editor
                WriteAudioReport(argv[++i], CatalogWithSounds(settings.audio, settings.sounds));
                return 0;
            } else if (argument == "--thumbnails" && i + 2 < argc) {
                thumbnails = argv[++i];
                settings.project.background = ParseThumbnailBackground(argv[++i]);
            } else
                throw std::runtime_error("Unsupported worker argument: " + argument);
        }
        SceneRegistry registry;
        registry.Add(GlowBenchmarkDefinition());
        registry.Add(RenderGalleryDefinition());
        registry.Add(LightGalleryDefinition());
        registry.Add(PostGalleryDefinition());
        AddRegisteredCatalogScenes(registry);
        AddRegisteredGraphicAssets(registry);
        registerScenes(registry); // Metadata and factories only: no GL/assets/instances.
        if (thumbnails) {
            SetTraceLogLevel(LOG_WARNING);
            PrepareHiddenWindow(settings);
            settings.audio = {}; // Drawing only: no reason to load the game's sounds
            settings.sounds = {};
            settings.uiSounds = {};
            AssetThumbnailGame game(settings, *thumbnails);
            game.Run();
            return 0;
        }
        channel = std::make_unique<ControlChannel>(session, false);
        channel->Send({{}, 0, 0, "audio", EncodeAudioCatalog(CatalogWithSounds(settings.audio, settings.sounds))});
        auto catalog = EncodeSceneCatalog(registry.Entries());
        const auto catalogBytes = catalog.dump().size();
        constexpr std::size_t EnvelopeMargin = 1024;
        if (catalogBytes >= WorkerMaxMessageBytes * 3 / 4)
            channel->Send({{},
                           0,
                           0,
                           "log",
                           {{"text", "Scene catalog is " + std::to_string(catalogBytes) + " bytes, limit " +
                                         std::to_string(WorkerMaxMessageBytes)}}});
        if (catalogBytes > WorkerMaxMessageBytes - EnvelopeMargin) {
            // Expected content failure: deliver the diagnosis without entering the crash boundary's exception path.
            try {
                channel->Send(
                    {{}, 0, 0, "catalog-failed", {{"text", SceneCatalogTooLargeText(catalog, catalogBytes)}}});
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                bool shutdown = false;
                while (!channel->Closed() && std::chrono::steady_clock::now() < deadline) {
                    for (const auto& message : channel->Poll())
                        if (message.type == "shutdown") shutdown = true;
                    if (shutdown && channel->QueuedBytes() == 0) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            } catch (const std::exception&) { // The editor may close the channel while the diagnosis is being sent.
            }
            return 1;
        }
        channel->Send({{}, 0, 0, "catalog", std::move(catalog)});
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
        const TuningForwarding tuning(*channel); // Oyun kurulurken kaydolan dosyaların ilk raporu da gitsin
        // Readback's per-frame INFO messages must not displace useful diagnostics in the output panel.
        SetTraceLogLevel(LOG_WARNING);
        PrepareHiddenWindow(settings);
        if (settings.project.canvasWidth <= 0 || settings.project.canvasHeight <= 0 ||
            settings.project.canvasWidth > WorkerMaxFrameWidth || settings.project.canvasHeight > WorkerMaxFrameHeight)
            throw std::runtime_error("Unsupported scene canvas size");
        std::unique_ptr<Game> game = createApplication
                                         ? createApplication(settings, registry, *channel, session)
                                         : std::make_unique<WorkerGame>(settings, registry, *channel, session);
        if (!game) throw std::runtime_error("Worker application factory returned no Game");
        game->Run();
        return 0;
    });
}
