#include "Access/EngineGodAccessor.h"
#include "Runtime/Game.h"

JsonIO::Document EngineGodAccessor::RunStatistics::Context(const Game& game) {
    JsonIO::Document result = JsonIO::Document::MakeObject();
    result["application_id"] = game._applicationId.string();
#if defined(__ANDROID__)
    result["platform"] = "android";
#elif defined(_WIN32)
    result["platform"] = "windows";
#elif defined(__APPLE__)
    result["platform"] = "macos";
#else
    result["platform"] = "linux";
#endif
    return result;
}
std::filesystem::path EngineGodAccessor::RunStatistics::Directory(const Game& game) { return game.SavePath("runs"); }
