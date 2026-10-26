#include "Runtime/EntryPoint.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include "Runtime/Game.h"

int main(int, char*[]) {
    try {
        auto game = CreateGame();
        if (!game) throw std::runtime_error("CreateGame returned an empty game");
        game->Run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
