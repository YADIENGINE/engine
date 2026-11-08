#include "Scene/LoadingScene.h"

LoadingScene::LoadingScene()
    : Scene(TimeDomain::Real) {}
bool LoadingScene::Finished() const { return true; }
