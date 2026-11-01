#pragma once
#include <functional>
#include <string>
#include "Utilities/Vec2.h"
#include "Editor/Api/SceneParameter.h"

class AudioSystem;
class FontFace;
class Profiler;

// References outlive the scene. Never retain these services after Stop/destruction.
// Presentation time is Painter::Time(), which the worker host pauses and scales.
struct SceneServices {
    Vec2 canvas;
    AudioSystem& audio;
    std::function<void(const std::string&)> log;
    // The game's font (Painter::Font()); null when the game has none. For measuring text outside drawing (hit tests).
    const FontFace* font = nullptr;
    std::function<std::string(const std::string&, const SceneParameters&)> applicationCommand;
    Profiler* profiler = nullptr;
};
