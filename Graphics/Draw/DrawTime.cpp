#include "Graphics/Draw/DrawTime.h"

namespace {
double drawSeconds = 0.0;
} // namespace

float DrawTime() { return static_cast<float>(drawSeconds); }
void SetDrawTime(double seconds) { drawSeconds = seconds; }
