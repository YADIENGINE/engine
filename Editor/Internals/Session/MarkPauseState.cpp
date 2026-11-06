#include "Editor/Internals/Session/MarkPauseState.h"

void MarkPauseState::Requested(std::uint64_t generation, std::uint64_t request) {
    _generation = generation;
    _request = request;
    _confirmed = false;
}
void MarkPauseState::Observe(std::uint64_t generation, std::uint64_t request, bool paused) {
    if (generation != _generation || request < _request) return;
    if (!paused)
        Clear();
    else if (_request && request == _request)
        _confirmed = true;
}
void MarkPauseState::Clear() {
    _generation = 0;
    _request = 0;
    _confirmed = false;
}
