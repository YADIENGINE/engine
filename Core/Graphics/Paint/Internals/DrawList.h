#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Graphics/Gpu/Internals/GpuTypes.h"
#include "Graphics/Gpu/Internals/PaintVertex.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Affine2.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

// A render target as recorded drawing sees it.
struct PaintTarget {
    GpuTarget target{};    // Null: the window's back buffer
    Float2 size;           // Logical size (paint coordinates)
    PixelRect viewport;    // Device pixels the logical size maps to
    bool emission = false; // Has a bloom attachment
    [[nodiscard]] Float2 PixelScale() const {
        return {static_cast<float>(viewport.width) / size.x, static_cast<float>(viewport.height) / size.y};
    }
};

// Consecutive primitives that share target, blend, clip and fit in the texture slots: one draw call.
struct DrawBatch {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    GpuBlend blend = GpuBlend::Premultiplied;
    std::optional<PixelRect> scissor;
    std::array<GpuTexture, PaintTextureSlots> textures{};
    std::uint8_t textureCount = 0;
};
struct BeginTargetCommand {
    PaintTarget target;
    std::optional<PremultipliedColor> clear;
};
struct EndTargetCommand {};
struct PushMarkerCommand {
    std::string_view name;
};
struct PopMarkerCommand {};
using DrawCommand = std::variant<DrawBatch, BeginTargetCommand, EndTargetCommand, PushMarkerCommand, PopMarkerCommand>;

enum class ScopeKind { Transform, Clip, Opacity, Blend, Group, Target, Marker };

// Drawing state; scopes save and restore it whole.
struct PaintState {
    Affine2 transform;
    std::optional<FloatRect> clip; // Current target's logical coordinates
    float opacity = 1.0f;
    Blend blend = Blend::Normal;
    std::uint32_t target = 0; // Index into the open target stack
};

// One frame's recorded drawing: geometry, commands and the state stack. Commands keep call order; a primitive joins
// the previous draw batch when nothing in between changed. Executed prefixes (see FrameExecutor) are sealed:
// later primitives never join an executed batch.
class DrawList {
public:
    DrawList() = default;

    // Starts a frame drawing into 'root'.
    void Reset(const PaintTarget& root, std::optional<PremultipliedColor> clear);
    // Closes the root target. Scopes still open or ended out of order: logic_error when strict, else one rejection.
    void Finish();
    [[nodiscard]] bool Finished() const { return _finished; }

    // --- State ---
    [[nodiscard]] const PaintState& State() const { return _states.back(); }
    [[nodiscard]] PaintState& MutableState() { return _states.back(); }
    [[nodiscard]] const PaintTarget& CurrentTarget() const { return _targets[State().target]; }
    // Device pixels per unit of the current target under the current transform (smallest axis).
    [[nodiscard]] float PixelScale() const;
    // Opens a scope (the current state is copied); returns its depth for PopScope.
    [[nodiscard]] std::uint32_t PushScope(ScopeKind kind);
    [[nodiscard]] ScopeKind KindAt(std::uint32_t depth) const { return _scopes[depth - 1]; }
    [[nodiscard]] std::uint32_t Depth() const { return static_cast<std::uint32_t>(_scopes.size()); }
    // Closes the scope at 'depth' and restores the state it saved; target, group and marker scopes also end their
    // target / marker. A scope above it still open is an error: it is closed too and the frame fails at Finish.
    void PopScope(std::uint32_t depth);
    void Fail(std::string message);

    // --- Programmer errors in drawing calls ---
    // Strict (the default in Debug builds): Reject throws invalid_argument at the call and Finish throws for scope
    // errors. Otherwise (Release) the call draws nothing, the error is counted and the frame goes on.
    void SetStrict(bool strict) { _strict = strict; }
    [[nodiscard]] bool Strict() const { return _strict; }
    void Reject(std::string_view message);
    [[nodiscard]] std::uint32_t Rejected() const { return _rejected; }
    [[nodiscard]] const std::string& FirstRejection() const { return _firstRejection; }

    // --- Targets and markers ---
    // Inside a just-pushed Target or Group scope: opens 'target' as the current target. 'fresh' resets the state
    // (identity transform, no clip, opaque, Normal blend); otherwise transform and clip carry over.
    void BeginTarget(const PaintTarget& target, std::optional<PremultipliedColor> clear, bool fresh);
    // Inside a just-pushed Marker scope.
    void PushMarker(std::string_view name);

    // --- Geometry ---
    // Premultiplied colour with the current opacity; Add blending (or 'additive') zeroes the alpha.
    [[nodiscard]] PaintColor Shade(Color4 color, bool additive = false, float strength = 1.0f) const;
    // Appends a primitive. 'indices' are relative to 'vertices'. Vertices sampling a texture name it with
    // 'textures' (slot = position in that list after mapping) — their 'slot' field is rewritten to the batch slot.
    void Add(std::span<PaintVertex> vertices, std::span<const std::uint32_t> indices,
             std::span<const GpuTexture> textures = {});
    void AddQuad(std::array<PaintVertex, 4>& corners, std::span<const GpuTexture> textures = {});

    // --- Execution (FrameExecutor) ---
    [[nodiscard]] const std::vector<PaintVertex>& Vertices() const { return _vertices; }
    [[nodiscard]] const std::vector<std::uint32_t>& Indices() const { return _indices; }
    [[nodiscard]] const std::vector<DrawCommand>& Commands() const { return _commands; }
    [[nodiscard]] std::size_t ExecutedCommands() const { return _executedCommands; }
    [[nodiscard]] std::size_t ExecutedVertices() const { return _executedVertices; }
    [[nodiscard]] std::size_t ExecutedIndices() const { return _executedIndices; }
    [[nodiscard]] bool HasPending() const { return _executedCommands < _commands.size(); }
    void MarkExecuted();
    // Number of primitives / commands recorded since the frame began (changes whenever anything is recorded).
    [[nodiscard]] std::uint64_t Revision() const { return _revision; }
    [[nodiscard]] const std::string& Error() const { return _error; }

private:
    [[nodiscard]] std::optional<PixelRect> Scissor() const;
    [[nodiscard]] GpuBlend PipelineBlend() const;
    // The batch the next primitive goes to (opening one if needed); fills 'slots' with the slot of each texture.
    DrawBatch& BatchFor(std::span<const GpuTexture> textures, std::array<std::uint8_t, PaintTextureSlots>& slots);

    std::vector<PaintVertex> _vertices;
    std::vector<std::uint32_t> _indices;
    std::vector<DrawCommand> _commands;
    std::vector<PaintState> _states;
    std::vector<ScopeKind> _scopes;
    std::vector<PaintTarget> _targets; // Open targets, root first
    std::size_t _executedCommands = 0;
    std::size_t _executedVertices = 0;
    std::size_t _executedIndices = 0;
    std::uint64_t _revision = 0;
    std::string _error;
    std::string _firstRejection;
    std::uint32_t _rejected = 0;
#ifdef NDEBUG
    bool _strict = false;
#else
    bool _strict = true;
#endif
    bool _finished = true;
};
