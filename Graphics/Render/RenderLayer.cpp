#include "Graphics/Render/RenderLayer.h"
#include <rlgl.h>
#include "Graphics/Render/Internals/RenderTargetStack.h"

RenderLayer::RenderLayer(int width, int height, TextureFilter filter)
    : _target(LoadRenderTexture(width, height)) {
    SetTextureFilter(_target.texture, filter);
}

RenderLayer::~RenderLayer() { UnloadRenderTexture(_target); }

void RenderLayer::Begin() const { PushRenderTarget(_target, _target.texture.width, _target.texture.height); }

void RenderLayer::End() const { PopRenderTarget(); }

void RenderLayer::MakeOpaque() const {
    // Renk: kaynak 0, hedef 1 (değişmez); alfa: kaynak 1, hedef 0.
    rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ONE, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    DrawRectangle(0, 0, _target.texture.width, _target.texture.height, WHITE);
    EndBlendMode();
}
