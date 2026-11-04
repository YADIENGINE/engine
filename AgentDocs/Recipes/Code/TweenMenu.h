// [recipe:TweenMenu-header]
#pragma once
#include <cstddef>
#include <vector>
#include "Animation/Tween.h"
#include "Animation/TweenSet.h"
#include "Utilities/Geometry/Float2.h"

class Painter;

enum class RecipeMenuPart { Title, Glow };

// A view on ordinary time: a coin counts as a value with its own tweens, so it needs no pointer into a list that moves.
struct RecipeCoin {
    Float2 at;
    SecondsTween<float> scale{0.0f};
    SecondsTween<float> alpha{1.0f};
};

class RecipeTweenMenu {
public:
    void Open();
    void MoveCursor(Float2 to);
    void AddCoin(Float2 at);
    void CollectCoin(std::size_t index);
    void Update(float deltaSeconds);
    void Draw(Painter& paint) const;

    [[nodiscard]] float TitleScale() const { return _parts[RecipeMenuPart::Title]; }
    [[nodiscard]] int TitleBeats() const { return _titleBeats; }
    [[nodiscard]] std::size_t Coins() const { return _coins.size(); }

private:
    SecondsTweenSet<RecipeMenuPart> _parts{1.0f}; // Values that need no field of their own
    SecondsTween<Float2> _cursor;                 // A value with a field: reads like a Float2
    std::vector<RecipeCoin> _coins;
    int _titleBeats = 0;
};
// [/recipe]
