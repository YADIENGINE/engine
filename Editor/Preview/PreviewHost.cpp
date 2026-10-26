#include "Editor/Preview/PreviewHost.h"
#include <raylib.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Preview/PreviewGame.h"

int RunPreviewHost(int argc, char** argv, const std::function<void(PreviewRegistry&)>& registerPreviews,
                   GameSettings settings, PreviewApplicationFactory createApplication) {
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
                throw std::runtime_error("Unsupported preview argument: " + argument);
        }
        PreviewRegistry registry;
        registerPreviews(registry); // Metadata and factories only: no GL/assets/instances.
        channel = std::make_unique<ControlChannel>(session, false);
        channel->Send({{}, 0, 0, "catalog", EncodePreviewCatalog(registry.Entries())});
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
        if (settings.canvasWidth <= 0 || settings.canvasHeight <= 0 || settings.canvasWidth > PreviewMaxFrameWidth ||
            settings.canvasHeight > PreviewMaxFrameHeight)
            throw std::runtime_error("Unsupported preview canvas size");
        std::unique_ptr<Game> game = createApplication
                                         ? createApplication(settings, registry, *channel, session)
                                         : std::make_unique<PreviewGame>(settings, registry, *channel, session);
        if (!game) throw std::runtime_error("Preview application factory returned no Game");
        game->Run();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "PreviewHost: %s\n", error.what());
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
