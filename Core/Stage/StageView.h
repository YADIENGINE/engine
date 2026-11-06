#pragma once
#include <cstddef>
#include <vector>
#include "Camera/CameraView.h"
#include "Graphics/Light/LightOccluders.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Color4.h"

class Painter;

// One view's turn in Stage::Draw, given to each pass of a StageView.
struct StagePass {
    // What this view shows this frame, the effects' shake and shot included. The floor, world and glow passes already
    // draw through it; the background picks its depth: paint.Camera(pass.view.Parallaxed(0.3f)).
    CameraView view;
    std::size_t camera = 0; // Which view of a split screen: 0 is the main one
};

// What lights the world this frame, filled by StageView::DescribeLight once a frame and shown in every view. The stage
// starts it empty each frame and adds its still occluders (Stage::Occluders) and the effects' short flashes.
struct StageLight {
    Color4 ambient{255, 255, 255, 255}; // The light everything gets; white: lights change nothing. Alpha is ignored.
    std::vector<LightSource> lights;
    LightOccluders moving; // Outlines of what moves, from where it is drawn this frame
};

// What a scene shows through a Stage, pass by pass. Stage::Draw calls these in a fixed order for each view: background,
// floor, (ground effects, atmosphere behind), world, air, (atmosphere in front, light), glow, (world effects); then
// once over the canvas (the effects' overlay) and the HUD. Only DrawWorld is required. It is passed to Stage::Draw for
// the call and never kept, so a scene is usually its own StageView: `class MyScene final : public Scene, StageView`.
class StageView {
public:
    virtual ~StageView() = default;

    // Far behind the world: sky, distant hills, stars. Outside paint.Camera, so each layer opens its own depth with
    // paint.Camera(pass.view.Parallaxed(factor)): 0 stays on the screen, 1 moves with the world.
    virtual void DrawBackground(Painter& /*paint*/, const StagePass& /*pass*/) const {}
    // What everything stands on, in world coordinates, and their shadows (DrawGroundShadow). Ground effects (marks)
    // come next, then the atmosphere's back layers.
    virtual void DrawFloor(Painter& /*paint*/, const StagePass& /*pass*/) const {}
    // What stands in the world, in world coordinates. In a 2.5D world sort it with DepthOrder.
    virtual void DrawWorld(Painter& paint, const StagePass& pass) const = 0;
    // Drifting things of the game's own (a DriftField of ash or leaves), after the world and before the atmosphere's
    // front layers. Outside paint.Camera, as a DriftField wants: field.Draw(paint, amount, pass.view).
    virtual void DrawAir(Painter& /*paint*/, const StagePass& /*pass*/) const {}
    // What shines by itself, in world coordinates, after the light so the dark does not touch it: neon, lamps, eyes.
    virtual void DrawGlow(Painter& /*paint*/, const StagePass& /*pass*/) const {}
    // Once over the whole canvas, last: the score, the scene's Ui, a split screen's frames and labels.
    virtual void DrawHud(Painter& /*paint*/) const {}
    // Once a frame, when the stage has light (StageSettings::light): the ambient, the lights and what moves. 'paint' is
    // for its clock (paint.Time(), for a flicker) and quality; nothing is drawn here.
    virtual void DescribeLight(const Painter& /*paint*/, StageLight& /*light*/) const {}
};
