#include "Graphics/Paint/Internals/DrawList.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
constexpr std::array<std::uint32_t, 6> QuadIndices{0, 1, 2, 0, 2, 3};

std::uint8_t Channel(float value) { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 255.0f))); }
} // namespace

void DrawList::Reset(const PaintTarget& root, std::optional<PremultipliedColor> clear) {
    _vertices.clear();
    _indices.clear();
    _commands.clear();
    _states.assign(1, PaintState{});
    _scopes.clear();
    _targets.assign(1, root);
    _executedCommands = 0;
    _executedVertices = 0;
    _executedIndices = 0;
    _revision = 0;
    _error.clear();
    _firstRejection.clear();
    _rejected = 0;
    _finished = false;
    _commands.emplace_back(BeginTargetCommand{root, clear});
}

void DrawList::Finish() {
    if (_finished) throw std::logic_error("DrawList: frame finished twice");
    if (!_scopes.empty() && _error.empty()) Fail("Paint scopes still open when the frame ended");
    while (!_scopes.empty())
        PopScope(Depth());
    _commands.emplace_back(EndTargetCommand{});
    _finished = true;
    if (_error.empty()) return;
    if (_strict) throw std::logic_error("Painter: " + _error);
    Reject(_error);
}

float DrawList::PixelScale() const {
    const Affine2& transform = State().transform;
    const Float2 target = CurrentTarget().PixelScale();
    const float scaleX = std::hypot(transform.a, transform.b) * target.x;
    const float scaleY = std::hypot(transform.c, transform.d) * target.y;
    return std::max(std::min(scaleX, scaleY), 1e-6f);
}

std::uint32_t DrawList::PushScope(ScopeKind kind) {
    _scopes.push_back(kind);
    _states.push_back(_states.back());
    return Depth();
}

void DrawList::PopScope(std::uint32_t depth) {
    if (depth == 0 || depth > Depth()) return; // Already closed while unwinding an earlier error
    if (depth != Depth()) Fail("A paint scope ended while a later one was still open");
    while (Depth() >= depth) {
        switch (_scopes.back()) {
        case ScopeKind::Target:
        case ScopeKind::Group:
            _targets.pop_back();
            _commands.emplace_back(EndTargetCommand{});
            break;
        case ScopeKind::Marker   : _commands.emplace_back(PopMarkerCommand{}); break;
        case ScopeKind::Transform:
        case ScopeKind::Clip     :
        case ScopeKind::Opacity  :
        case ScopeKind::Blend    : break;
        }
        _scopes.pop_back();
        _states.pop_back();
        ++_revision;
    }
}

void DrawList::Fail(std::string message) {
    if (_error.empty()) _error = std::move(message);
}

void DrawList::Reject(std::string_view message) {
    if (_strict) throw std::invalid_argument(std::string(message));
    if (_rejected == 0) _firstRejection = message;
    ++_rejected;
}

void DrawList::BeginTarget(const PaintTarget& target, std::optional<PremultipliedColor> clear, bool fresh) {
    _targets.push_back(target);
    PaintState& state = MutableState();
    if (fresh)
        state = PaintState{};
    else {
        state.opacity = 1.0f;
        state.blend = Blend::Normal;
    }
    state.target = static_cast<std::uint32_t>(_targets.size() - 1);
    _commands.emplace_back(BeginTargetCommand{target, clear});
    ++_revision;
}

void DrawList::PushMarker(std::string_view name) {
    _commands.emplace_back(PushMarkerCommand{name});
    ++_revision;
}

PaintColor DrawList::Shade(Color4 color, bool additive, float strength) const {
    const PaintState& state = State();
    const float alpha = static_cast<float>(color.a) / 255.0f * std::clamp(state.opacity, 0.0f, 1.0f);
    const float scale = alpha * std::max(strength, 0.0f);
    const bool add = additive || state.blend == Blend::Add;
    return {Channel(static_cast<float>(color.r) * scale), Channel(static_cast<float>(color.g) * scale),
            Channel(static_cast<float>(color.b) * scale), add ? std::uint8_t{0} : Channel(255.0f * alpha)};
}

