#pragma once
#include "Graphics/Render/RenderExecutionOptions.h"

struct FrameExecutionSettings {
    int fpsLimit = 60;
    bool vsync = false;
    RenderExecutionOptions drawing;
};
