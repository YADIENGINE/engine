#pragma once

// Build & Refresh. The running scene keeps working while the worker target builds; a successful build is started
// beside it (Starting) and replaces it only once ready. Idle after the replacement, a failure or a cancel.
enum class RefreshPhase { Idle, Building, Starting };

class RefreshState {
public:
    [[nodiscard]] RefreshPhase Phase() const { return _phase; }
    [[nodiscard]] bool Busy() const { return _phase != RefreshPhase::Idle; }
    [[nodiscard]] bool Building() const { return _phase == RefreshPhase::Building; }
    void Begin();
    void BuildFinished(bool success);
    void Started();
    void Cancel() { _phase = RefreshPhase::Idle; }

private:
    RefreshPhase _phase = RefreshPhase::Idle;
};
