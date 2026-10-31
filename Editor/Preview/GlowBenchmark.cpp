#include "Editor/Preview/GlowBenchmark.h"
#include <cmath>
#include <random>
#include "Graphics/Paint/Painter.h"

namespace {
class GlowBenchmark final : public EditorPreview {
public:
    GlowBenchmark(Vec2 canvas, const PreviewParameters& parameters) {
        const int count = std::stoi(std::get<std::string>(parameters.at("count")));
        const bool cluster = std::get<bool>(parameters.at("overlap"));
        _radius = static_cast<float>(std::get<double>(parameters.at("radius")));
        std::mt19937 rng(static_cast<std::uint32_t>(std::get<std::int64_t>(parameters.at("seed"))));
        std::uniform_real_distribution<float> offset(-1, 1);
        const int columns =
            static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count * canvas.x) / static_cast<float>(canvas.y))));
        const int rows = (count + columns - 1) / columns;
        const auto width = static_cast<float>(canvas.x), height = static_cast<float>(canvas.y);
        const auto gridColumns = static_cast<float>(columns), gridRows = static_cast<float>(rows);
        for (int i = 0; i < count; ++i) {
            _positions.push_back(
                cluster ? Float2{width * 0.5f + offset(rng) * width * 0.2f, height * 0.5f + offset(rng) * height * 0.2f}
                        : Float2{(static_cast<float>(i % columns) + 0.5f) * width / gridColumns,
                                 (static_cast<float>(i / columns) + 0.5f) * height / gridRows});
        }
    }

private:
    void Draw(Painter& paint) const override {
        for (const Float2 center : _positions)
            paint.Glow(center, _radius, {.color = {80, 210, 255, 100}});
    }
    std::vector<Float2> _positions;
    float _radius = 24;
};
} // namespace
PreviewDefinition GlowBenchmarkDefinition() {
    PreviewDefinition definition;
    definition.id = "engine.benchmark.glow-batching";
    definition.name = "Painter Glows";
    definition.category = "Profiler";
    definition.description =
        "Radial lights recorded and batched by Painter; A/B variants can override workload parameters.";
    definition.parameters = {{.id = "count",
                              .label = "Glows",
                              .kind = PreviewParameterKind::Choice,
                              .initial = std::string("100"),
                              .apply = PreviewApplyMode::OnReset,
                              .choices = {"20", "100", "500"}},
                             {.id = "overlap",
                              .label = "Cluster / overlap",
                              .kind = PreviewParameterKind::Boolean,
                              .initial = true,
                              .apply = PreviewApplyMode::OnReset},
                             {.id = "radius",
                              .label = "Radius",
                              .kind = PreviewParameterKind::Float,
                              .initial = 24.0,
                              .minimum = 4,
                              .maximum = 100,
                              .apply = PreviewApplyMode::OnReset},
                             {.id = "seed",
                              .label = "Seed",
                              .kind = PreviewParameterKind::Integer,
                              .initial = std::int64_t{1729},
                              .minimum = 0,
                              .maximum = 2147483647,
                              .apply = PreviewApplyMode::OnReset}};
    definition.benchmark = PreviewBenchmark{definition.id, 2};
    definition.create = [](const PreviewServices& services, const PreviewParameters& parameters) {
        return std::make_unique<GlowBenchmark>(services.canvas, parameters);
    };
    return definition;
}
