#pragma once
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include "Ui/Kernel/UiElement.h"
#include "Ui/Kernel/UiKeys.h"
#include "Ui/Kernel/UiLayerScope.h"
#include "Ui/Kernel/UiLooks.h"
#include "Ui/Kernel/UiTypes.h"

class UiKernel;
struct DeviceInput;

// A screen described in code, every frame. Calls between Begin and End say what is on screen; the answers (was it
// pressed?) come from comparing this frame's input with what the last frame showed, which is what the player saw.
// Nothing holds a pointer to the game's data, so there is nothing to keep in step with it.
//
//     void HandleInput(const DeviceInput& input) override { _ui.Input(input); }
//     void InputCancelled() override { _ui.CancelInput(); }
//     void UpdateReal(float seconds) override {
//         _ui.Begin(seconds);
//         auto panel = UiPanel(_ui, "menu", {.anchor = UiAnchor::Center});
//         if (UiButton(_ui, "play", "Play")) Play();
//         panel.Close();
//         _ui.End();
//     }
//     void Draw(Painter& paint) const override { _ui.Draw(paint); }
//
// Build in UpdateReal so that a pause menu moves while game time is stopped. A widget is a function over this class's
// public API; a game's own widgets have the same powers as the engine's.
class Ui {
public:
    explicit Ui(const UiSettings& settings = {});
    ~Ui();

    void SetCanvas(const FloatRect& canvas);
    void SetFont(const FontFace* font);
    // One entry per player at this screen, each with a cursor of their own. Default: one player on the whole keyboard.
    // With two or more players the mouse does not reach elements owned by a player.
    void SetUsers(std::vector<UiKeys> users);
    [[nodiscard]] int UserCount() const;
    [[nodiscard]] UiLooks& Looks();
    [[nodiscard]] const UiLooks& Looks() const;

    // This frame's input; call it from Scene::HandleInput.
    void Input(const DeviceInput& input);
    // Ends a press in progress; call it from Scene::InputCancelled.
    void CancelInput();

    void Begin(float seconds);
    void End();
    // Draws the frame built last. Safe to call any number of times between builds.
    void Draw(Painter& paint) const;

    // An element, open until the returned scope ends: elements added meanwhile are its children.
    [[nodiscard]] UiElement Element(UiId id, const UiBox& box = {}, const UiTraits& traits = {});
    // An element nobody needs to find again: a text, a row. Its identity is its place among its siblings.
    [[nodiscard]] UiElement Element(const UiBox& box = {}, const UiTraits& traits = {});
    // A sheet over the canvas with its own focus. While 'open' is false it keeps being true until its closing is done,
    // without taking input, so its content can animate out with Open(). A modal layer stops input reaching those below.
    [[nodiscard]] UiLayerScope Layer(UiId id, bool open, const UiLayerSpec& spec = {});

    // A back key was pressed, and the layer being described is the one on top.
    [[nodiscard]] UiAction Back() const;
    // A copy that stays valid until the next Begin, for capturing text in a drawing.
    [[nodiscard]] std::string_view Text(std::string_view text);
    // The font; logic_error when there is none.
    [[nodiscard]] const FontFace& Font() const;
    [[nodiscard]] float Delta() const;
    [[nodiscard]] std::span<const UiEvent> Events() const;
    [[nodiscard]] const UiStats& Stats() const;

private:
    std::unique_ptr<UiKernel> _kernel;

public:
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
};
