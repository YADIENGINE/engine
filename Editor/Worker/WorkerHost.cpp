#include "Editor/Worker/WorkerHost.h"
#include <raylib.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Session/SceneAutoRegistration.h"
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

int RunEditorWorker(int argc, char** argv, const std::function<void(SceneRegistry&)>& registerScenes,
                    GameSettings settings, WorkerApplicationFactory createApplication) {
    std::unique_ptr<ControlChannel> channel;
    try {
        std::string session;
        bool catalogOnly = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "--session" && i + 1 < argc)
                session = argv[++i];
            else if (argument == "--catalog")
                catalogOnly = true;
            else
                throw std::runtime_error("Unsupported worker argument: " + argument);
        }
        SceneRegistry registry;
        registry.Add(GlowBenchmarkDefinition());
        registry.Add(RenderGalleryDefinition());
        AddRegisteredEditorScenes(registry);
        registerScenes(registry); // Metadata and factories only: no GL/assets/instances.
        channel = std::make_unique<ControlChannel>(session, false);
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
        settings.window.visible = false;
        settings.window.alwaysRun = true;
        settings.window.maximized = false;
        settings.window.resizable = false;
        settings.window.width = settings.canvasWidth;
        settings.window.height = settings.canvasHeight;
        settings.window.targetFps = 30;
        settings.persistentOptions = false;
        settings.fixedWindow = true;
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
