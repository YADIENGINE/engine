#pragma once
#include <filesystem>
#include <string>
#include "Modules/JsonIO/JsonIO.hpp"

class Game;

// Engine owns durable gzip JSON writing; the consuming game owns every record field.
class RunTracking final {
public:
    explicit RunTracking(std::filesystem::path directory);
    // Writes <run_id>.json.gz. Same ID replaces the same record atomically on retry.
    [[nodiscard]] std::filesystem::path Write(const std::string& runId, const JsonIO::Document& record) const;
    [[nodiscard]] const std::filesystem::path& Directory() const { return _directory; }
    [[nodiscard]] static std::string NewRunId();
    [[nodiscard]] static std::string UtcNow();

    // Optional runtime bridge: link EngineRunTrackingRuntime to observe a Game.
    [[nodiscard]] static JsonIO::Document Context(const Game& game);
    [[nodiscard]] static std::filesystem::path Directory(const Game& game);

private:
    std::filesystem::path _directory;
};
