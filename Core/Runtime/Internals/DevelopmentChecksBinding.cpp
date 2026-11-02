// Compiled into each game and editor worker with the executable's own flags (Utilities/DevelopmentChecks.h).
#include "Utilities/DevelopmentChecks.h"

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
