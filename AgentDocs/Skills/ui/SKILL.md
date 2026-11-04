---
name: ui
description: Build or debug game UI - menus, options, pause screens, dialogs, panels, HUD, buttons, a widget of the game's own, a new look, a two-player menu. Use also for bugs such as "the button does not react", "input reaches the menu below", "the cursor is lost", "overlaps", "text does not fit". Not for the editor's own panels.
---

# Build or debug game UI

Reference: `Docs/Ui.md`. An example that compiles: `AgentDocs/Recipes/UiMenu.md`. Includes start at `Ui/`.

## Write it

1. A screen is a function called between `_ui.Begin(seconds)` and `_ui.End()`, in `UpdateReal`. Forward `HandleInput` to `_ui.Input`, `InputCancelled` to `_ui.CancelInput` and `Draw` to `_ui.Draw`.
2. Do not store positions. Say what goes where with `UiBox`: `UiFit`, `UiFixed`, `UiFill`, `direction`, `gap`, `alignX`, `anchor`.
3. Do not write focus code. Direction keys find the nearest element by itself, in any arrangement.
4. Read and write the game's own values directly in the function. There is nothing to bind or refresh.
5. A dialog or a pause screen is a `UiLayer`. Its input and cursor take care of themselves.
6. A widget that does not exist is a function over `ui.Element`, with `Remember`, `Draw` and `SetVisual`. Add a look type if a second game could want it looking different.
7. Sound and effects: read `ui.Events()` once a frame after building.

## Check it

UI runs without a window: test a screen by feeding `DeviceInput` to `Input` and looking at what it pressed. For how it looks, use the `visual-change` skill.

## When it misbehaves

| Symptom | Look at |
| --- | --- |
| A press does nothing | Is `_ui.Input` called? `interact` must be `Focus` or `Pointer`. A new element answers from its second frame on |
| It stopped reacting after a pause | Build in `UpdateReal`, not `Update` |
| Everything nests inside one panel | A scope ends with its variable: use `{ }` or `Close()` |
| "ended out of order" | A scope outlived a later one: keep them in local variables |
| "the same id" | Under one parent every id differs: use the item's key, or its index |
| The cursor jumps on a language change | The id comes from translated text: use a key |
| A crash or junk text while drawing | A draw lambda captured a reference or a temporary `string_view`: capture by value, use `ui.Text` |
| The mouse does nothing with two players | An area with an `owner` ignores the mouse by design |
| Text is cut or very small | It got less room than it wants: a `UiFit` label shrinks; `wrap` wraps it instead |
