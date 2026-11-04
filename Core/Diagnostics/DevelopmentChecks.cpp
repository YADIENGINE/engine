#include "Diagnostics/DevelopmentChecks.h"

namespace {
#ifdef NDEBUG
constinit bool Enabled = false;
#else
constinit bool Enabled = true;
#endif
} // namespace

bool DevelopmentChecks() { return Enabled; }

void SetDevelopmentChecks(bool enabled) { Enabled = enabled; }
