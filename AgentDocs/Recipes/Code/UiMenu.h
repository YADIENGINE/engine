// [recipe:UiMenu-header]
#pragma once
#include "Ui/Kernel/Ui.h"

struct DeviceInput;
class FontFace;
class Painter;

// A menu and a dialog over it. In a scene the three calls go in HandleInput, UpdateReal and Draw.
class RecipeMenu {
public:
    explicit RecipeMenu(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] int Plays() const { return _plays; }
    [[nodiscard]] int TilePresses() const { return _tilePresses; }
    [[nodiscard]] bool AskingToQuit() const { return _askingToQuit; }
    [[nodiscard]] bool Quit() const { return _quit; }

private:
    void Menu();
    void QuitDialog();

    Ui _ui;
    int _plays = 0;
    int _tilePresses = 0;
    bool _askingToQuit = false;
    bool _quit = false;
};
// [/recipe]
