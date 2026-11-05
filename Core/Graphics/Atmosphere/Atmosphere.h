#pragma once
#include <cstdint>
#include "Camera/CameraView.h"
#include "Graphics/Atmosphere/DriftField.h"
#include "Graphics/Atmosphere/FogField.h"
#include "Graphics/Atmosphere/GroundRipples.h"
#include "Weather/Weather.h"

class Painter;

// What each kind of weather looks like, behind what stands in the world and in front of it. Plain data: start from
// TopDownAtmosphere or SideViewAtmosphere and change what the game wants. A density of 0 or a clear colour leaves a
// part out.
struct AtmosphereLook {
    RippleLook ripples{};
    FogLook fogBehind{};
    DriftLook dustBehind{};
    DriftLook snowBehind{};
    DriftLook rainBehind{};
    DriftLook dustFront{};
    DriftLook snowFront{};
    DriftLook rainFront{};
    FogLook fogFront{};
};

// A world seen from above: fog anywhere, short rain over ripples on the floor.
[[nodiscard]] AtmosphereLook TopDownAtmosphere();
// A world seen from the side: long rain, and fog lying along the ground at height 'groundY'.
[[nodiscard]] AtmosphereLook SideViewAtmosphere(float groundY);

// The weather on screen: rain, snow, dust and fog at the amounts a Weather asks for, moving with its wind. The parts
// are usable on their own (DriftField, FogField, GroundRipples), which is how a game adds a kind of its own. Order is
// the tool, as with a LightLayer:
//
//     { auto world = paint.Camera(view); DrawFloor(paint); }
//     air.DrawBehind(paint, view);
//     { auto world = paint.Camera(view); DrawCharacters(paint); }
//     air.DrawFront(paint, view);
//     lights.Draw(paint, frame, view); // What is above this is lit like the world: dust shows only where light falls
//     DrawHud(paint);
//
// It only shows the weather. Rules read the Weather the game keeps, in its simulation if they change what happens.
class Atmosphere {
public:
    explicit Atmosphere(const AtmosphereLook& look = TopDownAtmosphere(), std::uint64_t seed = 1);

    // Each frame, on the clock that should move it: the game's stops it in a hitstop, a real one does not.
    void Update(float seconds, const Weather& weather);
    // Outside paint.Camera and paint.Transform; a split screen calls these for each view.
    void DrawBehind(Painter& paint, const CameraView& view = {}) const;
    void DrawFront(Painter& paint, const CameraView& view = {}) const;

private:
    Weather _weather;
    GroundRipples _ripples;
    FogField _fogBehind;
    DriftField _dustBehind;
    DriftField _snowBehind;
    DriftField _rainBehind;
    DriftField _dustFront;
    DriftField _snowFront;
    DriftField _rainFront;
    FogField _fogFront;
};
