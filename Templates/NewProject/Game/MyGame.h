#pragma once
#include "MyScene.h"
#include "Runtime/Game.h"

class MyGame final : public Game {
public:
    MyGame();

private:
    void Start() override;
    void HandleInput(const DeviceInput& input) override;
    MyScene _scene;
};
