#include "Ui/Kernel/UiLooks.h"
#include <utility>

namespace {
UiLooks& Active() {
    static UiLooks looks;
    return looks;
}
} // namespace

const UiLooks& ActiveUiLooks() { return Active(); }
void SetActiveUiLooks(UiLooks looks) { Active() = std::move(looks); }
