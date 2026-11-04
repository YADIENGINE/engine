# UI

Game UI: menus, dialogs, panels and HUD, written in code and redrawn from the game's own data each frame. It runs without a window, so a screen can be tested by feeding it input. The design and its reasons are in [D31](Design/README.md#d31-game-ui). `ToolUi/` is a different thing: the ImGui layer of the editor and launcher, which never ships in a game.

## Writing a screen

A screen is a function that says what is on screen now. There is no widget object to create, keep or update.

```cpp
class OptionsScene : public Scene {
public:
    explicit OptionsScene(const FontFace& font)
        : _ui({.canvas = {0.0f, 0.0f, 1280.0f, 720.0f}, .font = &font}) {}

private:
    void HandleInput(const DeviceInput& input) override { _ui.Input(input); }
    void InputCancelled() override { _ui.CancelInput(); }
    void UpdateReal(float seconds) override {
        _ui.Begin(seconds);
        Build();
        _ui.End();
    }
    void Draw(Painter& paint) const override { _ui.Draw(paint); }

    void Build() {
        auto panel = UiPanel(_ui, "options", {.anchor = UiAnchor::Center});
        UiLabel(_ui, "Options", {.size = 40.0f});
        if (UiButton(_ui, "sound", _sound ? "Sound: on" : "Sound: off", {.width = UiFill()})) _sound = !_sound;
        if (UiButton(_ui, "back", "Back", {.width = UiFill()}) || _ui.Back()) GoBack();
    }

    Ui _ui;
    bool _sound = true;
};
```

Build in `UpdateReal`, not `Update`: when game time stops `Update` gets zero seconds, and a pause menu would stop moving with it.

**A frame.** `Input` only stores the input. `Begin` decides what it means, from the rectangles the last frame left: what is under the mouse, where the cursor moves, what was pressed. Each widget call adds an element and answers from that. `End` lays everything out. `Draw` paints what was built, and may run any number of times. So a click is judged against what the player saw, and a new element cannot be pressed in its first frame.

**Scopes.** An element is open until its variable ends, and what is added meanwhile are its children. Two panels side by side need their own braces; `Close()` ends one early.

**Ids.** An id names an element among its siblings, and its state and cursor follow it from frame to frame. Use a key from the data, not its translated text, so that a language change does not move the cursor. Two elements with one id under one parent throw. In a loop, pass the item's own key or index. A label needs no id.

## Layout

Each element has a `UiBox`: `width` and `height`, `direction`, `gap`, `padding`, `alignX`, `alignY`.

| Size | Meaning |
| --- | --- |
| `UiFit()` | As large as the content; the default. Across the parent's direction it stops at the room there is, which is how a long text comes to wrap or shrink |
| `UiFixed(64)` | 64 units |
| `UiFill(2)` | A share of what is left along the parent's direction, here with weight 2; across it, the whole width |

`min` and `max` bound any of them: `UiFit(120.0f)`, `UiFill(1.0f, 0.0f, 400.0f)`.

`direction` is `Column`, `Row` or `Overlay`. An element with an `anchor` leaves the flow and sits in a corner, an edge or the centre of its parent, moved by `offset`: this is how a HUD places things. `UiRow`, `UiColumn` and `UiSpacer` group without drawing.

Positions are not written anywhere, so nothing has to be kept apart by hand and a longer translation still fits. Coordinates are logical canvas units; `UiSettings::canvas` says how large the canvas is.

## Focus and keys

Each player has one cursor, moved by direction keys to whatever lies that way on screen. There is no list of what is next to what: a grid, a sidebar and a footer work with no code. A screen starts with a cursor on its first focusable element (`UiSettings::autoFocus`), going past the last continues from the first (`wrapFocus`), and a held direction repeats (`repeatDelay`, `repeatSeconds`). When the focused element goes away the cursor moves to the nearest one left.

Moving the mouse moves the cursor; a resting mouse does not, so the keyboard keeps working under it. A click is a press and a release on the same element. Default keys are arrows and WASD, Enter and Space, Escape. `_ui.Back()` is true when a back key was pressed and the layer being described is the top one. `UiTraits::enabled = false` lets the cursor land, so the player can see why it does not work, and reports `Denied()` instead.

Gamepad and touch are not read. Everything above sits on `UiKeys`, so a gamepad would be one more source of the same moves.

### Several players on one screen

```cpp
_ui.SetUsers({UiKeys::Wasd(Rgb(0x60c0ff)), UiKeys::Arrows(Rgb(0xff8060))});
{
    auto left = ui.Element("p1", {}, {.owner = 0});
    PlayerCards(ui, _first);
}
{
    auto right = ui.Element("p2", {}, {.owner = 1});
    PlayerCards(ui, _second);
}
```

