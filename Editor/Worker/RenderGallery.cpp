#include "Editor/Worker/RenderGallery.h"
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Paint/Path.h"
#include "Graphics/Surface/Surface.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr int Columns = 4;
constexpr int Rows = 3;
constexpr float Margin = 16.0f;
constexpr float TitleSize = 15.0f;
constexpr Color4 Panel = Rgb(0x141826);
constexpr Color4 Frame = Rgb(0x2a3150);
constexpr Color4 Ink = Rgb(0xe8ecff);
constexpr Color4 Muted = Rgb(0x8890b0);
constexpr Color4 Cyan = Rgb(0x30e0ff);
constexpr Color4 Pink = Rgb(0xff3fa4);
constexpr Color4 Lime = Rgb(0x9dff4a);
constexpr Color4 Gold = Rgb(0xffc640);
constexpr Color4 Violet = Rgb(0x8a5cff);

class RenderGallery final : public EditorScene {
public:
    RenderGallery(const SceneServices& services, const SceneParameters& parameters)
        : _canvas(services.canvas) {
        for (const auto& [id, value] : parameters)
            (void)ApplyParameter(id, value);
    }

    std::string ApplyParameter(const std::string& id, const SceneValue& value) override {
        if (id == "animate") _animate = std::get<bool>(value);
        if (id == "bloom") _bloom = static_cast<float>(std::get<double>(value));
        if (id == "vignette") _vignette = static_cast<float>(std::get<double>(value));
        if (id == "scanlines") _scanlines = static_cast<float>(std::get<double>(value));
        if (id == "aberration") _aberration = static_cast<float>(std::get<double>(value));
        return {};
    }

private:
    void Draw(Painter& paint) const override {
        PostSettings& post = paint.Post();
        post.bloom.intensity = _bloom;
        post.vignette = _vignette;
        post.scanlines = _scanlines;
        post.aberration = _aberration;
        const float time = _animate ? static_cast<float>(paint.Time()) : 0.0f;
        paint.Rect({0.0f, 0.0f, static_cast<float>(_canvas.x), static_cast<float>(_canvas.y)}, Fill(Rgb(0x0a0c14)));
        using Demo = void (RenderGallery::*)(Painter&, const FloatRect&, float) const;
        constexpr std::array<std::pair<const char*, Demo>, Columns * Rows> Panels{{
            {"Shapes", &RenderGallery::Shapes},
            {"Lines and caps", &RenderGallery::Lines},
            {"Joins", &RenderGallery::Joins},
            {"Arcs", &RenderGallery::Arcs},
            {"Glow", &RenderGallery::Glows},
            {"Paths", &RenderGallery::Paths},
            {"Text", &RenderGallery::Texts},
            {"Text effects", &RenderGallery::TextEffects},
            {"Opacity vs Group", &RenderGallery::Groups},
            {"Transform and clip", &RenderGallery::Clips},
            {"Surface", &RenderGallery::Surfaces},
            {"Bloom (emission)", &RenderGallery::Bloom},
        }};
        const float width = (static_cast<float>(_canvas.x) - Margin * (Columns + 1)) / Columns;
        const float height = (static_cast<float>(_canvas.y) - Margin * (Rows + 1)) / Rows;
        for (std::size_t i = 0; i < Panels.size(); ++i) {
            const auto column = static_cast<float>(i % Columns);
            const auto row = static_cast<float>(i / Columns);
            const FloatRect cell{Margin + column * (width + Margin), Margin + row * (height + Margin), width, height};
            auto marker = paint.Marker(Panels[i].first);
            paint.RoundRect(cell, 8.0f, {.fill = Panel, .stroke = {1.0f, Frame, StrokeAlign::Inside}});
            if (paint.HasFont())
                paint.Text(Panels[i].first, {cell.x + 12.0f, cell.y + 8.0f}, {.size = TitleSize, .color = Muted});
            const FloatRect body{cell.x + 12.0f, cell.y + 32.0f, cell.width - 24.0f, cell.height - 44.0f};
            (this->*Panels[i].second)(paint, body, time);
        }
    }

