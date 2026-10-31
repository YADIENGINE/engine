#include "GameObjects/GameObject.h"
#include <cassert>

GameObject::GameObject() = default;
GameObject::~GameObject() {
    assert(_list == nullptr && "Registered GameObjects must outlive their ObjectList registration");
}
void GameObject::Start() {}
void GameObject::Update(float deltaSeconds) { (void)deltaSeconds; }
void GameObject::Draw(Painter& paint) const { (void)paint; }
void GameObject::Stop() {}
