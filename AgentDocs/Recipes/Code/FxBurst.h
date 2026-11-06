#pragma once
#include "Fx/FxStage.h"

// Usually on a Stage's effects: TriggerRecipeFx(_stage.Fx(), at). The stage updates and draws them.
void TriggerRecipeFx(FxStage& fx, Float2 at);