    void Shapes(Painter& paint, const FloatRect& area, float) const {
        const float size = std::min(area.width / 5.0f, area.height / 2.0f) - 10.0f;
        const auto slot = [&](int column, int row) {
            return FloatRect{area.x + static_cast<float>(column) * area.width / 5.0f + 5.0f,
                             area.y + static_cast<float>(row) * area.height / 2.0f + 5.0f, size, size};
        };
        paint.Rect(slot(0, 0), Fill(Cyan));
        paint.RoundRect(slot(1, 0), size * 0.25f, Fill(Pink));
        paint.ChamferRect(slot(2, 0), size * 0.25f, Fill(Lime));
        paint.Circle(slot(3, 0).Center(), size * 0.5f, Fill(Gold));
        paint.Ellipse(slot(4, 0).Center(), {size * 0.5f, size * 0.3f}, Fill(Violet));
        paint.RoundRect(slot(0, 1), 6.0f, {.stroke = {4.0f, Cyan, StrokeAlign::Inside}});
        paint.RoundRect(slot(1, 1), 6.0f, {.stroke = {4.0f, Pink, StrokeAlign::Center}});
        paint.RoundRect(slot(2, 1), 6.0f, {.stroke = {4.0f, Lime, StrokeAlign::Outside}});
        paint.Circle(slot(3, 1).Center(), size * 0.5f,
                     {.gradient = Gradient::Radial(size * 0.5f, Rgb(0xffffff), Gold), .stroke = {2.0f, Ink}});
        paint.RoundRect(slot(4, 1), 8.0f, {.gradient = Gradient::Vertical(size, Violet, Cyan)});
    }

