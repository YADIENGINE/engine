#pragma once
#include <filesystem>
#include "Modules/JsonIO/JsonIO.hpp"

class Game;

// Privileged observation is kept here, rather than in the observed classes.
class EngineGodAccessor {
public:
    [[nodiscard]] static JsonIO::Document Context(const Game& game);
    [[nodiscard]] static std::filesystem::path Directory(const Game& game);
};
