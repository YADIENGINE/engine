#include "Editor/Worker/LightGallery.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include "Animation/Flicker.h"
#include "Fx/FxCue.h"
#include "Fx/FxStage.h"
#include "Graphics/Light/CollisionOccluders.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/Shape.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr Color4 FloorA = Rgb(0x8f8a80);
constexpr Color4 FloorB = Rgb(0x77736b);
constexpr Color4 Warm = Rgb(0xffc98a);
constexpr Color4 Cold = Rgb(0x8ab8ff);
constexpr Color4 Green = Rgb(0x9dff8a);
constexpr Color4 Neon = Rgb(0xff3fa4);
constexpr Color4 CandleColor = Rgb(0xff9a4a);
constexpr Color4 TubeColor = Rgb(0xd8f4ff);
constexpr Color4 FireColor = Rgb(0xffa040);
constexpr Color4 SkyColor = Rgb(0xb8ccff);
constexpr unsigned CandleSeed = 1U;
constexpr unsigned TubeSeed = 2U;
constexpr LightFlashStyle Lightning{.seconds = 0.45f, .strength = 0.8f};
constexpr LightFlashStyle Explosion{.radius = 360.0f, .seconds = 0.5f, .strength = 1.5f};
constexpr Color4 WallColor = Rgb(0x5a5f73);
constexpr Color4 CrateColor = Rgb(0xb5793a);
constexpr float Tile = 80.0f;
constexpr float CrateSize = 34.0f;
enum class Layer { Wall };

class LightGallery final : public EditorScene {
public:
    LightGallery(const SceneServices& services, const SceneParameters& parameters)
        : _size(static_cast<float>(services.canvas.x), static_cast<float>(services.canvas.y))
        , _fx(FloatRect{0.0f, 0.0f, _size.x, _size.y}, FloatRect{0.0f, 0.0f, _size.x, _size.y}, CurrentTheme().fx) {
        for (const auto& [id, value] : parameters)
            (void)ApplyParameter(id, value);
        // Walls that stand still come from their colliders.
        const Float2 center = _size * 0.5f;
        const auto wall = MaskOf(Layer::Wall);
        (void)_colliders.Add(Shape::Box(150.0f, 12.0f), {center + Float2{-20.0f, -130.0f}}, wall);
        (void)_colliders.Add(Shape::Box(12.0f, 90.0f), {center + Float2{120.0f, 90.0f}, 0.3f}, wall);
        (void)_colliders.Add(Shape::Circle(36.0f), {center + Float2{-170.0f, 40.0f}}, wall);
        (void)_colliders.Add(Shape::Capsule({-60.0f, 0.0f}, {60.0f, 0.0f}, 14.0f),
                             {center + Float2{340.0f, 90.0f}, 0.8f}, wall);
        (void)_colliders.Add(Shape::Segment({-70.0f, 0.0f}, {70.0f, 0.0f}),
                             {center + Float2{-420.0f, -100.0f}, -0.6f}, wall);
        AddOccluders(_walls, _colliders, wall);
    }

    std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        if (id == "animate") _animate = std::get<bool>(value);
        if (id == "guides") _guides = std::get<bool>(value);
        if (id == "split") _split = std::get<bool>(value);
        if (id == "shadows") _shadows = std::get<bool>(value);
        if (id == "ambient") _ambient = static_cast<float>(std::get<double>(value));
        if (id == "bloom") _bloom = static_cast<float>(std::get<double>(value));
        if (id == "zoom") _zoom = static_cast<float>(std::get<double>(value));
        if (id == "turn") _turn = static_cast<float>(std::get<double>(value));
        if (id == "flashes") {
            _flashes = std::get<bool>(value);
            _fx.SetQuality({.flashes = _flashes});
        }
        if (id == "resolution" || id == "softness") {
            (id == "resolution" ? _resolution : _softness) = static_cast<float>(std::get<double>(value));
            _layer = LightLayer({.resolution = _resolution, .softness = _softness});
        }
        return {};
    }

    std::string ExecuteAction(const std::string& id) override {
        const Float2 center = _size * 0.5f;
        // Cues, as a game would play them. The explosion stands behind a wall, so its shadow shows.
        if (id == "lightning") _fx.Play(FxCue{FxLightFlash{Lightning}}, {.color = SkyColor});
        if (id == "explosion")
            _fx.Play(FxCue{FxLightFlash{Explosion}}, {.at = center + Float2{-20.0f, -40.0f}, .color = FireColor});
        return {};
    }

