#include <stdexcept>
#include "Editor/Worker/WorkerHost.h"

int main(int argc, char** argv) {
    return RunEditorWorker(argc, argv,
                           [](SceneRegistry&) { throw std::runtime_error("Crash fixture worker registration"); }, {});
}
