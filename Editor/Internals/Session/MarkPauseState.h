#pragma once
#include <cstdint>

// Why the current paused interval began. Only the reply to our own request can claim it for Mark.
class MarkPauseState {
public:
    void Requested(std::uint64_t generation, std::uint64_t request);
    void Observe(std::uint64_t generation, std::uint64_t request, bool paused);
    void Clear();
    [[nodiscard]] bool Active() const { return _confirmed; }
    [[nodiscard]] bool Pending() const { return _request != 0 && !_confirmed; }

private:
    std::uint64_t _generation = 0;
    std::uint64_t _request = 0;
    bool _confirmed = false;
};
