#include "Fx/World/FlashLights.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include "Animation/Easing.h"
#include "Fx/Internals/FxChecks.h"

bool IsValid(const LightFlashStyle& style) {
    return FxNotNegative(style.radius) && FxPositive(style.seconds) && FxNotNegative(style.strength);
}

void FlashLights::SetQuality(const FxQuality& quality) {
    _quality = quality;
    if (!_quality.flashes) Clear(); // One showing goes too: the player has just asked for this
}
void FlashLights::Start(FlashLimit& limit, Float2 at, Color4 color, const LightFlashStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid light flash style");
    if (!_quality.flashes || !limit.Start(FlashKind::Light)) return;
    if (_playing.size() >= MaxPlaying) _playing.erase(_playing.begin());
    _playing.push_back({at, color, style});
    Describe();
}
void FlashLights::Update(float deltaSeconds) {
    if (!FxPositive(deltaSeconds) || _playing.empty()) return;
    for (Playing& flash : _playing)
        flash.elapsed += deltaSeconds;
    std::erase_if(_playing, [](const Playing& flash) { return flash.elapsed >= flash.style.seconds; });
    Describe();
}
void FlashLights::Clear() {
    _playing.clear();
    Describe();
}
void FlashLights::Describe() {
    _lights.clear();
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    for (const Playing& flash : _playing) {
        const float left = 1.0f - EaseOutQuad(std::min(flash.elapsed / flash.style.seconds, 1.0f));
        const float strength = flash.style.strength * left;
        if (flash.style.radius > 0.0f) {
            _lights.push_back(
                {.at = flash.at, .radius = flash.style.radius, .color = flash.color, .intensity = strength});
            continue;
        }
        const float share = strength * static_cast<float>(flash.color.a) / 255.0f;
        red += static_cast<float>(flash.color.r) * share;
        green += static_cast<float>(flash.color.g) * share;
        blue += static_cast<float>(flash.color.b) * share;
    }
    const auto channel = [](float value) { return static_cast<std::uint8_t>(std::min(value, 255.0f)); };
    _ambient = {channel(red), channel(green), channel(blue), 255};
}
