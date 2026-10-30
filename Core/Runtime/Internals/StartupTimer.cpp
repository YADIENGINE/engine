#include "Runtime/Internals/StartupTimer.h"
#include <cstdio>
#include <utility>

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point Origin() {
    static const Clock::time_point origin = Clock::now();
    return origin;
}
double MillisecondsSince(Clock::time_point moment) {
    return std::chrono::duration<double, std::milli>(Clock::now() - moment).count();
}
} // namespace

StartupTimer::StartupTimer(std::string stage)
    : _stage(std::move(stage)) {
    Origin();
    _start = Clock::now();
}
StartupTimer::~StartupTimer() {
    std::printf("STARTUP: %-48s %8.1f ms   (total %8.1f ms)\n", _stage.c_str(), MillisecondsSince(_start),
                MillisecondsSince(Origin()));
}
void StartupTimer::Mark(const char* moment) {
    std::printf("STARTUP: %-48s %8s      (total %8.1f ms)\n", moment, "", MillisecondsSince(Origin()));
    std::fflush(stdout);
}
