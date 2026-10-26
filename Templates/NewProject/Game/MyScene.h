#pragma once
#include "Runtime/Scene.h"

class MyScene final : public Scene {
public:
    MyScene();
    ~MyScene() override;

private:
    void Enter() override;
    void HandleInput(const DeviceInput& input) override;
    void Update(float seconds) override;
    void Draw() const override;

    float _x = 640.0f;
    int _direction = 0;
};
