#include "Graphics/Surface/Surface.h"
#include <stdexcept>
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Internals/TextureRefAccess.h"

namespace {
void Validate(Float2 size) {
    if (!(size.x > 0.0f) || !(size.y > 0.0f)) throw std::invalid_argument("Surface needs a positive size");
}
} // namespace

Surface::Surface(Float2 size, SurfaceOptions options)
    : _state(std::make_unique<State>()) {
    Validate(size);
    _state->size = size;
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
    Validate(size);
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
