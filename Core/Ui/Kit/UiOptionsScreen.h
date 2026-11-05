#pragma once
#include <functional>
#include <string_view>
#include "Ui/Kernel/Ui.h"
#include "Ui/Kit/UiOptionsValues.h"

// Every word on the screen, in English until a game gives its own.
struct UiOptionsTexts {
    std::string_view title = "Options";
    std::string_view audio = "Audio";
    std::string_view display = "Display";
    std::string_view effects = "Effects";
    std::string_view back = "Back";
    std::string_view music = "Music";
    std::string_view soundEffects = "Sound effects";
    std::string_view fullscreen = "Fullscreen";
    std::string_view fpsLimit = "Frame rate limit";
    std::string_view vsync = "VSync";
    std::string_view showFps = "Show frame rate";
    std::string_view glow = "Glow";
    std::string_view particles = "Particles";
    std::string_view shake = "Screen shake";
    std::string_view motion = "Motion effects";
    std::string_view flashes = "Screen flashes";
    std::string_view background = "Background effects";
    std::string_view screenMotion = "Screen distortion";
    std::string_view full = "Full";
    std::string_view reduced = "Reduced";
    std::string_view off = "Off";
    std::string_view unlimited = "Unlimited";
};

struct UiOptionsConfig {
    UiOptionsTexts texts{};
    bool audio = true;
    bool display = true;
    bool effects = true;
    float width = 640.0f;
    float height = 500.0f;
    // One more tab with the game's own rows, described by this function while the tab is open: controls, language.
    std::string_view extraTab{};
    std::function<void(Ui&)> extraRows{};
};

struct UiOptionsResult {
    bool changed = false; // A value changed this frame
    bool closed = false;  // The back button or a back key: time to leave the screen
};

// A ready options screen in tabs, centred on the canvas: audio, display and effects. Call it while it should show.
//     UiOptionsValues values = game.UiOptions();
//     const UiOptionsResult result = UiOptionsScreen(ui, "options", values);
//     if (result.changed) game.ApplyUiOptions(values);
//     if (result.closed) GoBack();
// Copy it from Ui/Kit when a game needs it arranged differently; it uses nothing the game cannot.
[[nodiscard]] UiOptionsResult UiOptionsScreen(Ui& ui, UiId id, UiOptionsValues& values,
                                              const UiOptionsConfig& config = {});
