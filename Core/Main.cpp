#include "Runtime/EntryPoint.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include "Runtime/Game.h"
#include "Diagnostics/Internals/CrashSession.h"

int main(int, char*[]) {
    const CrashSession crash("game",
#ifdef NDEBUG
                             true
#else
                             false
#endif
    );
    return RunCrashBoundary([] {
        auto game = CreateGame();
        if (!game) throw std::runtime_error("CreateGame returned an empty game");
        game->Run();
        return 0;
    });
}