std::optional<PixelRect> DrawList::Scissor() const {
    const std::optional<FloatRect>& clip = State().clip;
    if (!clip) return std::nullopt;
    const PaintTarget& target = CurrentTarget();
    const Float2 scale = target.PixelScale();
    const PixelRect& view = target.viewport;
    const int left = std::clamp(static_cast<int>(std::floor(clip->x * scale.x)), 0, view.width);
    const int top = std::clamp(static_cast<int>(std::floor(clip->y * scale.y)), 0, view.height);
    const int right = std::clamp(static_cast<int>(std::ceil(clip->Right() * scale.x)), left, view.width);
    const int bottom = std::clamp(static_cast<int>(std::ceil(clip->Bottom() * scale.y)), top, view.height);
    return PixelRect{view.x + left, view.y + top, right - left, bottom - top};
}

GpuBlend DrawList::PipelineBlend() const {
    switch (State().blend) {
    case Blend::Normal:
    case Blend::Add     : return GpuBlend::Premultiplied;
    case Blend::Multiply: return GpuBlend::Multiply;
    case Blend::Screen  : return GpuBlend::Screen;
    }
    return GpuBlend::Premultiplied;
}

DrawBatch& DrawList::BatchFor(std::span<const GpuTexture> textures,
                              std::array<std::uint8_t, PaintTextureSlots>& slots) {
    if (textures.size() > PaintTextureSlots) throw std::invalid_argument("DrawList: too many textures in a primitive");
    const GpuBlend blend = PipelineBlend();
    const std::optional<PixelRect> scissor = Scissor();
    const auto fits = [&](const DrawBatch& batch) {
        std::size_t missing = 0;
        for (const GpuTexture texture : textures) {
            const auto end = batch.textures.begin() + batch.textureCount;
            if (std::find(batch.textures.begin(), end, texture) == end) ++missing;
        }
        return batch.textureCount + missing <= PaintTextureSlots;
    };
    DrawBatch* batch = nullptr;
    if (_commands.size() > _executedCommands) {
        if (auto* last = std::get_if<DrawBatch>(&_commands.back());
            last && last->blend == blend && last->scissor == scissor && fits(*last)) {
            batch = last;
        }
    }
    if (!batch) {
        DrawBatch fresh;
        fresh.firstIndex = static_cast<std::uint32_t>(_indices.size());
        fresh.blend = blend;
        fresh.scissor = scissor;
        batch = &std::get<DrawBatch>(_commands.emplace_back(fresh));
    }
    for (std::size_t i = 0; i < textures.size(); ++i) {
        const auto end = batch->textures.begin() + batch->textureCount;
        auto found = std::find(batch->textures.begin(), end, textures[i]);
        if (found == end) {
            batch->textures[batch->textureCount] = textures[i];
            found = batch->textures.begin() + batch->textureCount;
            ++batch->textureCount;
        }
        slots[i] = static_cast<std::uint8_t>(found - batch->textures.begin());
    }
    return *batch;
}

void DrawList::Add(std::span<PaintVertex> vertices, std::span<const std::uint32_t> indices,
                   std::span<const GpuTexture> textures) {
    if (_finished) throw std::logic_error("DrawList: drawing outside a frame");
    if (vertices.empty() || indices.empty()) return;
    std::array<std::uint8_t, PaintTextureSlots> slots{};
    DrawBatch& batch = BatchFor(textures, slots);
    const auto base = static_cast<std::uint32_t>(_vertices.size());
    const auto mapped = [&](std::size_t index) { return slots[std::min(index, textures.size() - 1)]; };
    for (PaintVertex& vertex : vertices) {
        if (!textures.empty()) {
            vertex.slot = mapped(vertex.slot);
            if (vertex.kind == PaintKind::Composite)
                vertex.shape0 = static_cast<float>(mapped(static_cast<std::size_t>(vertex.shape0)));
        }
        _vertices.push_back(vertex);
    }
    for (const std::uint32_t index : indices)
        _indices.push_back(base + index);
    batch.indexCount += static_cast<std::uint32_t>(indices.size());
    ++_revision;
}

void DrawList::AddQuad(std::array<PaintVertex, 4>& corners, std::span<const GpuTexture> textures) {
    Add(corners, QuadIndices, textures);
}

void DrawList::MarkExecuted() {
    _executedCommands = _commands.size();
    _executedVertices = _vertices.size();
    _executedIndices = _indices.size();
}
