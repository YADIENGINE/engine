#include "Editor/Api/GraphicAsset.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneServices.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Graphics/Paint/Painter.h"

namespace {
constexpr std::string_view AssetSuffix = ".asset.cpp";
constexpr std::string_view GraphicsFolder = "/assets/graphics/";
constexpr float PreviewShare = 0.6f; // Of the canvas's shorter side

struct GraphicMaker {
    GraphicAsset (*make)();
    std::string file;
};
auto& Makers() {
    static std::vector<GraphicMaker> makers;
    return makers;
}

FloatRect PreviewBox(Vec2 canvas) {
    const float side = static_cast<float>(std::min(canvas.x, canvas.y)) * PreviewShare;
    return {(static_cast<float>(canvas.x) - side) * 0.5f, (static_cast<float>(canvas.y) - side) * 0.5f, side, side};
}

class GraphicAssetScene final : public EditorScene {
public:
    GraphicAssetScene(std::shared_ptr<const GraphicAsset> asset, std::size_t item, Vec2 canvas)
        : _asset(std::move(asset))
        , _item(item)
        , _box(PreviewBox(canvas)) {}
    [[nodiscard]] SceneInspection Inspect() const override {
        SceneInspection inspection;
        inspection.focus = SceneContentBounds{static_cast<int>(_box.x), static_cast<int>(_box.y),
                                              static_cast<int>(_box.width), static_cast<int>(_box.height)};
        return inspection;
    }

protected:
    void Draw(Painter& paint) const override { _asset->draw(paint, _box, _item, static_cast<float>(paint.Time())); }

private:
    std::shared_ptr<const GraphicAsset> _asset;
    std::size_t _item;
    FloatRect _box;
};

std::string Replaced(std::string text, std::string_view from, const std::string& to) {
    for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}
} // namespace

std::string AssetPathKey(const std::filesystem::path& path) {
    auto text = path.lexically_normal().generic_string();
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<SceneDefinition> GraphicAssetDefinitions(const GraphicAsset& asset, const std::filesystem::path& file,
                                                     const std::filesystem::path& engineRoot) {
    const auto where = file.lexically_normal().generic_string();
    if (!asset.draw) throw std::invalid_argument("Graphic asset without draw: " + where);
    const auto key = AssetPathKey(file);
    const auto folder = key.rfind(GraphicsFolder);
    if (folder == std::string::npos || !key.ends_with(AssetSuffix))
        throw std::invalid_argument("Graphic asset must be an Assets/Graphics/**/X.asset.cpp file: " + where);
    // Skip "/Assets/": the source's own spelling keeps the id readable.
    const auto start = folder + std::string_view("/assets/").size();
    const auto relative = where.substr(start, where.size() - start - AssetSuffix.size());
    const bool engine = key.starts_with(AssetPathKey(engineRoot) + "/");
    const auto id = std::string("asset:") + (engine ? "Engine/" : "") + relative;
    const auto setName = relative.substr(relative.rfind('/') + 1);

    auto shared = std::make_shared<const GraphicAsset>(asset);
    const auto entry = [&](std::size_t item, const std::string& itemName, const std::string& itemId,
                           const std::string& tags) {
        SceneDefinition definition;
        definition.id = itemId;
        definition.name = itemName.empty() ? setName : setName + " / " + itemName;
        definition.category = "Assets";
        definition.description = "Graphic asset " + relative + (tags.empty() ? "" : " (" + tags + ")");
        definition.asset =
            SceneAssetInfo{AssetPathKey(file), setName, itemName, Replaced(asset.usage, "{item}", itemName)};
        definition.create = [shared, item](const SceneServices& services, const SceneParameters&) {
            return std::make_unique<GraphicAssetScene>(shared, item, services.canvas);
        };
        return definition;
    };
    std::vector<SceneDefinition> definitions;
    if (asset.items.empty()) definitions.push_back(entry(0, {}, id, {}));
    for (std::size_t item = 0; item < asset.items.size(); ++item) {
        const auto& [name, tags] = asset.items[item];
        if (name.empty()) throw std::invalid_argument("Graphic asset item without a name: " + where);
        definitions.push_back(entry(item, name, id + "#" + name, tags));
    }
    return definitions;
}

bool RegisterGraphicAsset(GraphicAsset (*make)(), std::source_location where) {
    Makers().push_back({make, where.file_name()});
    return true;
}

std::vector<GraphicThumbnail> RegisteredGraphicThumbnails() {
    std::vector<GraphicThumbnail> thumbnails;
    for (const auto& maker : Makers()) {
        auto asset = std::make_shared<const GraphicAsset>(maker.make());
        const auto definitions = GraphicAssetDefinitions(*asset, maker.file, ENGINE_EDITOR_ROOT);
        for (std::size_t item = 0; item < definitions.size(); ++item)
            thumbnails.push_back({definitions[item].id, asset, item});
    }
    return thumbnails;
}

std::string AssetThumbnailFile(const std::string& id) {
    std::uint64_t hash = 14695981039346656037ull; // FNV-1a
    for (const unsigned char c : id) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return std::format("{:016x}.png", hash);
}

void AddRegisteredGraphicAssets(SceneRegistry& registry) {
    for (const auto& maker : Makers())
        for (auto& definition : GraphicAssetDefinitions(maker.make(), maker.file, ENGINE_EDITOR_ROOT))
            registry.Add(std::move(definition));
}
