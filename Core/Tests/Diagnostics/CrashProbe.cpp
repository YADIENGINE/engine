#include <windows.h>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include "Diagnostics/Internals/CrashSession.h"
#include "Diagnostics/Internals/CrashPacket.h"

__declspec(noinline) void ThrowFromProbe() { throw std::runtime_error("deliberate probe exception"); }
__declspec(noinline) void ViolateMemory() {
    volatile int* invalid = nullptr;
    *invalid = 17;
}
int main(int argc, char** argv) {
    if (argc == 6 && std::string_view(argv[1]) == "--monitor") {
        // Test-only hung helper: acknowledges startup, then never acknowledges the crash request.
        const auto handle = [](const char* value) { return reinterpret_cast<HANDLE>(std::stoull(value)); };
        auto* packet =
            static_cast<CrashPacket*>(MapViewOfFile(handle(argv[2]), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashPacket)));
        if (!packet) return 2;
        InterlockedExchange(&packet->state, 1);
        SetEvent(handle(argv[4]));
        Sleep(INFINITE);
        return 0;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const CrashSession crash("probe");
    SetCrashContext("probe-context");
    return RunCrashBoundary([&] {
        const std::string_view mode = argc > 1 ? argv[1] : "normal";
        if (mode == "throw") ThrowFromProbe();
        if (mode == "unknown") throw 17;
        if (mode == "access") ViolateMemory();
        if (mode == "thread") {
            std::thread task(ViolateMemory);
            task.join();
        }
        if (mode == "terminate") std::terminate();
        if (mode == "abort") std::abort();
        if (mode == "exit") std::exit(0);
        if (mode == "cleanup") {
            RunWithCrashCleanup([] { ThrowFromProbe(); },
                                [] {
                                    try {
                                        throw std::runtime_error("secondary cleanup error");
                                    } catch (...) {
                                    }
                                });
        }
        if (mode == "handled") {
            try {
                ThrowFromProbe();
            } catch (const std::exception&) {
            }
        }
        return 0;
    });
}
