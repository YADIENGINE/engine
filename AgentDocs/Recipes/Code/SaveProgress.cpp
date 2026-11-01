// [recipe:SaveProgress]
#include "AgentDocs/Recipes/Code/SaveProgress.h"
#include "Modules/Persistence/Persistence.h"
#include "Options/GameOptions.h"

namespace {
constexpr std::int64_t HintsEnabled = 1;
} // namespace

// In a derived Game use Options()/SaveOptions() and CreatePersistence("progress.json") instead.
std::int64_t SaveRecipeProgress(const std::filesystem::path& directory, std::int64_t completed) {
    GameOptions options(directory / "settings.json");
    options.Load();
    options.SetInt("myproject.hints", HintsEnabled);
    options.Save();
    Persistence progress(directory / "progress.json", {.keepBackup = true});
    progress.Load();
    progress.SetInt("completed", completed);
    progress.Save();
    return progress.GetInt("completed");
}
// [/recipe]
