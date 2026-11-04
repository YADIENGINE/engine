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

`direction` can also be `Grid`, with `columns`: equal columns from left to right, each row as high as its tallest child. `UiGrid(ui, 4, {.gap = 8.0f})` is the short form.

## Focus and keys

Each player has one cursor, moved by direction keys to whatever lies that way on screen. There is no list of what is next to what: a grid, a sidebar and a footer work with no code. A screen starts with a cursor on its first focusable element (`UiSettings::autoFocus`), going past the last continues from the first (`wrapFocus`), and a held direction repeats (`repeatDelay`, `repeatSeconds`). When the focused element goes away the cursor moves to the nearest one left.

Moving the mouse moves the cursor; a resting mouse does not, so the keyboard keeps working under it. A click is a press and a release on the same element. Default keys are arrows and WASD, Enter and Space, Escape. `_ui.Back()` is true when a back key was pressed and the layer being described is the top one. `UiTraits::enabled = false` lets the cursor land, so the player can see why it does not work, and reports `Denied()` instead.

An element that sets `UiTraits::capture` takes the direction keys along that axis while it has the cursor, and reads them with `Step()`: this is how a slider has left and right. They repeat when held. `_ui.PageStep()` gives the previous and next page keys (Q and E), for the top layer.

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

## Controls

Rows with a label on the left and a control on the right. Up and down leave them; left and right change them. All take `UiControlOptions`: `enabled`, `accent`, `textSize`, `width`, `height`, `tooltip`.

```cpp
if (UiToggle(ui, "vsync", "VSync", _vsync)) Apply();                  // Confirm or a click flips; left off, right on
if (UiSlider(ui, "music", "Music", _music, 0, 100, 5)) SetMusic(_music); // Left, right and dragging; int or float
const std::array<std::string_view, 3> names{"Easy", "Normal", "Hard"};
if (UiStepper(ui, "level", "Difficulty", _level, names)) Restart();   // "< Normal >"; wraps
if (const int step = UiStepper(ui, "size", "Size", SizeText())) Resize(step); // -1, 0 or 1, for anything else
```

All return true when the value changed, and take the game's own variable.

## Tabs

```cpp
const std::array<std::string_view, 2> names{"Weapons", "Armour"};
static_cast<void>(UiTabs(ui, "shop", names, _page));
if (_page == 0) WeaponList(ui);
```

The row is one stop for the cursor, where left and right switch; going down and back up keeps the choice. Q and E switch from anywhere. Each tab's content is the caller's: only the open one is described.

## Lists and grids

```cpp
UiScroll list(ui, "items", {.gap = 4.0f});
for (const Item& item : _items) {
    if (UiButton(ui, item.key, item.name, {.width = UiFill()})) Buy(item);
}
```

The area scrolls when its content is taller than it. It follows a keyboard cursor, including to a row out of sight, and takes the wheel and a drag on its bar. A list that scrolls under a resting cursor does not jump back to it. For a grid: `UiScroll grid(ui, "tiles", {.direction = UiDirection::Grid, .columns = 4, .gap = 8.0f});`. Rows out of sight cannot be clicked. `options.height` defaults to `UiFill()`, the room left in the parent, so its parent needs a height of its own.

Every item is described every frame. For very long lists, describe only those near the view: `Offset()` says where it is.

## Tooltips

`{.tooltip = "Saves to slot one"}` on a button or a control. It appears after the mouse or a cursor has rested for `delay` seconds, stays inside the canvas, and leaves focus and input alone. In a widget of your own, call `UiTooltip(ui, element, text)` while the element is open. `{.onFocus = false}` keeps it to the mouse, for when a keyboard cursor rests on it all the time.

## Ready screens

`Ui/Kit` holds whole screens, written only with what is above, so a game can copy one and change it.

```cpp
void Build() {
    UiOptionsValues values = _game.UiOptions();
    const UiOptionsResult result = UiOptionsScreen(_ui, "options", values);
    if (result.changed) _game.ApplyUiOptions(values);
    if (result.closed) GoBack();
}
```

`UiOptionsScreen` has tabs for audio, display and effects, over plain `UiOptionsValues`; `Game::UiOptions` and `Game::ApplyUiOptions` give and take them, and they persist with the game's other options. `UiOptionsConfig` turns tabs off, sets the size, gives `UiOptionsTexts` for another language, and adds a tab of the game's own rows (`extraTab`, `extraRows`). It adds no sound, since a game's sounds are its own; see `ui.Events()` below. Window size and render resolution are not on it yet: they need the monitor's list.

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
- `Held()` and `ui.Pointer()` are for dragging, `Step()` for keys it captures, `Size()` and `FocusInside()` for what a scrolling area needs, and `PointerInside()` with `ui.Wheel()` for the wheel.
- `ui.Events()` lists what happened this frame (`FocusMoved`, `Activated`, `Stepped`, `Denied`, `Back`, `LayerOpened`, `LayerClosed`). Play sounds and effects from it; a cursor placed by the screen itself is not an event, so opening a menu makes no sound, and layer events are for modal layers only.

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
| `Ui/Widgets` | Behaviour only: `UiLabel`, `UiButton`, `UiPanel`, `UiRow`, `UiColumn`, `UiGrid`, `UiSpacer`, `UiLayer`, `UiToggle`, `UiSlider`, `UiStepper`, `UiTabs`, `UiScroll`, `UiTooltip` | `Looks`, `Kernel`, text measuring; no drawing |
| `Ui/Kit` | Whole screens: `UiOptionsScreen` | `Widgets` and whatever a screen needs |

`python Engine check` enforces this, and keeps simulations away from all of it. UI is view code: a choice that changes a simulation goes in as a command or input, so a replay holds it.

A new widget is a `Widgets/UiX` and a `Looks/UiXLook`; a new look is a folder under `Looks/`. Neither changes the kernel, and if one would have to, that is a design question for D31 first.
