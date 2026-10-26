#include <memory>
#include "MyGame.h"
#include "Runtime/EntryPoint.h"

std::unique_ptr<Game> CreateGame() { return std::make_unique<MyGame>(); }
