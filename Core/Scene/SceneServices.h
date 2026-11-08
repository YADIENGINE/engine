#pragma once
#include <functional>
#include <string>
#include "Utilities/Geometry/Vec2.h"

class AudioSystem;
class FontFace;
class SimulationHost;
class SoundPlayer;
class TimeScale;
struct FxQuality;

// What a scene uses from its host: Game::Services() in the game, the same set in the editor. A scene's constructor
// takes `const SceneServices&` plus its own values, so the game and an editor catalog entry construct it alike.
// References outlive the scene. Never retain these services after Stop/destruction.
// Presentation time is Painter::Time(), which the editor pauses and scales.
struct SceneServices {
    Vec2 canvas;
    AudioSystem& audio;
    SoundPlayer& sounds;      // The game's sound tables: the same player as Game::Sounds()
    TimeScale& time;          // Game time's scale (Game::Time()): hitstop and slow motion
    const FxQuality& effects; // The applied effect choice (Game::Effects()); follows option changes
    // The game's font (Painter::Font()); null when the game has none. For measuring text outside drawing (hit tests).
    const FontFace* font = nullptr;
    std::function<void(const std::string&)> log; // A line for the developer: stderr in the game, Output in the editor
    SimulationHost* simulation = nullptr;        // Use it through HostedSimulation; null outside the editor
    // Optional host cursor visibility, visible by default; may be empty (always in editor-hosted scenes), so check
    // before calling; the host refuses to hide while the window is unfocused, so call again when input returns
    // to keep it hidden; the host restores visibility at scene exit; never locks, confines or warps the pointer.
    std::function<void(bool visible)> setCursorVisible;
};
