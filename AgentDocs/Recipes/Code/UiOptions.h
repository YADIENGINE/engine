// [recipe:UiOptions-header]
#pragma once
#include "Ui/Kernel/Ui.h"
#include "Ui/Kit/UiOptionsScreen.h"

struct DeviceInput;
class FontFace;
class Painter;

// The ready options screen with a tab of the game's own, and a scrolling list with tabs written by hand.
class RecipeOptions {
public:
    explicit RecipeOptions(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] const UiOptionsValues& Values() const { return _values; }
    [[nodiscard]] bool ShowHints() const { return _showHints; }
    [[nodiscard]] int Difficulty() const { return _difficulty; }
    [[nodiscard]] bool Closed() const { return _closed; }

private:
    void GameRows(Ui& ui);

    Ui _ui;
    UiOptionsValues _values;
    bool _showHints = true;
    int _difficulty = 1;
    bool _closed = false;
};

// A list of levels in a scrolling grid under tabs, on the same widgets.
class RecipeLevels {
public:
    explicit RecipeLevels(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] int Chosen() const { return _chosen; }
    [[nodiscard]] int World() const { return _world; }

private:
    Ui _ui;
    int _world = 0;
    int _chosen = -1;
};
// [/recipe]
