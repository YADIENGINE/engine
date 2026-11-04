#pragma once

class Ui;

// Told when a Ui has finished building a frame, so that something above the kernel can act on what happened without the
// kernel knowing what that is. Game listens this way to play the UI's sounds; tests listen with no audio at all.
class UiListener {
public:
    virtual ~UiListener() = default;
    // At the end of Ui::End. Ui::Events and Ui::Looks hold this frame's.
    virtual void FrameBuilt(const Ui& ui) = 0;

protected:
    UiListener() = default;
    UiListener(const UiListener&) = default;
    UiListener& operator=(const UiListener&) = default;
};

// Who every Ui tells: read at each Ui::End, so nothing dangles when it changes. Null: nobody. Borrowed. Game sets it.
[[nodiscard]] UiListener* ActiveUiListener();
void SetActiveUiListener(UiListener* listener);
