#include "Editor/Internals/Git/GitAttention.h"

void GitAttention::Update(int behind, double now) {
    if (behind > _behind) _pulseStart = now;
    if (behind == 0) _pulseStart = -1;
    _behind = behind;
}
