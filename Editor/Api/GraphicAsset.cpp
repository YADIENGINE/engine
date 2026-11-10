#include "Editor/Api/GraphicAsset.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Scene/SceneServices.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Pixel/PixelCanvas.h"
#include "Input/Input.h"

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

std::filesystem::path& EngineRoot() {
    static std::filesystem::path root;
    return root;
}

FloatRect PreviewBox(Vec2 canvas) {
    const float side = static_cast<float>(std::min(canvas.x, canvas.y)) * PreviewShare;
    return {(static_cast<float>(canvas.x) - side) * 0.5f, (static_cast<float>(canvas.y) - side) * 0.5f, side, side};
}

// How the preview shows a pixel graphic (GraphicAsset::pixels).
enum class PreviewView { Drawing, Pixels, Inks, Parts };
constexpr std::string_view PreviewViewNames[] = {"Drawing", "Pixels", "Inks", "Parts"};
constexpr float PixelArea = 0.9f;         // Of the canvas: room for the zoomed sprite
constexpr int GridFromZoom = 4;           // Pixels this many device pixels wide or more get a grid
constexpr Color4 GridColor{0, 0, 0, 70};
constexpr Color4 CheckerLight{58, 62, 70, 255};
constexpr Color4 CheckerDark{46, 50, 57, 255};
constexpr Color4 PivotColor{255, 80, 200, 255};

// A distinct, readable colour per index (golden-ratio hues), for the Inks and Parts views.
Color4 IndexColor(int index) {
    const float hue = std::fmod(static_cast<float>(index) * 0.61803398875f, 1.0f) * 6.0f;
    const float x = 1.0f - std::abs(std::fmod(hue, 2.0f) - 1.0f);
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (hue < 1.0f) r = 1.0f, g = x;
    else if (hue < 2.0f) r = x, g = 1.0f;
    else if (hue < 3.0f) g = 1.0f, b = x;
    else if (hue < 4.0f) g = x, b = 1.0f;
    else if (hue < 5.0f) r = x, b = 1.0f;
    else r = 1.0f, b = x;
    const auto channel = [](float value) { return static_cast<std::uint8_t>(std::lround(60.0f + value * 195.0f)); };
    return {channel(r), channel(g), channel(b), 255};
}

PixelPalette IndexPalette() {
    std::vector<Color4> colors;
    for (int index = 1; index < MaxInks; ++index)
        colors.push_back(IndexColor(index));
    return PixelPalette(colors);
}

std::string Hex(Color4 color) {
    return std::format("#{:02x}{:02x}{:02x}{}", color.r, color.g, color.b,
                       color.a == 255 ? std::string() : std::format("{:02x}", color.a));
}

