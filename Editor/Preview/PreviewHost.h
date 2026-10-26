#pragma once
#include <functional>
#include <memory>
#include "Editor/Api/PreviewRegistry.h"
#include "Runtime/GameSettings.h"

class Game;
class ControlChannel;
using PreviewApplicationFactory = std::function<std::unique_ptr<Game>(const GameSettings&, const PreviewRegistry&,
                                                                      ControlChannel&, const std::string&)>;

// Project adapters supply registrations/settings and link only their development worker to this entry point.
int RunPreviewHost(int argc, char** argv, const std::function<void(PreviewRegistry&)>& registerPreviews,
                   GameSettings settings, PreviewApplicationFactory createApplication = {});
