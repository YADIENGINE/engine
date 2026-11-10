#include "Graphics/Surface/Surface.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Internals/TextureRefAccess.h"

namespace {
Float2 Validated(Float2 size, const SurfaceOptions& options) {
    if (!(size.x > 0.0f) || !(size.y > 0.0f) || !std::isfinite(size.x) || !std::isfinite(size.y))
        throw std::invalid_argument("Surface needs a positive size");
    if (!options.pixels) return size;
    return {std::max(1.0f, std::ceil(size.x - 1e-3f)), std::max(1.0f, std::ceil(size.y - 1e-3f))};
}
} // namespace

Surface::Surface(Float2 size, SurfaceOptions options)
    : _state(std::make_unique<State>()) {
    _state->size = Validated(size, options);
    _state->options = options;
}

Surface::~Surface() {
    if (_state) _state->Release();
}

Surface::Surface(Surface&&) noexcept = default;

Surface& Surface::operator=(Surface&& other) noexcept {
    if (this == &other) return *this;
    if (_state) _state->Release();
    _state = std::move(other._state);
    return *this;
}

void Surface::Resize(Float2 size) {
    size = Validated(size, _state->options);
    if (size == _state->size) return;
    _state->Release();
    _state->size = size;
}

Float2 Surface::Size() const { return _state->size; }

const SurfaceOptions& Surface::Options() const { return _state->options; }

TextureRef Surface::Texture() const {
    const State& state = *_state;
    if (!state.drawn) return {};
    return TextureRefAccess::Make(state.color, state.pixelWidth, state.pixelHeight, state.size.x, state.size.y, true);
}
