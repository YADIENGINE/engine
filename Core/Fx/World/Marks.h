#pragma once
#include <cstddef>
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

class Painter;

// A mark on the ground: a scorch, a splash, the dust of a landing, the shadow of something about to fall. It lies under
// what stands in the world and fades. The colour's alpha is its strength.
struct MarkStyle {
    float radius = 24.0f;      // Half its width, at intensity 1
    float seconds = 6.0f;      // How long it lies
    float fadeSeconds = 2.0f;  // The last part of 'seconds', over which it fades out
    float growSeconds = 0.0f;  // Grows from nothing over the first part: a shadow of what is falling
    float flatten = 1.0f;      // Its height against its width: below 1 it lies flat in a three-quarter view
};

[[nodiscard]] bool IsValid(const MarkStyle& style);

// Marks on the ground of a field, drawn in world coordinates under everything that stands (FxStage::DrawGroundLayer).
// They run on Update time, so a hitstop holds them.
class Marks {
public:
    static constexpr std::size_t MaxLying = 256; // A new one replaces the oldest

    // An invalid style throws invalid_argument.
    void Leave(Float2 at, Color4 color, const MarkStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint) const;
    [[nodiscard]] std::size_t Count() const { return _lying.size(); }

private:
    struct Lying {
        Float2 at;
        Color4 color;
        MarkStyle style;
        float age = 0.0f;
    };
    std::vector<Lying> _lying;
};
