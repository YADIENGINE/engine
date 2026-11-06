#include "Editor/Worker/PostGallery.h"
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr const char* ModeSliders = "Sliders";
constexpr const char* ModeAmount = "Amount";
constexpr const char* ModeBreathing = "Breathing amount";

constexpr PostSettings Calm() {
    PostSettings post;
    post.vignette = 0.25f;
    post.grain.amount = 0.06f;
    return post;
}

constexpr PostSettings Disturbed() {
    PostSettings post;
    post.vignette = 0.4f;
    post.aberration = 1.5f;
    post.fade = 0.6f;
    post.tint = {.color = Rgb(0x8c9ccc), .amount = 0.8f};
    post.grain.amount = 0.3f;
    post.wobble = {.amount = 6.0f, .speed = 1.8f};
    post.tape = {.band = 0.7f, .shift = 2.0f, .speed = 1.5f};
    return post;
}

class PostGallery final : public Scene {
public:
    PostGallery(const SceneServices& services, const SceneParameters& parameters)
        : _size(static_cast<float>(services.canvas.x), static_cast<float>(services.canvas.y)) {
        for (const auto& [id, value] : parameters)
            (void)ApplyParameter(id, value);
    }
    ~PostGallery() override { ShutdownNoThrow(); }

    std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        const auto number = [&] { return static_cast<float>(std::get<double>(value)); };
        if (id == "mode") _mode = std::get<std::string>(value);
        if (id == "amount") _amount = number();
        if (id == "screenMotion") _screenMotion = std::get<bool>(value);
        if (id == "fade") _sliders.fade = number();
        if (id == "tint") _sliders.tint.amount = number();
        if (id == "tintColor") _sliders.tint.color = std::get<Color4>(value);
        if (id == "grain") _sliders.grain.amount = number();
        if (id == "grainSize") _sliders.grain.size = number();
        if (id == "wobble") _sliders.wobble.amount = number();
        if (id == "wobbleSpeed") _sliders.wobble.speed = number();
        if (id == "band") _sliders.tape.band = number();
        if (id == "shift") _sliders.tape.shift = number();
        if (id == "tapeSpeed") _sliders.tape.speed = number();
        if (id == "vignette") _sliders.vignette = number();
        return {};
    }

private:
    [[nodiscard]] float Amount(double time) const {
        if (_mode == ModeBreathing) return 0.5f - 0.5f * std::cos(static_cast<float>(time) * 0.6f);
        return _amount;
    }

    void Draw(Painter& paint) const override {
        const double time = paint.Time();
        const float amount = Amount(time);
        PostSettings post = _mode == ModeSliders ? _sliders : Mix(Calm(), Disturbed(), amount);
        if (!_screenMotion) { // What a player sees with RenderQuality::screenMotion off
            post.grain.amount = 0.0f;
            post.wobble.amount = 0.0f;
            post.tape = {};
        }
        paint.Post() = post;
        DrawView(paint, static_cast<float>(time));
        DrawRamp(paint);
        DrawBars(paint);
        DrawHud(paint, amount);
    }

    void DrawView(Painter& paint, float time) const {
        paint.Rect({0.0f, 0.0f, _size.x, _size.y}, {.gradient = Gradient::Vertical(_size.y, Rgb(0x10141f), Rgb(0x05060a))});
        const Float2 moon{_size.x * 0.72f, _size.y * 0.24f};
        paint.Circle(moon, 46.0f, Fill(Rgb(0xd8dcc8)));
        paint.Circle(moon + Float2{-14.0f, -8.0f}, 9.0f, Fill(Rgb(0xb8bca8)));
        const float ground = _size.y * 0.72f;
        paint.Rect({0.0f, ground, _size.x, _size.y - ground}, Fill(Rgb(0x0b0d12)));
        for (int i = 0; i < 9; ++i) {
            const float x = _size.x * (0.06f + 0.11f * static_cast<float>(i));
            const float height = 90.0f + 60.0f * std::fmod(static_cast<float>(i) * 0.37f, 1.0f) * 2.0f;
            paint.Rect({x, ground - height, 44.0f, height}, Fill(Rgb(0x1a1e2a)));
            paint.Rect({x + 15.0f, ground - height * 0.6f, 14.0f, 18.0f}, Fill(i % 3 == 0 ? Rgb(0xd9b36a) : Rgb(0x242938)));
        }
        const Float2 walker{_size.x * (0.5f + 0.3f * std::sin(time * 0.4f)), ground + 34.0f};
        paint.Circle(walker - Float2{0.0f, 32.0f}, 10.0f, Fill(Rgb(0xc04a3a)));
        paint.RoundRect({walker.x - 10.0f, walker.y - 22.0f, 20.0f, 34.0f}, 5.0f, Fill(Rgb(0x8f3a30)));
    }

    // Where the grain's strength shows: it should be stronger on the dark steps.
    void DrawRamp(Painter& paint) const {
        constexpr int Steps = 12;
        const float width = _size.x * 0.5f / Steps;
        for (int i = 0; i < Steps; ++i) {
            const auto level = static_cast<std::uint8_t>(i * 255 / (Steps - 1));
            paint.Rect({_size.x * 0.25f + static_cast<float>(i) * width, _size.y - 110.0f, width, 26.0f},
                       Fill({level, level, level, 255}));
        }
    }

    // Where the fade and the tint show.
    void DrawBars(Painter& paint) const {
        constexpr std::array<Color4, 6> Bars{Rgb(0xff3030), Rgb(0xffc030), Rgb(0x40e040),
                                             Rgb(0x30d0ff), Rgb(0x4060ff), Rgb(0xff40c0)};
        const float width = _size.x * 0.5f / static_cast<float>(Bars.size());
        for (std::size_t i = 0; i < Bars.size(); ++i)
            paint.Rect({_size.x * 0.25f + static_cast<float>(i) * width, _size.y - 80.0f, width, 26.0f}, Fill(Bars[i]));
    }

    // Text at the edges and in the middle: the corners should stay still and readable.
    void DrawHud(Painter& paint, float amount) const {
        constexpr Color4 Ink = Rgb(0xf0f0e8);
        paint.Rect({24.0f, 24.0f, 220.0f, 16.0f}, Fill(Rgb(0x30343f)));
        paint.Rect({24.0f, 24.0f, 150.0f, 16.0f}, Fill(Rgb(0xd04a3a)));
        if (!paint.HasFont()) return;
        paint.Text("SCORE 012450", {_size.x - 24.0f, 22.0f}, {.size = 22.0f, .color = Ink, .align = TextAlign::Right});
        paint.Text("Press E to open the door", {_size.x * 0.5f, _size.y * 0.5f},
                   {.size = 24.0f, .color = Ink, .align = TextAlign::Center});
        paint.Text("Small text: the quick brown fox jumps over the lazy dog 0123456789",
                   {24.0f, _size.y - 42.0f}, {.size = 14.0f, .color = Ink});
        if (_mode != ModeSliders)
            paint.Text("amount " + std::to_string(amount).substr(0, 4), {24.0f, 48.0f}, {.size = 16.0f, .color = Ink});
    }

    Float2 _size;
    PostSettings _sliders;
    std::string _mode = ModeSliders;
    float _amount = 0.0f;
    bool _screenMotion = true;
};

