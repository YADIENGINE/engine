#pragma once
#include <functional>
#include <memory>
#include "Editor/Api/SceneRegistry.h"
#include "Runtime/GameSettings.h"

class Game;
class ControlChannel;
using WorkerApplicationFactory = std::function<std::unique_ptr<Game>(const GameSettings&, const SceneRegistry&,
                                                                     ControlChannel&, const std::string&)>;

// Project adapters supply registrations/settings and link only their development worker to this entry point.
int RunEditorWorker(int argc, char** argv, const std::function<void(SceneRegistry&)>& registerScenes,
                    GameSettings settings, WorkerApplicationFactory createApplication = {});
