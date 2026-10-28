#pragma once

enum class GlowBatching { Batched, Individual };

// Diagnostic execution policy, deliberately separate from persistent player graphics preferences.
struct RenderExecutionOptions {
    GlowBatching glowBatching = GlowBatching::Batched;
    bool operator==(const RenderExecutionOptions&) const = default;
};
