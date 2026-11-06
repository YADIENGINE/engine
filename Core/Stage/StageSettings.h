#pragma once
#include <optional>
#include "Camera/CameraSettings.h"
#include "Graphics/Atmosphere/Atmosphere.h"
#include "Graphics/Light/LightLayer.h"

// What a Stage is made of. Light and atmosphere are off unless given here: a scene without them pays nothing for them.
struct StageSettings {
    // How the stage's cameras follow. Their limits are also the world: where effects play and inside which a cinematic
    // shot keeps. Without limits the world is the canvas.
    CameraSettings camera{};
    // A light layer: the world darkens to the ambient plus the lights StageView::DescribeLight gives each frame, and
    // Stage::Occluders holds what stands still and stops light.
    std::optional<LightLayerOptions> light;
    // Weather on screen, behind and in front of what stands: Stage::Weather says how much, Stage::Atmosphere shows it.
    std::optional<AtmosphereLook> atmosphere;
    // The views are the ears: every frame the sound player gets one listener per view (SoundPlayer::SetListeners), so
    // a sound right of a view's middle is heard on the right. Off for a game that places its own listener, a character.
    bool listeners = true;
};
