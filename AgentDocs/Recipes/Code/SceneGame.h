// [recipe:SceneGame-header]
#pragma once
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include "Fx/TransitionClock.h"
#include "Runtime/Game.h"

class RecipeGame final : public Game {
public:
    explicit RecipeGame(const GameSettings& settings);

private:
    void Start() override;
    void HandleInput(const DeviceInput& input) override;
    void Update(float realSeconds) override;
    void Draw(Painter& paint) const override;
    RecipeScene _first;
    RecipeScene _second;
    TransitionClock _transition;
};
// [/recipe]
