Draw a character from primitives and drive its pose with a spring.

APIs: Core/Graphics/Paint/Painter.h, Core/Graphics/Paint/Path.h, Core/Graphics/Anim/SpringPoint.h.

Call Pose from Update with the chosen delta; call Draw from the production View. Keep decorative cycles on paint.Time().

<!-- recipe:AnimatedCharacter-header -->
```cpp
#pragma once
#include "Graphics/Anim/SpringPoint.h"

class Painter;

class RecipeCharacterView {
public:
    void Pose(Float2 target, float deltaSeconds);
    void Draw(Painter& paint) const;

private:
    SpringPoint _pose;
};
```
<!-- /recipe -->

<!-- recipe:AnimatedCharacter -->
```cpp
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Paint/Path.h"

namespace {
constexpr float PoseStiffness = 90.0f;
constexpr float PoseDamping = 16.0f;
constexpr float PoseMaxStep = 0.05f;
constexpr float SwayRadians = 0.08f;
constexpr Float2 BodyCenter{160.0f, 160.0f};
constexpr Float2 BodyRadii{32.0f, 40.0f};
constexpr Color4 BodyColor = Rgb(0x60c0ff);
constexpr Float2 EarBase{-24.0f, -20.0f};
constexpr Float2 EarControl{-44.0f, -80.0f};
constexpr Float2 EarTip{-8.0f, -34.0f};
constexpr Float2 EyeOffset{12.0f, -8.0f};
constexpr float EyeRadius = 4.0f;
constexpr Color4 EyeColor = Rgb(0x102030);
} // namespace

void RecipeCharacterView::Pose(Float2 target, float deltaSeconds) {
    _pose.Follow(target, deltaSeconds, PoseStiffness, PoseDamping, PoseMaxStep);
}
void RecipeCharacterView::Draw(Painter& paint) const {
    const float phase = static_cast<float>(paint.Time());
    const Float2 center = BodyCenter + _pose.position;
    auto pose = paint.Transform(Affine2::Rotate(SwayRadians * std::sin(phase), center));
    paint.Ellipse(center, BodyRadii, Fill(BodyColor));
    Path ear;
    ear.MoveTo(center + EarBase).QuadTo(center + EarControl, center + EarTip).Close();
    paint.FillPath(ear, Fill(BodyColor));
    paint.Circle(center + EyeOffset, EyeRadius, Fill(EyeColor));
}
```
<!-- /recipe -->

Pitfalls: Core/Graphics/Anim/SpringPoint.h is header-only; Follow clamps long frames. Do not retain Painter or a scope between calls.

Concept: `Docs/Graphics/Painter.md#shapes`.