    void Lines(Painter& paint, const FloatRect& area, float time) const {
        constexpr std::array<float, 6> Widths{0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
        constexpr std::array<StrokeCap, 3> Caps{StrokeCap::Butt, StrokeCap::Round, StrokeCap::Square};
        for (std::size_t i = 0; i < Widths.size(); ++i) {
            const float y = area.y + 8.0f + static_cast<float>(i) * area.height * 0.11f;
            for (std::size_t c = 0; c < Caps.size(); ++c) {
                const float x = area.x + 10.0f + static_cast<float>(c) * area.width * 0.22f;
                paint.Line({x, y}, {x + area.width * 0.16f, y + 3.0f},
                           {.width = Widths[i], .color = Ink, .cap = Caps[c]});
            }
        }
        const Float2 center{area.Right() - area.width * 0.2f, area.y + area.height * 0.55f};
        for (int i = 0; i < 12; ++i) {
            const float angle = time * 0.4f + static_cast<float>(i) * Pi / 6.0f;
            paint.Line(center + Float2::FromAngle(angle, 12.0f), center + Float2::FromAngle(angle, area.height * 0.4f),
                       {.width = 1.5f,
                        .color = WithAlpha(Cyan, 0.4f + 0.05f * static_cast<float>(i)),
                        .cap = StrokeCap::Round});
        }
    }

    void Joins(Painter& paint, const FloatRect& area, float time) const {
        constexpr std::array<StrokeJoin, 3> Joins{StrokeJoin::Miter, StrokeJoin::Bevel, StrokeJoin::Round};
        for (std::size_t j = 0; j < Joins.size(); ++j) {
            const float y = area.y + 14.0f + static_cast<float>(j) * area.height * 0.22f;
            std::vector<Float2> zigzag;
            for (int i = 0; i < 6; ++i)
                zigzag.push_back({area.x + 10.0f + static_cast<float>(i) * area.width * 0.11f,
                                  y + ((i % 2) == 0 ? 0.0f : 26.0f + 8.0f * std::sin(time + static_cast<float>(i)))});
            paint.Polyline(zigzag,
                           {.width = 9.0f, .color = WithAlpha(Lime, 0.8f), .cap = StrokeCap::Round, .join = Joins[j]});
        }
        std::vector<Float2> star;
        const Float2 center{area.Right() - area.width * 0.18f, area.Center().y};
        for (int i = 0; i < 10; ++i) {
            const float radius = (i % 2) == 0 ? area.height * 0.38f : area.height * 0.16f;
            star.push_back(center + Float2::FromAngle(time * 0.3f + static_cast<float>(i) * Pi / 5.0f, radius));
        }
        paint.Polygon(star, {.fill = WithAlpha(Gold, 0.35f), .stroke = {3.0f, Gold}});
    }

    void Arcs(Painter& paint, const FloatRect& area, float time) const {
        const float radius = std::min(area.width / 6.0f, area.height / 2.6f);
        for (int i = 0; i < 3; ++i) {
            const Float2 center{area.x + area.width * (0.18f + 0.32f * static_cast<float>(i)), area.Center().y};
            const float sweep = 2.0f * Pi * (0.15f + 0.8f * Fract(time * 0.15f + static_cast<float>(i) * 0.3f));
            paint.Arc(center, radius, -Pi / 2.0f, 2.0f * Pi, {.width = 6.0f, .color = Frame});
            paint.Arc(center, radius, -Pi / 2.0f, sweep,
                      {.width = 6.0f,
                       .color = i == 1 ? Pink : Cyan,
                       .cap = i == 0 ? StrokeCap::Butt : StrokeCap::Round,
                       .glow = {i == 2 ? 10.0f : 0.0f, Cyan}});
        }
    }

    void Glows(Painter& paint, const FloatRect& area, float time) const {
        const Float2 center = area.Center();
        const float orbit = area.height * 0.18f;
        constexpr std::array<Color4, 3> Lights{Rgb(0xff3030), Rgb(0x30ff40), Rgb(0x3060ff)};
        for (std::size_t i = 0; i < Lights.size(); ++i)
            paint.Glow(center + Float2::FromAngle(time + static_cast<float>(i) * 2.0f * Pi / 3.0f, orbit),
                       area.height * 0.35f, {.color = Lights[i]});
        paint.Circle({area.x + 30.0f, area.y + 30.0f}, 12.0f, {.stroke = {3.0f, Cyan}, .glow = {18.0f, Cyan}});
        paint.ChamferRect({area.Right() - 90.0f, area.y + 14.0f, 76.0f, 34.0f}, 8.0f,
                          {.fill = Rgb(0x10131f), .stroke = {2.0f, Pink}, .glow = {16.0f, Pink, 0.8f}});
        paint.Line({area.x + 20.0f, area.Bottom() - 18.0f}, {area.Right() - 20.0f, area.Bottom() - 18.0f},
                   {.width = 3.0f, .color = Lime, .cap = StrokeCap::Round, .glow = {14.0f, Lime}});
    }

    void Paths(Painter& paint, const FloatRect& area, float time) const {
        Path heart;
        const Float2 h{area.x + area.width * 0.22f, area.y + area.height * 0.3f};
        const float s = area.height * 0.32f;
        heart.MoveTo(h)
            .CubicTo(h + Float2{0.0f, -s * 0.6f}, h + Float2{-s * 1.2f, -s * 0.5f}, h + Float2{-s, s * 0.2f})
            .CubicTo(h + Float2{-s * 0.8f, s * 0.8f}, h + Float2{-s * 0.1f, s}, h + Float2{0.0f, s * 1.4f})
            .CubicTo(h + Float2{s * 0.1f, s}, h + Float2{s * 0.8f, s * 0.8f}, h + Float2{s, s * 0.2f})
            .CubicTo(h + Float2{s * 1.2f, -s * 0.5f}, h + Float2{0.0f, -s * 0.6f}, h)
            .Close();
        paint.FillPath(heart, {.fill = Pink, .stroke = {2.0f, Ink}});
        Path wave;
        const float left = area.x + area.width * 0.45f;
        wave.MoveTo({left, area.Center().y});
        for (int i = 0; i < 4; ++i) {
            const float x = left + static_cast<float>(i) * area.width * 0.12f;
            const float lift = area.height * 0.3f * std::sin(time + static_cast<float>(i));
            wave.QuadTo({x + area.width * 0.06f, area.Center().y - lift}, {x + area.width * 0.12f, area.Center().y});
        }
        paint.StrokePath(wave, {.width = 4.0f, .color = Cyan, .cap = StrokeCap::Round, .join = StrokeJoin::Round});
    }

    void Texts(Painter& paint, const FloatRect& area, float) const {
        if (!paint.HasFont()) return;
        constexpr std::array<float, 6> Sizes{10.0f, 13.0f, 16.0f, 22.0f, 30.0f, 44.0f};
        float y = area.y;
        for (const float size : Sizes) {
            paint.Text("Glyphs ağçşİı " + std::to_string(static_cast<int>(size)), {area.x, y},
                       {.size = size, .color = Ink});
            y += size * 1.15f;
        }
        paint.TextBox("Wrapped text stays inside its box and keeps words together.",
                      {area.Right() - area.width * 0.42f, area.y, area.width * 0.42f, area.height},
                      {.size = 13.0f, .color = Muted, .align = TextAlign::Right, .wrap = TextWrap::Word});
    }

    void TextEffects(Painter& paint, const FloatRect& area, float time) const {
        if (!paint.HasFont()) return;
        paint.Text("NEON", {area.x + 10.0f, area.y + 4.0f},
                   {.size = 40.0f, .color = Rgb(0xd8fbff), .glow = {12.0f, Cyan, 1.2f}, .emission = 0.6f});
        paint.Text("OUTLINE", {area.x + 10.0f, area.y + 56.0f},
                   {.size = 30.0f, .color = Gold, .outline = {3.0f, Rgb(0x402000)}});
        paint.Text("Shadow", {area.x + 10.0f, area.y + 98.0f},
                   {.size = 24.0f, .color = Ink, .shadow = {{2.0f, 3.0f}, Rgb(0x000000, 200)}});
        const Float2 pivot{area.Right() - area.width * 0.22f, area.Center().y};
        auto pulse = paint.Transform(Affine2::Scale(1.0f + 0.35f * std::sin(time * 1.5f), pivot) *
                                     Affine2::Rotate(0.15f * std::sin(time), pivot));
        paint.Text("Scale", pivot - Float2{0.0f, 14.0f}, {.size = 28.0f, .color = Pink, .align = TextAlign::Center});
    }

    void Groups(Painter& paint, const FloatRect& area, float time) const {
        const float fade = 0.5f + 0.4f * std::sin(time);
        const float radius = area.height * 0.25f;
        const Float2 left{area.x + area.width * 0.25f, area.Center().y};
        const Float2 right{area.x + area.width * 0.72f, area.Center().y};
        {
            auto faded = paint.Opacity(fade);
            paint.Circle(left - Float2{radius * 0.45f, 0.0f}, radius, Fill(Cyan));
            paint.Circle(left + Float2{radius * 0.45f, 0.0f}, radius, Fill(Cyan));
        }
        {
            auto group = paint.Group({.opacity = fade});
            paint.Circle(right - Float2{radius * 0.45f, 0.0f}, radius, Fill(Cyan));
            paint.Circle(right + Float2{radius * 0.45f, 0.0f}, radius, Fill(Cyan));
        }
        if (paint.HasFont()) {
            paint.Text("Opacity", {left.x, area.Bottom() - 18.0f},
                       {.size = 12.0f, .color = Muted, .align = TextAlign::Center});
            paint.Text("Group", {right.x, area.Bottom() - 18.0f},
                       {.size = 12.0f, .color = Muted, .align = TextAlign::Center});
        }
    }

    void Clips(Painter& paint, const FloatRect& area, float time) const {
        const FloatRect list{area.x, area.y, area.width * 0.45f, area.height};
        {
            auto clip = paint.Clip(list);
            const float scroll = std::fmod(time * 30.0f, 40.0f);
            for (int i = -1; i < 8; ++i)
                paint.RoundRect({list.x, list.y + static_cast<float>(i) * 40.0f - scroll, list.width, 32.0f}, 6.0f,
                                Fill((i % 2) == 0 ? Frame : Rgb(0x1e2440)));
        }
        paint.Rect(list, Outline(1.0f, Muted, StrokeAlign::Outside));
        const Float2 center{area.Right() - area.width * 0.25f, area.Center().y};
        auto spin = paint.Transform(Affine2::Rotate(time * 0.7f, center));
        paint.RoundRect(FloatRect::FromCenter(center, {area.height * 0.5f, area.height * 0.5f}), 6.0f,
                        {.fill = Violet, .stroke = {2.0f, Ink}});
        paint.Circle(center, area.height * 0.08f, Fill(Gold));
    }

    void Surfaces(Painter& paint, const FloatRect& area, float time) const {
        if (paint.Stale(_cached)) { // Drawn once per resolution, then reused
            auto into = paint.Target(_cached);
            paint.RoundRect({0.0f, 0.0f, 120.0f, 80.0f}, 10.0f, {.gradient = Gradient::Vertical(80.0f, Pink, Violet)});
            paint.Circle({60.0f, 40.0f}, 22.0f, {.fill = Gold, .stroke = {3.0f, Ink}});
        }
        for (int i = 0; i < 3; ++i) {
            const Float2 center{area.x + area.width * (0.2f + 0.3f * static_cast<float>(i)), area.Center().y};
            const float scale = 0.6f + 0.25f * static_cast<float>(i);
            paint.Image(_cached, FloatRect::FromCenter(center, Float2{120.0f, 80.0f} * scale),
                        {.rotation = time * (0.3f + 0.2f * static_cast<float>(i))});
        }
    }

    void Bloom(Painter& paint, const FloatRect& area, float time) const {
        const Float2 center = area.Center();
        for (int i = 0; i < 5; ++i) {
            const float x = area.x + area.width * (0.12f + 0.19f * static_cast<float>(i));
            paint.Circle({x, center.y + 20.0f * std::sin(time * 2.0f + static_cast<float>(i))}, 9.0f,
                         {.fill = Rgb(0xffffff), .emission = 0.25f * static_cast<float>(i)});
        }
        paint.Line({area.x + 10.0f, area.y + 14.0f}, {area.Right() - 10.0f, area.y + 14.0f},
                   {.width = 3.0f, .color = Pink, .emission = 1.0f});
        paint.Rect({area.x + 10.0f, area.Bottom() - 30.0f, area.width - 20.0f, 20.0f}, Fill(Rgb(0x1a1f33)));
        if (paint.HasFont())
            paint.Text("emission 0 .. 1 (set Bloom above 0)", {center.x, area.Bottom() - 28.0f},
                       {.size = 12.0f, .color = Muted, .align = TextAlign::Center});
    }

    Vec2 _canvas;
    mutable Surface _cached{{120.0f, 80.0f}};
    bool _animate = true;
    float _bloom = 0.8f;
    float _vignette = 0.0f;
    float _scanlines = 0.0f;
    float _aberration = 0.0f;
};

SceneParameter Slider(const char* id, const char* label, double initial, double maximum) {
    return {.id = id,
            .label = label,
            .kind = SceneParameterKind::Float,
            .initial = initial,
            .maximum = maximum,
            .group = "Post-processing"};
}
} // namespace

SceneDefinition RenderGalleryDefinition() {
    SceneDefinition definition;
    definition.id = "engine.render.gallery";
    definition.name = "Render Gallery";
    definition.category = "Engine";
    definition.description = "Every Painter feature on one canvas: shapes, lines, glows, paths, text, groups, "
                             "clipping, surfaces and post-processing.";
    definition.parameters = {
        {.id = "animate", .label = "Animate", .kind = SceneParameterKind::Boolean, .initial = true},
        Slider("bloom", "Bloom", 0.8, 2.0),
        Slider("vignette", "Vignette", 0.0, 1.0),
        Slider("scanlines", "Scanlines", 0.0, 1.0),
        Slider("aberration", "Aberration", 0.0, 4.0),
    };
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RenderGallery>(services, parameters);
    };
    return definition;
}