class GraphicAssetScene final : public Scene {
public:
    GraphicAssetScene(const SceneServices& services, std::shared_ptr<const GraphicAsset> asset, std::size_t item,
                      const SceneParameters& parameters)
        : _asset(std::move(asset))
        , _item(item)
        , _canvas(services.canvas)
        , _box(PreviewBox(services.canvas)) {
        for (const auto& [id, value] : parameters)
            (void)ApplyParameter(id, value);
    }

    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        if (id == "view" && std::holds_alternative<std::string>(value)) {
            for (std::size_t i = 0; i < std::size(PreviewViewNames); ++i)
                if (PreviewViewNames[i] == std::get<std::string>(value)) _view = static_cast<PreviewView>(i);
            return {};
        }
        if (id == "grid" && std::holds_alternative<bool>(value)) {
            _grid = std::get<bool>(value);
            return {};
        }
        return Scene::ApplyParameter(id, value);
    }

    [[nodiscard]] SceneInspection Inspect() const override {
        SceneInspection inspection;
        const FloatRect focus = _view == PreviewView::Drawing || !_shown ? _box : _shown->area;
        inspection.focus = SceneContentBounds{static_cast<int>(focus.x), static_cast<int>(focus.y),
                                              static_cast<int>(focus.width), static_cast<int>(focus.height)};
        if (!_shown) return inspection;
        const PixelSprite& sprite = _shown->pixels.sprite;
        inspection.values.push_back({"Size", std::format("{} x {}", sprite.Width(), sprite.Height())});
        inspection.values.push_back({"Pivot", std::format("{}, {}", sprite.Pivot().x, sprite.Pivot().y)});
        inspection.values.push_back({"Zoom", std::format("{}x", _shown->zoom), SceneReadoutKind::Number});
        if (_hover) {
            const Ink ink = sprite.At(_hover->x, _hover->y);
            inspection.values.push_back({"Pixel", std::format("{}, {}", _hover->x, _hover->y)});
            inspection.values.push_back(
                {"Ink", ink == ClearInk ? std::string("0 (clear)")
                                        : std::format("{} {}", ink, Hex(_shown->pixels.palette[ink]))});
            inspection.values.push_back({"Part", std::to_string(sprite.PartAt(_hover->x, _hover->y))});
        }
        SceneTable inks{"Inks", {"Ink", "Colour", "Pixels"}, {}};
        std::array<int, MaxInks> counts{};
        for (const Ink ink : sprite.Inks())
            ++counts[ink];
        for (int ink = 1; ink < MaxInks; ++ink)
            if (counts[static_cast<std::size_t>(ink)] > 0)
                inks.rows.push_back({std::to_string(ink), Hex(_shown->pixels.palette[static_cast<Ink>(ink)]),
                                     std::to_string(counts[static_cast<std::size_t>(ink)])});
        inspection.tables.push_back(std::move(inks));
        return inspection;
    }

protected:
    void HandleInput(const DeviceInput& input) override {
        _hover.reset();
        if (!_shown || !input.mousePosition) return;
        const Float2 point = ToFloat2(*input.mousePosition);
        const FloatRect& area = _shown->area;
        if (!area.Contains(point)) return;
        const auto zoom = static_cast<float>(_shown->zoom);
        _hover = Vec2{static_cast<int>((point.x - area.x) / zoom), static_cast<int>((point.y - area.y) / zoom)};
    }

    void Draw(Painter& paint) const override {
        const float time = static_cast<float>(paint.Time());
        if (!_asset->pixels) {
            _asset->draw(paint, _box, _item, time);
            return;
        }
        Shown shown{_asset->pixels(_item, time), {}, 1};
        const PixelSprite& sprite = shown.pixels.sprite;
        if (_view == PreviewView::Drawing || sprite.Empty()) {
            _shown = sprite.Empty() ? std::nullopt : std::optional<Shown>(std::move(shown));
            _asset->draw(paint, _box, _item, time);
            return;
        }
        // Whole device pixels per sprite pixel, the largest that fits.
        const float device = paint.PixelScale();
        const float side = std::min(static_cast<float>(_canvas.x), static_cast<float>(_canvas.y)) * PixelArea;
        const float fit = std::min(side * device / static_cast<float>(sprite.Width()),
                                   side * device / static_cast<float>(sprite.Height()));
        shown.zoom = std::max(1, static_cast<int>(std::floor(fit)));
        const float unit = static_cast<float>(shown.zoom) / device;
        const Float2 size{static_cast<float>(sprite.Width()) * unit, static_cast<float>(sprite.Height()) * unit};
        shown.area = {std::round((static_cast<float>(_canvas.x) - size.x) * 0.5f * device) / device,
                      std::round((static_cast<float>(_canvas.y) - size.y) * 0.5f * device) / device, size.x, size.y};
        const FloatRect& area = shown.area;
        const float checker = std::max(unit, 8.0f / device);
        for (float y = 0.0f; y < area.height; y += checker)
            for (float x = 0.0f; x < area.width; x += checker) {
                const bool light = (static_cast<int>(x / checker) + static_cast<int>(y / checker)) % 2 == 0;
                paint.Rect({area.x + x, area.y + y, std::min(checker, area.width - x), std::min(checker, area.height - y)},
                           Fill(light ? CheckerLight : CheckerDark));
            }
        const Float2 pivot = ToFloat2(sprite.Pivot()) * unit;
        if (_view == PreviewView::Pixels) {
            paint.Sprite(sprite, shown.pixels.palette, area.Position() + pivot, {.scale = unit});
        } else if (_view == PreviewView::Inks) {
            paint.Sprite(sprite, _indexPalette, area.Position() + pivot, {.scale = unit});
        } else {
            if (_partsOf != sprite.Id()) {
                PixelCanvas parts(sprite.Width(), sprite.Height());
                for (int y = 0; y < sprite.Height(); ++y)
                    for (int x = 0; x < sprite.Width(); ++x)
                        if (sprite.At(x, y) != ClearInk)
                            parts.Set(x, y, static_cast<Ink>(std::min(sprite.PartAt(x, y) + 1, MaxInks - 1)));
                _parts = PixelSprite(parts, sprite.Pivot());
                _partsOf = sprite.Id();
            }
            paint.Sprite(_parts, _indexPalette, area.Position() + pivot, {.scale = unit});
        }
        if (_grid && shown.zoom >= GridFromZoom) {
            const LineStyle line{.width = 1.0f / device, .color = GridColor};
            for (int x = 1; x < sprite.Width(); ++x) {
                const float at = area.x + static_cast<float>(x) * unit;
                paint.Line({at, area.y}, {at, area.Bottom()}, line);
            }
            for (int y = 1; y < sprite.Height(); ++y) {
                const float at = area.y + static_cast<float>(y) * unit;
                paint.Line({area.x, at}, {area.Right(), at}, line);
            }
        }
        const Float2 mark = area.Position() + pivot;
        const float arm = 6.0f / device;
        paint.Line({mark.x - arm, mark.y}, {mark.x + arm, mark.y}, {.width = 2.0f / device, .color = PivotColor});
        paint.Line({mark.x, mark.y - arm}, {mark.x, mark.y + arm}, {.width = 2.0f / device, .color = PivotColor});
        if (_hover)
            paint.Rect({area.x + static_cast<float>(_hover->x) * unit, area.y + static_cast<float>(_hover->y) * unit,
                        unit, unit},
                       Outline(1.5f / device, Color4{255, 255, 255, 255}, StrokeAlign::Inside));
        _shown = std::move(shown);
    }

