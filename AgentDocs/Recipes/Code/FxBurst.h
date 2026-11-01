#pragma once
#include "Fx/FxStage.h"

[[nodiscard]] FxStage MakeRecipeFx(const FloatRect& area);
void TriggerRecipeFx(FxStage& fx, Float2 at);
void DrawRecipeFx(Painter& paint, const FxStage& fx, const FloatRect& viewArea);
