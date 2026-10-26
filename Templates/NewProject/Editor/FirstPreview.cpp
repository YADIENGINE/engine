#include "FirstPreview.h"
#include <raylib.h>
#include <cmath>
#include <memory>
#include <variant>
#include "Graphics/Draw/DrawTime.h"

FirstPreview::FirstPreview(const PreviewParameters& parameters)
    : _radius(static_cast<float>(std::get<double>(parameters.at("radius")))) {}

std::string FirstPreview::ApplyParameter(const std::string& id, const PreviewValue& value) {
    if (id != "radius") return "Unknown parameter";
    _radius = static_cast<float>(std::get<double>(value));
    return {};
}

void FirstPreview::Draw() const {
    const float phase = static_cast<float>(DrawTime());
    DrawCircleV({640.0f, 360.0f}, _radius, Fade(ORANGE, 0.6f + 0.4f * std::sin(phase)));
}

PreviewDefinition FirstPreviewDefinition() {
    PreviewDefinition definition;
    definition.id = "myproject.first";
    definition.name = "First preview";
    definition.category = "Drawing";
    definition.parameters = {{"radius", "Radius", PreviewParameterKind::Float, 80.0, 8.0, 200.0}};
    definition.create = [](const PreviewServices&, const PreviewParameters& parameters) {
        return std::make_unique<FirstPreview>(parameters);
    };
    return definition;
}
