#include "Editor/Internals/Settings/EditorSettingsStore.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr std::size_t MaxSettingsBytes = 4 * 1024 * 1024;
bool ValidFontSize(int value) { return std::ranges::find(EditorFontSizes, value) != EditorFontSizes.end(); }
void ValidateIds(std::vector<std::string>& ids, std::size_t maximum) {
    std::vector<std::string> result;
    for (const auto& id : ids) {
        if (!id.empty() && id.size() <= 256 && std::ranges::find(result, id) == result.end()) result.push_back(id);
        if (result.size() == maximum) break;
    }
    ids = std::move(result);
}
} // namespace
EditorSettingsStore::EditorSettingsStore(std::filesystem::path file)
    : _file(std::move(file)) {}
std::string EditorSettingsStore::Load(const std::string& project, EditorPreferences& preferences, int& fontSize) {
    preferences = {};
    fontSize = 100;
    _document = nlohmann::json::object();
    try {
        if (!std::filesystem::exists(_file)) return {};
        auto document = nlohmann::json::parse(ReadBinaryFile(_file, MaxSettingsBytes));
        if (!document.is_object() || document.value("version", 0) != 1)
            throw std::runtime_error("Unsupported settings format");
        const int font = document.value("fontSize", 100);
        EditorPreferences loaded;
        const auto projects = document.value("projects", nlohmann::json::object());
        if (!projects.is_object()) throw std::runtime_error("Invalid project preferences");
        if (const auto found = projects.find(project); found != projects.end()) {
            const auto& value = *found;
            if (value.contains("visible")) {
                const auto& visible = value.at("visible");
                if (!visible.is_array()) throw std::runtime_error("Invalid panel visibility");
                for (std::size_t i = 0; i < std::min(visible.size(), loaded.visible.size()); ++i)
                    loaded.visible[i] = visible.at(i).get<bool>();
            }
            loaded.docking = value.value("docking", std::string{});
            const int layout =
                value.value("layout", static_cast<int>(loaded.docking.empty() ? EditorWorkspaceLayout::Develop
                                                                              : EditorWorkspaceLayout::Custom));
            if (layout >= static_cast<int>(EditorWorkspaceLayout::Develop) &&
                layout <= static_cast<int>(EditorWorkspaceLayout::Custom))
                loaded.layout = static_cast<EditorWorkspaceLayout>(layout);
            loaded.customDocking = value.value("customDocking", std::string{});
            if (value.contains("customVisible")) {
                const auto& visible = value.at("customVisible");
                if (!visible.is_array()) throw std::runtime_error("Invalid custom panel visibility");
                for (std::size_t i = 0; i < std::min(visible.size(), loaded.customVisible.size()); ++i)
                    loaded.customVisible[i] = visible.at(i).get<bool>();
            }
            if (loaded.layout == EditorWorkspaceLayout::Custom && loaded.customDocking.empty()) {
                loaded.customDocking = loaded.docking;
                loaded.customVisible = loaded.visible;
            }
            const int bottom = value.value("lastBottomPanel", static_cast<int>(EditorPanel::Output));
            loaded.lastBottomPanel =
                bottom == static_cast<int>(EditorPanel::Profiler) ? EditorPanel::Profiler : EditorPanel::Output;
            loaded.favorites = value.value("favorites", std::vector<std::string>{});
            loaded.recent = value.value("recent", std::vector<std::string>{});
            ValidateIds(loaded.favorites, 2048);
            ValidateIds(loaded.recent, 10);
            loaded.followOutput = value.value("followOutput", true);
            loaded.showMetrics = value.value("showMetrics", false);
            const auto filter = value.value("output", nlohmann::json::object());
            loaded.outputFilter = {filter.value("info", true), filter.value("warning", true),
                                   filter.value("error", true), filter.value("search", std::string{})};
            loaded.outputFilter.search.resize(std::min(loaded.outputFilter.search.size(), std::size_t{255}));
        }
        preferences = std::move(loaded);
        fontSize = ValidFontSize(font) ? font : 100;
        _document = std::move(document);
        return {};
    } catch (const std::exception& error) {
        preferences = {};
        fontSize = 100;
        _document = nlohmann::json::object();
        return std::string("Could not load editor preferences: ") + error.what();
    }
}
std::string EditorSettingsStore::Save(const std::string& project, const EditorPreferences& preferences, int fontSize) {
    try {
        auto document = _document;
        document["version"] = 1;
        document["fontSize"] = ValidFontSize(fontSize) ? fontSize : 100;
        const auto& filter = preferences.outputFilter;
        document["projects"][project] = {
            {"visible", preferences.visible},
            {"docking", preferences.docking},
            {"layout", static_cast<int>(preferences.layout)},
            {"customDocking", preferences.customDocking},
            {"customVisible", preferences.customVisible},
            {"lastBottomPanel", static_cast<int>(preferences.lastBottomPanel)},
            {"favorites", preferences.favorites},
            {"recent", preferences.recent},
            {"followOutput", preferences.followOutput},
            {"showMetrics", preferences.showMetrics},
            {"output",
             {{"info", filter.info}, {"warning", filter.warning}, {"error", filter.error}, {"search", filter.search}}}};
        const auto bytes = document.dump(2);
        if (bytes.size() > MaxSettingsBytes) throw std::runtime_error("Editor preferences exceed capacity");
        std::filesystem::create_directories(_file.parent_path());
        (void)WriteBinaryFileAtomic(_file, bytes);
        _document = std::move(document);
        return {};
    } catch (const std::exception& error) {
        return std::string("Could not save editor preferences: ") + error.what();
    }
}
