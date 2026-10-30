#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class GpuTimer {
public:
    GpuTimer();
    ~GpuTimer();
    void Begin(std::uint64_t frame);
    void End();
    void Poll(const std::function<void(std::uint64_t, double)>& result);
    [[nodiscard]] bool Supported() const;
    [[nodiscard]] std::size_t Pending() const;
    [[nodiscard]] std::string Description() const;

private:
    struct State;
    std::unique_ptr<State> _state;
};
