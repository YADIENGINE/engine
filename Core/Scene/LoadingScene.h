#pragma once
#include "Scene/Scene.h"

// The screen a game shows while its sounds load (Game::SetLoadingScene). It always runs on real time. The game's first
// scene starts after it, once the sounds are ready and Finished() says so: the first scene's Start and Enter can play.
// A scene that only draws closes as soon as the sounds are ready; one with a closing animation overrides Finished().
class LoadingScene : public Scene {
public:
    LoadingScene();

    // Host side: the sounds are ready.
    void MarkLoaded() { _loaded = true; }
    // Whether this screen has shown what it wants to. Asked only after Loaded(), so it cannot close before the sounds
    // are ready. Default: true.
    [[nodiscard]] virtual bool Finished() const;

protected:
    // True once the sounds are ready; from then on this scene may play them. Playing earlier throws. The editor's
    // preview has no loading to wait for, so there it stays false.
    [[nodiscard]] bool Loaded() const { return _loaded; }

private:
    bool _loaded = false;
};
