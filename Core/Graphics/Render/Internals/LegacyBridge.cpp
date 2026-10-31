#include "Graphics/Render/Internals/LegacyBridge.h"
#include <raylib.h>
#include <rlgl.h>
#include <array>
#include <cmath>
#include "Graphics/Render/Internals/RenderTargetStack.h"

void LegacyBridge::Bind(const LegacyBinding& binding) {
    rlDrawRenderBatchActive();
    if (_pushed) {
        PopRenderTarget();
        _pushed = false;
    }
    const auto width = static_cast<int>(std::lround(binding.size.x));
    const auto height = static_cast<int>(std::lround(binding.size.y));
    if (binding.target.framebuffer != 0) {
        RenderTexture2D target{};
        target.id = binding.target.framebuffer;
        target.texture = {binding.target.colorTexture, binding.target.width, binding.target.height, 1,
                          PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        PushRenderTarget(target, width, height);
        _pushed = true;
        rlActiveDrawBuffers(1); // raylib's shaders write colour only; leave a bloom attachment alone
    } else {
        rlDisableFramebuffer();
        rlViewport(binding.viewport.x, GetRenderHeight() - (binding.viewport.y + binding.viewport.height),
                   binding.viewport.width, binding.viewport.height);
        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlOrtho(0.0, width, height, 0.0, 0.0, 1.0);
    }
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    const Affine2& t = binding.transform;
    if (t != Affine2::Identity()) {
        const std::array<float, 16> matrix{t.a,  t.b,  0.0f, 0.0f, t.c,  t.d,  0.0f, 0.0f,
                                           0.0f, 0.0f, 1.0f, 0.0f, t.tx, t.ty, 0.0f, 1.0f};
        rlMultMatrixf(matrix.data());
    }
    if (binding.scissor)
        BeginScissorMode(binding.scissor->x, binding.scissor->y, binding.scissor->width, binding.scissor->height);
    else
        EndScissorMode();
    // The Painter changed blending behind rlgl's cache: switching away and back makes rlgl apply alpha blending.
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlDisableDepthTest();
}

void LegacyBridge::Flush() { rlDrawRenderBatchActive(); }

void LegacyBridge::Release() {
    rlDrawRenderBatchActive();
    EndScissorMode();
    if (_pushed) {
        PopRenderTarget();
        _pushed = false;
    }
}
