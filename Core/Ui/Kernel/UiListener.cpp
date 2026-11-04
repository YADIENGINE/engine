#include "Ui/Kernel/UiListener.h"

namespace {
UiListener*& Active() {
    static UiListener* listener = nullptr;
    return listener;
}
} // namespace

UiListener* ActiveUiListener() { return Active(); }
void SetActiveUiListener(UiListener* listener) { Active() = listener; }
