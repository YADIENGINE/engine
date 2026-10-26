#include "Editor/Internals/Session/RefreshState.h"
#include <stdexcept>

RefreshState::RefreshState() = default;
bool RefreshState::Busy() const {
    return _phase == RefreshPhase::Stopping || _phase == RefreshPhase::Building || _phase == RefreshPhase::Starting;
}
void RefreshState::Begin(bool workerAlive) {
    if (Busy()) throw std::logic_error("A refresh is already in progress");
    _phase = workerAlive ? RefreshPhase::Stopping : RefreshPhase::Building;
}
void RefreshState::WorkerExited() {
    if (_phase != RefreshPhase::Stopping) throw std::logic_error("No worker exit is pending");
    _phase = RefreshPhase::Building;
}
void RefreshState::BuildFinished(bool success) {
    if (_phase != RefreshPhase::Building) throw std::logic_error("Build started before worker exit");
    _phase = success ? RefreshPhase::Starting : RefreshPhase::Failed;
}
void RefreshState::Started() {
    if (_phase != RefreshPhase::Starting) throw std::logic_error("No preview restart is pending");
    _phase = RefreshPhase::Idle;
}
void RefreshState::Cancel() { _phase = RefreshPhase::Idle; }
