#include "Editor/Internals/Session/RefreshState.h"
#include <stdexcept>

void RefreshState::Begin() {
    if (Busy()) throw std::logic_error("A refresh is already in progress");
    _phase = RefreshPhase::Building;
}
void RefreshState::BuildFinished(bool success) {
    if (_phase != RefreshPhase::Building) throw std::logic_error("No build is running");
    _phase = success ? RefreshPhase::Starting : RefreshPhase::Idle;
}
void RefreshState::Started() {
    if (_phase != RefreshPhase::Starting) throw std::logic_error("No new preview is starting");
    _phase = RefreshPhase::Idle;
}
