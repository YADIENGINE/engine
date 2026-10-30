#pragma once
#include <memory>

class Game;
// Implemented by the project, linked with the engine's entry point.
[[nodiscard]] std::unique_ptr<Game> CreateGame();
