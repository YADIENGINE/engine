#include "Runtime/Scene.h"

Scene::Scene(TimeDomain time)
    : _time(time) {
    _self.Add(*this);
}
void Scene::AddObject(GameObject& object) { _objects.Add(object); }
void Scene::Enter() {}
void Scene::Exit() {}
void Scene::HandleInput(const DeviceInput& input) { (void)input; }
void Scene::Activate() {
    if (_active || _shutdown) return;
    _self.Start();
    _objects.Start();
    _active = true;
    Enter();
}
void Scene::Deactivate() {
    if (!_active) return;
    _active = false;
    Exit();
}
void Scene::ReadInput(const DeviceInput& input) {
    if (_active && Enabled()) HandleInput(input);
}
void Scene::Tick(float deltaSeconds) {
    if (!_active || !Enabled()) return;
    _objects.Update(deltaSeconds);
    _self.Update(deltaSeconds);
}
void Scene::Render() const {
    if (!_active || !Enabled()) return;
    _objects.Draw();
    _self.Draw();
}
void Scene::Shutdown() {
    if (_shutdown) return;
    _shutdown = true;
    Deactivate();
    _objects.Stop();
    _self.Stop();
}
