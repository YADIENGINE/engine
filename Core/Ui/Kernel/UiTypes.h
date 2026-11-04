#pragma once
#include <cstdint>
#include <functional>
#include "Ui/Kernel/UiId.h"
#include "Ui/Layout/UiLayout.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class FontFace;
class Painter;

// None: only drawn. Pointer: reacts to the mouse. Focus: also takes keyboard focus.
enum class UiInteract { None, Pointer, Focus };
// Direction keys along these axes go to a focused element instead of moving the cursor: a slider's left and right.
enum class UiCapture { None, Horizontal, Vertical, Both };

inline constexpr int UiAnyUser = -1;

// What pressing or changing an element should sound like, by meaning: the kernel only carries this, and Ui/Sound turns
// it into the game's sound. Auto: a press is Confirm and a change is Step. None: silent, as the game plays its own.
enum class UiSound { Auto, None, Move, Step, Confirm, Back, Deny, Open, Close };

// A change to how an element and everything inside it appear, around the element's centre. It does not move its
// siblings. Clicks follow it, so what looks like a button is where the button is.
struct UiVisual {
    Float2 offset{};
    float scale = 1.0f;
    float opacity = 1.0f;
};

// What an element does, apart from its place.
struct UiTraits {
    UiInteract interact = UiInteract::None;
    UiCapture capture = UiCapture::None; // Read with UiElement::Step
    // A disabled element can still be focused, so the player sees why it does not work; activating it reports Denied.
    bool enabled = true;
    // What pressing it plays. A press that does nothing by itself, such as on a slider, says None.
    UiSound sound = UiSound::Auto;
    int owner = UiAnyUser; // Only this player's keys reach it. Inherited by children.
    bool clip = false;     // Children are cut at its edges, and cannot be clicked outside them
    UiVisual visual{};
    UiMeasure measure{}; // For a leaf with content of its own, such as a text
};

// A pressed button: true when it happened, with the player who did it.
struct UiAction {
    bool happened = false;
    int user = 0;
    bool byPointer = false; // The mouse did it, so Ui::Pointer says where
    [[nodiscard]] explicit operator bool() const { return happened; }
};

// A direction key given to an element that captures it: -1, 0 or 1 on each axis. Repeats while held.
struct UiStep {
    int x = 0;
    int y = 0;
    int user = 0;
    [[nodiscard]] explicit operator bool() const { return x != 0 || y != 0; }
};

// What an element's drawing knows this frame.
struct UiDrawState {
    FloatRect rect{};
    const FontFace* font = nullptr;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool enabled = true;
    std::uint32_t focusUsers = 0; // One bit per player whose cursor is here
    // The first two of those players' colours (UiKeys::color); alpha 0 when there is none, or for the theme's accent.
    Color4 cursorColor{0, 0, 0, 0};
    Color4 secondCursorColor{0, 0, 0, 0};
    float age = 0.0f;             // Seconds since it first appeared
};
using UiDrawFunction = std::function<void(Painter&, const UiDrawState&)>;

// Layer events are for modal layers; a notice or a tooltip coming and going says nothing.
// Changed comes from the widget (UiElement::ReportChange), since only it knows whether a key changed anything. Back
// comes from a screen asking Ui::Back and getting true, so a back key nobody asked about is no event.
enum class UiEventKind { FocusMoved, Activated, Changed, Denied, Back, LayerOpened, LayerClosed };
// What happened this frame, for sounds and effects. Read it after building: valid until the next Begin.
struct UiEvent {
    UiEventKind kind = UiEventKind::FocusMoved;
    UiId id{};
    int user = 0;
    FloatRect rect{};
    UiSound sound = UiSound::Auto; // The element's choice, for Activated and Changed
};

struct UiSettings {
    FloatRect canvas{0.0f, 0.0f, 1280.0f, 720.0f};
    const FontFace* font = nullptr; // Borrowed
    bool autoFocus = true;          // Each screen starts with a cursor on its first focusable element
    bool wrapFocus = true;          // Moving past the last element continues from the first
    float repeatDelay = 0.35f;      // A held direction key repeats after this long...
    float repeatSeconds = 0.09f;    // ...every this long
};

// Counts for the last built frame.
struct UiStats {
    int elements = 0;
    int interactive = 0;
    int stateEntries = 0;
    int errors = 0; // Programmer errors skipped when development checks are off
};