private:
    struct Shown {
        GraphicPixels pixels;
        FloatRect area; // Where the sprite's pixels are on the canvas
        int zoom = 1;   // Device pixels per sprite pixel
    };
    std::shared_ptr<const GraphicAsset> _asset;
    std::size_t _item;
    Vec2 _canvas;
    FloatRect _box;
    PreviewView _view = PreviewView::Pixels;
    bool _grid = true;
    PixelPalette _indexPalette = IndexPalette();
    std::optional<Vec2> _hover;
    mutable std::optional<Shown> _shown; // What the last frame drew, for the mouse and the Inspector
    mutable PixelSprite _parts;          // The Parts view's sprite, rebuilt when the sprite changes
    mutable std::uint64_t _partsOf = 0;
};

std::vector<SceneParameter> PixelParameters() {
    return {{.id = "view",
             .label = "View",
             .kind = SceneParameterKind::Choice,
             .initial = std::string("Pixels"),
             .choices = {std::begin(PreviewViewNames), std::end(PreviewViewNames)},
             .description = "Drawing: as the game draws it; Pixels: zoomed; Inks and Parts: a colour per ink or part",
             .choiceButtons = true},
            {.id = "grid", .label = "Grid", .kind = SceneParameterKind::Boolean, .initial = true}};
}

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
        if (shared->pixels) definition.parameters = PixelParameters();
        definition.create = [shared, item](const SceneServices& services, const SceneParameters& parameters) {
            return std::make_unique<GraphicAssetScene>(services, shared, item, parameters);
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
        const auto definitions = GraphicAssetDefinitions(*asset, maker.file, EngineRoot());
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
        for (auto& definition : GraphicAssetDefinitions(maker.make(), maker.file, EngineRoot()))
            registry.Add(std::move(definition));
}

void SetGraphicAssetEngineRoot(const std::filesystem::path& root) { EngineRoot() = root; }
