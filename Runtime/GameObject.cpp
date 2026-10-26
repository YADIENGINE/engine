#include "Runtime/GameObject.h"

GameObject::GameObject() = default;
GameObject::~GameObject() = default;
void GameObject::Start() {}
void GameObject::Update(float deltaSeconds) { (void)deltaSeconds; }
void GameObject::Draw() const {}
void GameObject::Stop() {}
