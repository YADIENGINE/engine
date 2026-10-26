#include "Graphics/Style/Theme.h"
#include "Graphics/Style/NeonArcadeTheme.h"

namespace {
Theme& Active() {
    static Theme theme = NeonArcadeTheme;
    return theme;
}
} // namespace

const Theme& CurrentTheme() { return Active(); }

void SetTheme(const Theme& theme) { Active() = theme; }
