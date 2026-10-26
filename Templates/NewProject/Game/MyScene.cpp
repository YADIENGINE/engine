#include "MyScene.h"
#include <raylib.h>
#include <algorithm>
#include "Input/InputFrame.h"

namespace {
constexpr float StartX = 640.0f;
constexpr float CenterY = 360.0f;
constexpr float Speed = 240.0f;
constexpr float Radius = 24.0f;
constexpr float CanvasWidth = 1280.0f;
} // namespace

MyScene::MyScene()
    : Scene(TimeDomain::Game) {}
MyScene::~MyScene() { Shutdown(); }
void MyScene::Enter() { _x = StartX; }
void MyScene::HandleInput(const DeviceInput& input) { _direction = InputFrame(input).Horizontal(); }
void MyScene::Update(float seconds) {
    _x = std::clamp(_x + static_cast<float>(_direction) * Speed * seconds, Radius, CanvasWidth - Radius);
    _direction = 0;
}
void MyScene::Draw() const { DrawCircleV({_x, CenterY}, Radius, ORANGE); }
