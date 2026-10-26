#pragma once

enum class RefreshPhase { Idle, Stopping, Building, Starting, Failed };

// The build gate is advanced only after the owned worker process has actually exited.
class RefreshState {
public:
    RefreshState();
    [[nodiscard]] RefreshPhase Phase() const { return _phase; }
    [[nodiscard]] bool Busy() const;
    void Begin(bool workerAlive);
    void WorkerExited();
    void BuildFinished(bool success);
    void Started();
    void Cancel();

private:
    RefreshPhase _phase = RefreshPhase::Idle;
};