private:
    void Update(float seconds) override { _fx.Update(seconds, seconds); }

    void Draw(Painter& paint) const override {
        paint.Post().bloom.intensity = _bloom;
        const float time = _animate ? static_cast<float>(paint.Time()) : 0.0f;
        const double flicker = _animate ? paint.Time() : 0.0;
        const Float2 center = _size * 0.5f;
        const std::array lights{
            LightSource{.at = center + Float2::FromAngle(time * 0.7f, 220.0f), .radius = 240.0f, .color = Warm},
            LightSource{.at = {center.x - 360.0f, center.y + 150.0f},
                        .radius = 420.0f,
                        .color = Cold,
                        .cone = {.direction = -0.5f + std::sin(time * 0.6f) * 0.6f, .halfAngle = 0.45f}},
            LightSource{.at = {center.x + 360.0f, center.y - 120.0f}, .radius = 170.0f, .color = Green, .core = 0.6f},
            LightSource{.at = {center.x - 40.0f, center.y + 240.0f},
                        .radius = 150.0f,
                        .color = CandleColor,
                        .intensity = Flicker(flicker, FlickerStyle::Candle(), CandleSeed, _flashes)},
            LightSource{.at = {center.x + 420.0f, center.y + 230.0f},
                        .radius = 190.0f,
                        .color = TubeColor,
                        .intensity = Flicker(flicker, FlickerStyle::DeadFluorescent(), TubeSeed, _flashes)},
        };
        if (!_split) {
            DrawView(paint, {.offset = center, .target = center, .rotation = _turn, .zoom = _zoom}, lights, time);
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
        DrawView(paint, left, lights, time);
        DrawView(paint, right, lights, time);
    }

    void DrawView(Painter& paint, const CameraView& view, std::span<const LightSource> lights, float time) const {
        // What moves gives its outline from where it is drawn, not from a collider.
        const Float2 crate = _size * 0.5f + Float2::FromAngle(-time * 0.5f, 120.0f);
        const Affine2 crateAt = Affine2::Translate(crate) * Affine2::Rotate(time * 0.8f);
        const std::array<Float2, 4> crateOutline{Float2{-CrateSize, -CrateSize}, Float2{CrateSize, -CrateSize},
                                                 Float2{CrateSize, CrateSize}, Float2{-CrateSize, CrateSize}};
        {
            auto world = paint.Camera(view);
            DrawFloor(paint);
            DrawWalls(paint);
            {
                auto placed = paint.Transform(crateAt);
                paint.Polygon(crateOutline, Fill(CrateColor));
            }
            DrawSign(paint, {_size.x * 0.5f - 140.0f, _size.y * 0.5f + 40.0f}); // Before the layer: its colour goes dark
        }
        LightOccluders moving;
        moving.Outline(crateOutline, crateAt);
        const std::array<const LightOccluders*, 2> sets{&_walls, &moving};
        LightFrame frame{.ambient = Ambient(), .lights = lights, .flashes = _fx.LightFlashes()};
        if (_shadows) frame.occluders = sets;
        _layer.Draw(paint, frame, view);
        {
            auto world = paint.Camera(view);
            DrawSign(paint, {_size.x * 0.5f + 140.0f, _size.y * 0.5f + 40.0f}); // After the layer: at its full colour
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

    void DrawWalls(Painter& paint) const {
        _colliders.ForEach([&](const ColliderView& collider) {
            const std::span<const Float2> own = collider.shape.Points();
            const Affine2 at = Affine2::Translate(collider.pose.position) * Affine2::Rotate(collider.pose.angle);
            auto placed = paint.Transform(at);
            switch (collider.shape.Kind()) {
            case ShapeKind::Circle: paint.Circle(own[0], collider.shape.Radius(), Fill(WallColor)); break;
            case ShapeKind::Capsule:
            case ShapeKind::Segment:
                paint.Line(own[0], own[1],
                           {.width = std::max(collider.shape.Radius() * 2.0f, 3.0f),
                            .color = WallColor,
                            .cap = StrokeCap::Round});
                break;
            default: paint.Polygon(own, Fill(WallColor)); break;
            }
        });
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
    FxStage _fx;
    LightLayer _layer;
    CollisionWorld _colliders;
    LightOccluders _walls;
    bool _animate = true;
    bool _guides = false;
    bool _split = false;
    bool _shadows = true;
    bool _flashes = true;
    float _resolution = 0.5f;
    float _softness = 8.0f;
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
    definition.description = "The light layer: point lights, a cone and a core over a floor, walls from colliders and "
                             "a moving crate that cast shadows with soft edges, a candle and a failing tube that "
                             "flicker, lightning and an explosion played as cues, a sign that shines by itself before "
                             "and after the layer, camera and split screen.";
    definition.parameters = {
        {.id = "animate", .label = "Animate", .kind = SceneParameterKind::Boolean, .initial = true},
        {.id = "guides", .label = "Guides", .kind = SceneParameterKind::Boolean, .initial = false},
        {.id = "split", .label = "Split screen", .kind = SceneParameterKind::Boolean, .initial = false},
        {.id = "shadows", .label = "Shadows", .kind = SceneParameterKind::Boolean, .initial = true},
        {.id = "flashes", .label = "Flashes", .kind = SceneParameterKind::Boolean, .initial = true},
        Slider("softness", "Shadow softness", 8.0, 0.0, 30.0),
        Slider("ambient", "Ambient", 0.12, 0.0, 1.0),
        Slider("resolution", "Resolution", 0.5, 0.25, 1.0),
        Slider("bloom", "Bloom", 0.8, 0.0, 2.0),
        Slider("zoom", "Zoom", 1.0, 0.25, 3.0),
        Slider("turn", "Turn", 0.0, -3.2, 3.2),
    };
    definition.actions = {{"lightning", "Lightning", "A short light over the whole scene.", true},
                          {"explosion", "Explosion", "A short light in one place, with shadows."}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<LightGallery>(services, parameters);
    };
    return definition;
}
