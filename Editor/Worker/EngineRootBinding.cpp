// Compiled into each editor worker with the path of its project's engine copy; engine libraries carry no path.
#include "Editor/Internals/Session/GraphicAssetCatalog.h"

namespace {
[[maybe_unused]] const bool Bound = [] {
    SetGraphicAssetEngineRoot(ENGINE_WORKER_ENGINE_ROOT);
    return true;
}();
} // namespace
