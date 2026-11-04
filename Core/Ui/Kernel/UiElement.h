#pragma once
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>
#include "Ui/Kernel/UiTypes.h"

class UiKernel;

// An open element. Elements added while it lives are its children, so keep it in a local variable; like PaintScope,
// scopes end in the reverse order they began. Close() ends it early. Questions about the player are answered from what
// the last frame showed, and all answers are false in a layer that is closing.
class [[nodiscard]] UiElement {
public:
    UiElement(UiKernel& kernel, int node)
        : _kernel(&kernel)
        , _node(node) {}
    UiElement(UiElement&& other) noexcept
        : _kernel(std::exchange(other._kernel, nullptr))
        , _node(other._node) {}
    ~UiElement();
    // Ends the scope here; the element stays on screen.
    void Close();

    [[nodiscard]] UiId Id() const;
    [[nodiscard]] bool Hovered() const;
    // Held by the mouse or by a confirm key.
    [[nodiscard]] bool Pressed() const;
    // A mouse press that began on it still lasts, wherever the mouse is now: dragging.
    [[nodiscard]] bool Held() const;
    // The mouse is over it, in a layer that takes input, whatever else lies between.
    [[nodiscard]] bool PointerInside() const;
    // A cursor is on it: 'user''s, or by default anyone's.
    [[nodiscard]] bool Focused(int user = UiAnyUser) const;
    [[nodiscard]] UiAction Activated() const;
    // A direction key it captures (UiTraits::capture).
    [[nodiscard]] UiStep Step() const;
    // Activating it was refused, because it is disabled.
    [[nodiscard]] bool Denied() const;
    void TakeFocus(int user = 0);
    // Where it was last frame, in canvas coordinates after clips and UiVisual. Empty on its first frame.
    [[nodiscard]] FloatRect LastRect() const;
    // Its laid-out size last frame, before clips and UiVisual.
    [[nodiscard]] Float2 Size() const;
    // Where a cursor on it or on something inside it was last frame, even if clipped out of sight.
    [[nodiscard]] std::optional<FloatRect> FocusInside() const;
    // Seconds since it first appeared.
    [[nodiscard]] float Age() const;

    // Called while drawing, under this element's children. Runs after this frame's Build has returned, so capture by
    // value, with Ui::Text for text.
    void Draw(UiDrawFunction draw);
    void SetVisual(const UiVisual& visual);

    // A value that lives as long as the element stays on screen, one per type: animation, scroll position.
    template <class State, class... Args> [[nodiscard]] State& Remember(Args&&... args) {
        const std::type_index type(typeid(State));
        if (void* found = FindState(type)) return *static_cast<State*>(found);
        return *static_cast<State*>(AddState(type, std::make_shared<State>(std::forward<Args>(args)...)));
    }

private:
    [[nodiscard]] void* FindState(std::type_index type) const;
    void* AddState(std::type_index type, std::shared_ptr<void> state);

    UiKernel* _kernel; // Null once closed or moved from
    int _node;

public:
    UiElement(const UiElement&) = delete;
    UiElement& operator=(const UiElement&) = delete;
    UiElement& operator=(UiElement&&) = delete;
};
