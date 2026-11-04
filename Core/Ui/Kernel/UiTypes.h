#pragma once
#include <cstdint>
#include <functional>
#include "Ui/Kernel/UiId.h"
#include "Ui/Layout/UiLayout.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

class FontFace;
class Painter;

// None: only drawn. Pointer: reacts to the mouse. Focus: also takes keyboard focus.
enum class UiInteract { None, Pointer, Focus };

inline constexpr int UiAnyUser = -1;

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
    // A disabled element can still be focused, so the player sees why it does not work; activating it reports Denied.
    bool enabled = true;
    int owner = UiAnyUser; // Only this player's keys reach it. Inherited by children.
    bool clip = false;     // Children are cut at its edges, and cannot be clicked outside them
    UiVisual visual{};
    UiMeasure measure{}; // For a leaf with content of its own, such as a text
};

// A pressed button: true when it happened, with the player who did it.
struct UiAction {
    bool happened = false;
    int user = 0;
    [[nodiscard]] explicit operator bool() const { return happened; }
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

enum class UiEventKind { FocusMoved, Activated, Denied, Back, LayerOpened, LayerClosed };
// What happened this frame, for sounds and effects. Valid until the next Begin.
struct UiEvent {
    UiEventKind kind = UiEventKind::FocusMoved;
    UiId id{};
    int user = 0;
    FloatRect rect{};
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
