#include "Graphics/Style/Theme.h"
#include "Graphics/Style/PlainTheme.h"

namespace {
Theme& Active() {
    static Theme theme = PlainTheme;
    return theme;
}
} // namespace

const Theme& CurrentTheme() { return Active(); }

void SetTheme(const Theme& theme) { Active() = theme; }
