// Generated from ProjectSettings.schema.json by the engine's settings tools (Tools/tuning/settings.py).
// Do not edit: change the schema and run `python . check --fix`.
#pragma once
#include <array>
#include <string>
#include <string_view>
#include "Utilities/Color4.h"

// The project's settings (Docs/ProjectSettings.md): `Game/Game.settings.json` holds the values that differ
// from these defaults, and the editor edits them under Project > Project settings. A game reads them as
// GameSettings::project; engine hosts (the editor's worker, tools, tests) may change them in code.
struct ProjectSettings {
    enum class LoadingScreen : int { Default, Horror, Rpg, SciFi, Arcade, Cozy };
    static constexpr std::array<std::string_view, 6> LoadingScreenNames{"Default", "Horror", "Rpg", "SciFi", "Arcade", "Cozy"};

    // Identity
    // Window title; also the product name in the exe's details
    std::string title = "Game";
    // Folder for options and saves in the player's data folder; changing it after a release loses them
    std::string applicationId = "Game";
    // Company name in the exe's details; empty: none
    std::string company = "";
    // Game version in the exe's details: one to four numbers
    std::string version = "1.0.0";
    // Windows .ico from the project folder for the window, taskbar and exe; empty: none
    std::string icon = "";

    // Window
    // Window size when the game starts (px)
    int windowWidth = 1280;
    int windowHeight = 720;
    // Smallest size the player can drag the window to (px)
    int windowMinWidth = 640;
    int windowMinHeight = 360;
    bool resizable = true;
    // Start maximized
    bool maximized = false;
    // Frame rate limit until the player chooses one; 0: unlimited
    int targetFps = 60;

    // Canvas
    // The size the game draws at, scaled to the window (px)
    int canvasWidth = 1280;
    int canvasHeight = 720;
    // Canvas colour before anything is drawn
    Color4 background = Color4{0, 0, 0, 255};
    // Colour around the canvas when the window's shape differs
    Color4 letterbox = Color4{0, 0, 0, 255};
    // Smooth scaling; off keeps pixels sharp
    bool smoothCanvas = true;

    // Text
    // Font Painter draws with when given none, from Game/Assets; empty: none
    std::string font = "";

    // Startup
    // Which of the engine's loading screens opens the game; a scene given with Game::SetLoadingScene replaces it
    LoadingScreen loadingScreen = ProjectSettings::LoadingScreen::Default;

    friend bool operator==(const ProjectSettings&, const ProjectSettings&) = default;

    static constexpr std::string_view Schema = R"schema({
  "title": { "type": "text", "value": "Game", "group": "Identity", "note": "Window title; also the product name in the exe's details" },
  "applicationId": { "type": "path", "value": "Game", "group": "Identity", "note": "Folder for options and saves in the player's data folder; changing it after a release loses them" },
  "company": { "type": "text", "value": "", "group": "Identity", "note": "Company name in the exe's details; empty: none" },
  "version": { "type": "text", "format": "version", "value": "1.0.0", "group": "Identity", "note": "Game version in the exe's details: one to four numbers" },
  "icon": { "type": "path", "value": "", "group": "Identity", "note": "Windows .ico from the project folder for the window, taskbar and exe; empty: none" },
  "windowWidth": { "type": "int", "value": 1280, "min": 1, "max": 16384, "unit": "px", "group": "Window", "note": "Window size when the game starts" },
  "windowHeight": { "type": "int", "value": 720, "min": 1, "max": 16384, "unit": "px", "group": "Window" },
  "windowMinWidth": { "type": "int", "value": 640, "min": 1, "max": 16384, "unit": "px", "group": "Window", "note": "Smallest size the player can drag the window to" },
  "windowMinHeight": { "type": "int", "value": 360, "min": 1, "max": 16384, "unit": "px", "group": "Window" },
  "resizable": { "value": true, "group": "Window" },
  "maximized": { "value": false, "group": "Window", "note": "Start maximized" },
  "targetFps": { "type": "int", "value": 60, "min": 0, "max": 1000, "group": "Window", "note": "Frame rate limit until the player chooses one; 0: unlimited" },
  "canvasWidth": { "type": "int", "value": 1280, "min": 1, "max": 8192, "unit": "px", "group": "Canvas", "note": "The size the game draws at, scaled to the window" },
  "canvasHeight": { "type": "int", "value": 720, "min": 1, "max": 8192, "unit": "px", "group": "Canvas" },
  "background": { "type": "color", "value": "#000000", "group": "Canvas", "note": "Canvas colour before anything is drawn" },
  "letterbox": { "type": "color", "value": "#000000", "group": "Canvas", "note": "Colour around the canvas when the window's shape differs" },
  "smoothCanvas": { "value": true, "group": "Canvas", "note": "Smooth scaling; off keeps pixels sharp" },
  "font": { "type": "path", "value": "", "group": "Text", "note": "Font Painter draws with when given none, from Game/Assets; empty: none" },
  "loadingScreen": { "type": "choice", "choices": ["Default", "Horror", "Rpg", "SciFi", "Arcade", "Cozy"], "value": "Default", "group": "Startup", "note": "Which of the engine's loading screens opens the game; a scene given with Game::SetLoadingScene replaces it" }
})schema";

    template <class Visitor>
    void VisitSettings(Visitor& visitor) {
        visitor.Field("title", title);
        visitor.Field("applicationId", applicationId);
        visitor.Field("company", company);
        visitor.Field("version", version);
        visitor.Field("icon", icon);
        visitor.Field("windowWidth", windowWidth);
        visitor.Field("windowHeight", windowHeight);
        visitor.Field("windowMinWidth", windowMinWidth);
        visitor.Field("windowMinHeight", windowMinHeight);
        visitor.Field("resizable", resizable);
        visitor.Field("maximized", maximized);
        visitor.Field("targetFps", targetFps);
        visitor.Field("canvasWidth", canvasWidth);
        visitor.Field("canvasHeight", canvasHeight);
        visitor.Field("background", background);
        visitor.Field("letterbox", letterbox);
        visitor.Field("smoothCanvas", smoothCanvas);
        visitor.Field("font", font);
        visitor.Choice("loadingScreen", loadingScreen, LoadingScreenNames);
    }
};
