#include "Graphics/Render/ShadowMask.h"
#include <rlgl.h>
#include <algorithm>
#include <cmath>

void DrawMergedShadow(float darkness, const std::function<void(Color)>& shapes) {
    const float keep = 1.0f - std::clamp(darkness, 0.0f, 1.0f);
    // Maske: parçalar rengi değiştirmeden alfaya 'keep' yazar (üst üste binen yerde de aynı değer).
    rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ONE, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    shapes({0, 0, 0, static_cast<unsigned char>(std::lround(keep * 255.0f))});
    // Karartma: renk alfayla çarpılır ve alfa 1 olur; aynı piksele ikinci kez çizmek artık bir şey değiştirmez.
    rlSetBlendFactorsSeparate(RL_ZERO, RL_DST_ALPHA, RL_ONE, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    shapes(WHITE);
    EndBlendMode();
}
