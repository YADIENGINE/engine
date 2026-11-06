#include <memory>
#include <cstdlib>
#include <stdexcept>
#include "Runtime/Game.h"

std::unique_ptr<Game> CreateGame() {
    if (std::getenv("CRASH_FIXTURE_NATIVE")) {
        volatile int* invalid = nullptr;
        *invalid = 17;
    }
    throw std::runtime_error("Crash fixture game creation");
}
