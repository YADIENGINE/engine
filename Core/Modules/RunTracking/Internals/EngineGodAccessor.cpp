#include "Modules/RunTracking/Internals/EngineGodAccessor.h"
#include "Modules/RunTracking/RunTracking.h"
#include "Runtime/Game.h"

JsonIO::Document EngineGodAccessor::Context(const Game& game) {
    JsonIO::Document result = JsonIO::Document::MakeObject();
    result["application_id"] = game._applicationId.string();
#if defined(_WIN32)
    result["platform"] = "windows";
#elif defined(__APPLE__)
    result["platform"] = "macos";
#else
    result["platform"] = "linux";
#endif
    return result;
}
std::filesystem::path EngineGodAccessor::Directory(const Game& game) { return game.SavePath("runs"); }

JsonIO::Document RunTracking::Context(const Game& game) { return EngineGodAccessor::Context(game); }
std::filesystem::path RunTracking::Directory(const Game& game) { return EngineGodAccessor::Directory(game); }
