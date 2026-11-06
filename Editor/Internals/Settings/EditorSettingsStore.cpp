#include "Editor/Internals/Settings/EditorSettingsStore.h"
#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <utility>
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr std::size_t MaxSettingsBytes = 4 * 1024 * 1024;
bool ValidFontSize(int value) { return std::ranges::find(EditorFontSizes, value) != EditorFontSizes.end(); }
// Preferences before panels were saved by key held arrays in this order (the Tuning panel was folded into Inspector).
constexpr std::array<const char*, 9> LegacyPanelKeys{"catalog", "viewport", "inspector", "output",       "profiler",
                                                     "tuning",  "console",  "assets",    "audio-preview"};

std::optional<EditorPanel> PanelOf(std::string_view key) {
    for (const auto& panel : EditorPanels)
        if (key == panel.key) return panel.id;
    return std::nullopt;
}
nlohmann::json SavePanels(const EditorPanelVisibility& visible) {
    auto panels = nlohmann::json::object();
    for (const auto& panel : EditorPanels)
        panels[panel.key] = visible[static_cast<std::size_t>(panel.id)];
    return panels;
}
// `keyed` ({"inspector": true, ...}) or a `legacy` array; panels neither names keep their defaults.
void LoadPanels(const nlohmann::json& project, const char* keyed, const char* legacy, EditorPanelVisibility& visible) {
    if (const auto found = project.find(keyed); found != project.end()) {
        if (!found->is_object()) throw std::runtime_error("Invalid panel visibility");
        for (const auto& [key, shown] : found->items())
            if (const auto panel = PanelOf(key)) visible[static_cast<std::size_t>(*panel)] = shown.get<bool>();
        return;
    }
    if (const auto found = project.find(legacy); found != project.end()) {
        if (!found->is_array()) throw std::runtime_error("Invalid panel visibility");
        for (std::size_t i = 0; i < std::min(found->size(), LegacyPanelKeys.size()); ++i)
            if (const auto panel = PanelOf(LegacyPanelKeys[i]))
                visible[static_cast<std::size_t>(*panel)] = found->at(i).get<bool>();
    }
}
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
        if (const auto theme = document.find("theme"); theme != document.end() && theme->is_string())
            loaded.theme = ParseEditorTheme(theme->get<std::string>());
        const auto projects = document.value("projects", nlohmann::json::object());
        if (!projects.is_object()) throw std::runtime_error("Invalid project preferences");
        if (const auto found = projects.find(project); found != projects.end()) {
            const auto& value = *found;
            LoadPanels(value, "panels", "visible", loaded.visible);
            loaded.docking = value.value("docking", std::string{});
            const int layout =
                value.value("layout", static_cast<int>(loaded.docking.empty() ? EditorWorkspaceLayout::Develop
                                                                              : EditorWorkspaceLayout::Custom));
            if (layout >= static_cast<int>(EditorWorkspaceLayout::Develop) &&
                layout <= static_cast<int>(EditorWorkspaceLayout::Agent))
                loaded.layout = static_cast<EditorWorkspaceLayout>(layout);
            loaded.customDocking = value.value("customDocking", std::string{});
            LoadPanels(value, "customPanels", "customVisible", loaded.customVisible);
            if (loaded.layout == EditorWorkspaceLayout::Custom && loaded.customDocking.empty()) {
                loaded.customDocking = loaded.docking;
                loaded.customVisible = loaded.visible;
            }
            loaded.lastBottomPanel = EditorPanel::Output;
            std::optional<EditorPanel> bottom;
            if (const auto key = value.find("bottomPanel"); key != value.end() && key->is_string())
                bottom = PanelOf(key->get<std::string>());
            else if (const auto index = value.find("lastBottomPanel");
                     index != value.end() && index->is_number_unsigned())
                if (index->get<std::size_t>() < LegacyPanelKeys.size())
                    bottom = PanelOf(LegacyPanelKeys[index->get<std::size_t>()]);
            for (const auto panel : EditorBottomPanels)
                if (bottom == panel) loaded.lastBottomPanel = panel;
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
        if (const auto agent = document.find("agent"); agent != document.end() && agent->is_object()) {
            const auto key = agent->value("kind", std::string{});
            for (const auto& kind : AgentKinds)
                if (key == kind.key) loaded.agent.kind = kind.id;
            loaded.agent.customCommand = agent->value("customCommand", std::string{});
            loaded.agent.customCommand.resize(std::min(loaded.agent.customCommand.size(), std::size_t{1024}));
            loaded.agent.control = agent->value("control", true);
            loaded.agent.visualChecks = agent->value("visualChecks", true);
            loaded.agent.configured = agent->value("configured", false);
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
        document["theme"] = EditorThemeInfo(preferences.theme).key;
        document["agent"] = {{"kind", AgentKindInfo(preferences.agent.kind).key},
                             {"customCommand", preferences.agent.customCommand},
                             {"control", preferences.agent.control},
                             {"visualChecks", preferences.agent.visualChecks},
                             {"configured", preferences.agent.configured}};
        const auto& filter = preferences.outputFilter;
        document["projects"][project] = {
            {"panels", SavePanels(preferences.visible)},
            {"docking", preferences.docking},
            {"layout", static_cast<int>(preferences.layout)},
            {"customDocking", preferences.customDocking},
            {"customPanels", SavePanels(preferences.customVisible)},
            {"bottomPanel", EditorPanelInfo(preferences.lastBottomPanel).key},
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
