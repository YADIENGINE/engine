#include "Editor/Worker/LightGallery.h"
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr Color4 FloorA = Rgb(0x8f8a80);
constexpr Color4 FloorB = Rgb(0x77736b);
constexpr Color4 Warm = Rgb(0xffc98a);
constexpr Color4 Cold = Rgb(0x8ab8ff);
constexpr Color4 Green = Rgb(0x9dff8a);
constexpr Color4 Neon = Rgb(0xff3fa4);
constexpr float Tile = 80.0f;

class LightGallery final : public EditorScene {
public:
    LightGallery(const SceneServices& services, const SceneParameters& parameters)
        : _size(static_cast<float>(services.canvas.x), static_cast<float>(services.canvas.y)) {
        for (const auto& [id, value] : parameters)
            (void)ApplyParameter(id, value);
    }

    std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        if (id == "animate") _animate = std::get<bool>(value);
        if (id == "guides") _guides = std::get<bool>(value);
        if (id == "split") _split = std::get<bool>(value);
        if (id == "ambient") _ambient = static_cast<float>(std::get<double>(value));
        if (id == "bloom") _bloom = static_cast<float>(std::get<double>(value));
        if (id == "zoom") _zoom = static_cast<float>(std::get<double>(value));
        if (id == "turn") _turn = static_cast<float>(std::get<double>(value));
        if (id == "resolution") _layer = LightLayer({.resolution = static_cast<float>(std::get<double>(value))});
        return {};
    }

private:
    void Draw(Painter& paint) const override {
        paint.Post().bloom.intensity = _bloom;
        const float time = _animate ? static_cast<float>(paint.Time()) : 0.0f;
        const Float2 center = _size * 0.5f;
        const std::array lights{
            LightSource{.at = center + Float2::FromAngle(time * 0.7f, 220.0f), .radius = 240.0f, .color = Warm},
            LightSource{.at = {center.x - 360.0f, center.y + 150.0f},
                        .radius = 420.0f,
                        .color = Cold,
                        .cone = {.direction = -0.5f + std::sin(time * 0.6f) * 0.6f, .halfAngle = 0.45f}},
            LightSource{.at = {center.x + 360.0f, center.y - 120.0f}, .radius = 170.0f, .color = Green, .core = 0.6f},
        };
        if (!_split) {
            DrawView(paint, {.offset = center, .target = center, .rotation = _turn, .zoom = _zoom}, lights);
            return;
        }
        const Float2 half{_size.x * 0.5f, _size.y};
        const CameraView left{.offset = half * 0.5f,
                              .target = lights[0].at,
                              .rotation = _turn,
                              .zoom = _zoom,
                              .viewport = {0.0f, 0.0f, half.x, half.y}};
        const CameraView right{.offset = {half.x * 1.5f, half.y * 0.5f},
                               .target = lights[1].at,
                               .rotation = _turn,
                               .zoom = _zoom,
                               .viewport = {half.x, 0.0f, half.x, half.y}};
        DrawView(paint, left, lights);
        DrawView(paint, right, lights);
    }

    void DrawView(Painter& paint, const CameraView& view, std::span<const LightSource> lights) const {
        {
            auto world = paint.Camera(view);
            DrawFloor(paint);
            DrawSign(paint, {_size.x * 0.5f - 140.0f, _size.y * 0.5f});
        }
        const LightFrame frame{.ambient = Ambient(), .lights = lights};
        _layer.Draw(paint, frame, view);
        {
            auto world = paint.Camera(view);
            DrawSign(paint, {_size.x * 0.5f + 140.0f, _size.y * 0.5f}); // After the layer: at its full colour
        }
        if (_guides) DrawLightGuides(paint, frame, view);
    }

    void DrawFloor(Painter& paint) const {
        const int columns = static_cast<int>(_size.x / Tile) + 1;
        const int rows = static_cast<int>(_size.y / Tile) + 1;
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column)
                paint.Rect({static_cast<float>(column) * Tile, static_cast<float>(row) * Tile, Tile, Tile},
                           Fill((row + column) % 2 == 0 ? FloorA : FloorB));
        paint.Circle({_size.x * 0.25f, _size.y * 0.3f}, 42.0f, Fill(Rgb(0xd04a3a)));
        paint.RoundRect({_size.x * 0.68f, _size.y * 0.66f, 150.0f, 90.0f}, 12.0f, Fill(Rgb(0x3a7bd0)));
        paint.ChamferRect({_size.x * 0.42f, _size.y * 0.74f, 120.0f, 70.0f}, 14.0f, Fill(Rgb(0xd0b03a)));
    }

    static void DrawSign(Painter& paint, Float2 at) {
        paint.RoundRect(FloatRect::FromCenter(at, {150.0f, 54.0f}), 14.0f,
                        {.stroke = {5.0f, Neon}, .emission = 1.0f});
        paint.Line(at - Float2{50.0f, 0.0f}, at + Float2{50.0f, 0.0f},
                   {.width = 5.0f, .color = Neon, .cap = StrokeCap::Round, .emission = 1.0f});
    }

    [[nodiscard]] Color4 Ambient() const {
        const auto channel = [&](float share) { return static_cast<std::uint8_t>(_ambient * share * 255.0f); };
        return {channel(0.8f), channel(0.9f), channel(1.0f), 255};
    }

    Float2 _size;
    LightLayer _layer;
    bool _animate = true;
    bool _guides = false;
    bool _split = false;
    float _ambient = 0.12f;
    float _bloom = 0.8f;
    float _zoom = 1.0f;
    float _turn = 0.0f;
};

SceneParameter Slider(const char* id, const char* label, double initial, double minimum, double maximum) {
    return {.id = id,
            .label = label,
            .kind = SceneParameterKind::Float,
            .initial = initial,
            .minimum = minimum,
            .maximum = maximum};
}
} // namespace

SceneDefinition LightGalleryDefinition() {
    SceneDefinition definition;
    definition.id = "engine.light.gallery";
    definition.name = "Light Gallery";
    definition.category = "Engine";
    definition.description = "The light layer: point lights, a cone and a core over a floor, a sign that shines by "
                             "itself before and after the layer, camera and split screen.";
    definition.parameters = {
        {.id = "animate", .label = "Animate", .kind = SceneParameterKind::Boolean, .initial = true},
        {.id = "guides", .label = "Guides", .kind = SceneParameterKind::Boolean, .initial = false},
        {.id = "split", .label = "Split screen", .kind = SceneParameterKind::Boolean, .initial = false},
        Slider("ambient", "Ambient", 0.12, 0.0, 1.0),
        Slider("resolution", "Resolution", 0.5, 0.25, 1.0),
        Slider("bloom", "Bloom", 0.8, 0.0, 2.0),
        Slider("zoom", "Zoom", 1.0, 0.25, 3.0),
        Slider("turn", "Turn", 0.0, -3.2, 3.2),
    };
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<LightGallery>(services, parameters);
    };
    return definition;
}
