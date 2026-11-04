// Compiled into each game and editor worker with the executable's own flags (Diagnostics/DevelopmentChecks.h).
#include "Diagnostics/DevelopmentChecks.h"

namespace {
#ifdef NDEBUG
constexpr bool ExecutableChecks = false;
#else
constexpr bool ExecutableChecks = true;
#endif

[[maybe_unused]] const bool Bound = [] {
    SetDevelopmentChecks(ExecutableChecks);
    return true;
}();
} // namespace