Each player has a cursor of their own. An area with an `owner` takes only that player's keys, and with two or more players the mouse does not reach it. In an area without an owner both cursors can be on one element, and `UiAction::user` says who pressed. A look gets `focusUsers`, `cursorColor` and `secondCursorColor` to tell them apart.

## Layers

```cpp
if (auto pause = UiLayer(_ui, "pause", _paused, {.alignX = UiAlign::Center, .alignY = UiAlign::Center})) {
    auto panel = UiPanel(_ui, "panel");
    if (UiButton(_ui, "resume", "Resume") || _ui.Back()) _paused = false;
}
```

A layer is a sheet over the canvas with its own cursor. While `open` is false it stays true until its closing is done, taking no input, so it can animate out: `Open()` runs from 0 to 1. A modal layer, the default, stops input reaching what is below, and when it closes the cursor below returns to where it was; a new opening starts over. `modal = false` is for what floats over a working screen, such as a notice. `ui.Layer` is the bare version, and `UiLayer` adds a dimmed backdrop and a fade from the look.

An element that disappears at once takes its state along. An item that should animate out of a list stays in the game's data until it is done.

## Widgets of your own

A widget is a function over the same API the engine's widgets use, so a game's widget can do all they can.

```cpp
bool SpellSlot(Ui& ui, UiId id, Icon icon) {
    UiElement slot = ui.Element(id, {.width = UiFixed(64.0f), .height = UiFixed(64.0f)}, {.interact = UiInteract::Focus});
    float& glow = slot.Remember<float>(0.0f);
    glow += ((slot.Focused() ? 1.0f : 0.0f) - glow) * std::min(ui.Delta() * 12.0f, 1.0f);
    slot.Draw([icon, glow = glow](Painter& paint, const UiDrawState& state) {
        DrawIcon(paint, icon, state.rect.Center(), state.rect.width * 0.5f, {.color = Mix(Dim, Bright, glow)});
    });
    return static_cast<bool>(slot.Activated());
}
```

- `Remember<T>` keeps a value for as long as the element stays on screen, one per type.
- `Draw` runs after the function has returned: capture by value, and pass text through `ui.Text(...)`.
- `SetVisual` moves, scales or fades an element and its children without moving its siblings, and clicks follow it. This is how entrance animations are written, with `Age()` or a remembered tween.
- `UiTraits::measure` gives a leaf a size of its own, and `clip` cuts children at the edges.
- `ui.Events()` lists what happened this frame (`FocusMoved`, `Activated`, `Denied`, `Back`, `LayerOpened`, `LayerClosed`). Play sounds and effects from it; a cursor placed by the screen itself is not an event, so opening a menu makes no sound.

## Looks

Widgets say how they behave and looks say how they appear. Each widget has a look type with what it needs, and its default values draw it plainly from the theme's palette. A look set holds what differs from those.

```cpp
GameSettings settings;
settings.theme = NeonArcadeTheme;     // Colours, backgrounds and effects
settings.uiLooks = NeonLooks();       // How widgets look; empty is plain
```

To change one widget, for the whole game or one screen:

```cpp
UiLooks looks = NeonLooks();
looks.Set(UiButtonLook{.textSize = 30.0f, .draw = DrawMyButton});
settings.uiLooks = looks;             // Or _ui.Looks().Set(...) on one Ui
```

A single call takes `.look` and `.accent`. A game's own widget can have a look type too: a struct with default values, read with `ui.Looks().Get<MyLook>()`.

Change looks between frames. A built frame keeps pointing at the looks it was built with.

## Folders

| Folder | What it holds | Includes |
| --- | --- | --- |
| `Ui/Layout` | The box model; private solver | `Utilities` only |
| `Ui/Kernel` | `Ui`, `UiElement`, layers, focus, ids, keys, events, the look registry | `Layout`, `Input`, `Painter`; no widget and no look |
| `Ui/Looks` | One look type per widget, with its plain drawing; `Neon/` | `Kernel`, `Graphics`; no widget and no input |
| `Ui/Widgets` | Behaviour only: `UiLabel`, `UiButton`, `UiPanel`, `UiRow`, `UiColumn`, `UiSpacer`, `UiLayer` | `Looks`, `Kernel`, text measuring; no drawing |

`python Engine check` enforces this, and keeps simulations away from all of it. UI is view code: a choice that changes a simulation goes in as a command or input, so a replay holds it.

A new widget is a `Widgets/UiX` and a `Looks/UiXLook`; a new look is a folder under `Looks/`. Neither changes the kernel, and if one would have to, that is a design question for D31 first.
