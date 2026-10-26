#pragma once
struct WindowSettings {
    int width = 1280;
    int height = 720;
    const char* title = "Game";
    int targetFps = 60;
    int minWidth = 640;
    int minHeight = 360;
    bool resizable = true;
    bool maximized = false;
    // Offscreen consumers can keep the context hidden for its entire lifetime.
    bool visible = true;
    bool alwaysRun = false;
};