SceneParameter Slider(const char* id, const char* label, const char* group, double initial, double maximum) {
    return {.id = id,
            .label = label,
            .kind = SceneParameterKind::Float,
            .initial = initial,
            .maximum = maximum,
            .group = group};
}
} // namespace

SceneDefinition PostGalleryDefinition() {
    SceneDefinition definition;
    definition.id = "engine.post.gallery";
    definition.name = "Screen Finish Gallery";
    definition.category = "Engine";
    definition.description = "Film grain, colour fade, tint, wobble and tape, one by one with the sliders, or all "
                             "together through a single amount between a calm and a disturbed finish.";
    definition.parameters = {
        {.id = "mode",
         .label = "Show",
         .kind = SceneParameterKind::Choice,
         .initial = std::string(ModeSliders),
         .choices = {ModeSliders, ModeAmount, ModeBreathing},
         .group = "Show",
         .choiceButtons = true},
        Slider("amount", "Amount (calm to disturbed)", "Show", 0.0, 1.0),
        {.id = "screenMotion",
         .label = "Player's screen motion",
         .kind = SceneParameterKind::Boolean,
         .initial = true,
         .group = "Show",
         .description = "Shows what is left when a player turns RenderQuality::screenMotion off."},
        Slider("fade", "Fade", "Colour", 0.0, 1.0),
        Slider("tint", "Tint", "Colour", 0.0, 1.0),
        {.id = "tintColor",
         .label = "Tint colour",
         .kind = SceneParameterKind::Color,
         .initial = Rgb(0x8c9ccc),
         .group = "Colour"},
        Slider("vignette", "Vignette", "Colour", 0.0, 1.0),
        Slider("grain", "Grain", "Grain", 0.0, 1.0),
        Slider("grainSize", "Grain size", "Grain", 1.0, 4.0),
        Slider("wobble", "Wobble", "Wobble", 0.0, 16.0),
        Slider("wobbleSpeed", "Wobble speed", "Wobble", 1.0, 5.0),
        Slider("band", "Tracking band", "Tape", 0.0, 1.0),
        Slider("shift", "Line shift", "Tape", 0.0, 8.0),
        Slider("tapeSpeed", "Band speed", "Tape", 1.0, 5.0),
    };
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<PostGallery>(services, parameters);
    };
    return definition;
}
