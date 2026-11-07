# Stage

A `Stage` (`Core/Stage/Stage.h`) is a scene's view side in one place: its cameras, its effects, an optional light layer,
optional weather on screen, and the sound listeners. It draws them in one fixed order. The scene says what is there,
pass by pass, through a `StageView`; it never writes the order itself, so a pass the engine adds later lands in the right
place without a change to the scene. Decision record: [D54](../Design/README.md#d54-stage).

Every part stays usable on its own ([Camera](../Camera.md), [Effects](Effects.md), [Lighting](Lighting.md),
[Atmosphere](Atmosphere.md)); a stage only assembles them. It is view code: it decides nothing, and a simulation cannot
include it.

## A scene with a stage

```cpp
class MyScene final : public Scene, StageView {
public:
    explicit MyScene(const SceneServices& services)
        : _stage(services, {.camera = {.limits = Level}, .light = LightLayerOptions{}}) {
        _stage.Cameras().Main().Follow(_player);
        _stage.Cameras().Snap();
    }

private:
    void UpdateReal(float seconds) override { _stage.UpdateReal(seconds); } // Even in a hitstop or a pause
    void Update(float seconds) override {
        _stage.Cameras().Main().Follow(_player); // What the cameras keep in view
        _stage.Update(seconds);                  // Game time
    }
    void Draw(Painter& paint) const override { _stage.Draw(paint, *this); }
    void DrawWorld(Painter& paint, const StagePass&) const override { paint.Circle(_player, 20.0f, Fill(Hero)); }
    void DescribeLight(const Painter&, StageLight& light) const override {
        light.ambient = Night;
        light.lights.push_back({.at = _player, .radius = 240.0f});
    }

    Stage _stage;
    Float2 _player;
};
```

The stage is built from the scene's `SceneServices` and borrows them: the sound player for the listeners, the time scale
for cues that change time (`FxHitstop`, `FxSlowMotion`), and the player's effect choice, which it follows when the options
change. Effects play in the world: the camera's limits, or the canvas without them.

## The frame

For each view (one, or one per player of a split screen):

| Pass | Who draws | Coordinates |
| --- | --- | --- |
| Background | `DrawBackground` | None set: each layer opens `paint.Camera(pass.view.Parallaxed(depth))` |
| Floor | `DrawFloor` | World |
| Ground effects | The stage: marks (`FxStage::Mark`) | World |
| Atmosphere behind | The stage, with weather on | |
| World | `DrawWorld` (required) | World |
| Air of the game's own | `DrawAir`: a `DriftField` of ash, leaves | None set: `field.Draw(paint, amount, pass.view)` |
| Atmosphere in front | The stage | |
| Light | The stage: ambient and lights from `DescribeLight`, still occluders, the effects' flashes | |
| Glow | `DrawGlow`: what shines by itself, after the dark | World |
| World effects | The stage: particles, rings, text, bolts | World |
| Guides | The stage, when `SetGuides` asks | Canvas |

Then once over the canvas: the effects' overlay (flash, vignette, banners), then `DrawHud` (score, the scene's `Ui`).
`StagePass::view` is what that view shows this frame, the effects' shake and shot included; `StagePass::camera` says which
view of a split it is. `DescribeLight` runs once a frame, before the first view, and the same light shows in every view.

## Optional parts

`StageSettings` switches them on; a scene without them pays nothing.

- `light`: a `LightLayer` with its options. `Occluders()` holds what stands still and stops light (fill it once, for
  example with `AddOccluders` from colliders); `DescribeLight` gives what moves each frame. `Light()` returns the layer, to
  replace it with other options.
- `atmosphere`: an `Atmosphere` with its look. `Weather()` is a `SecondsTween<Weather>`: `To` changes the weather over
  game time, `Snap` sets it (each frame, when the weather lives in a simulation). `Atmosphere()` returns it, to replace
  its look.
- `listeners` (on by default): every frame the stage puts one sound listener on each view, its middle, turned with it, panning
  fully at its side edge. Turn it off when the game places its own listener, such as a character. The listeners
  belong to the game's one sound player, so only one stage should set them at a time; a scene without a stage that
  plays positioned sounds sets its own (`SoundPlayer::SetListener`).

Asking for a part that is off throws `std::logic_error`.

## Two clocks

The stage has the scene's two hooks:

- `UpdateReal(realSeconds)`, from `Scene::UpdateReal`, runs every frame, a hitstop and a pause included: banners, toasts,
  how long slow motion looks slow, a changed effect choice, the listeners.
- `Update(gameSeconds)`, from `Scene::Update`, after the cameras were told what to follow: the cameras, the weather and the
  effects' lifetimes and cue delays. A scene's `Update` does not run while game time stands still, so a hitstop holds
  them.

Each call moves only its own clock's parts. A scene in `TimeDomain::Real` gets real seconds in both hooks and still moves
nothing twice.

## Views from elsewhere

`SetViews(views)` draws and hears through the given views instead of the stage's own cameras, until the next `SetViews`;
an empty span returns to the own cameras. The views are copied. That is how a camera that lives in a simulation
([D35](../Camera.md#a-camera-in-the-rules)) is shown between its steps:

```cpp
void Update(float seconds) override {
    (void)_hosted.Runner().Advance(seconds);
    const std::array views{_hosted.Sim().Camera().View(_hosted.Runner().Alpha())};
    _stage.SetViews(views);
    _stage.Update(seconds);
}
```

It may be called at any time; calling it in the constructor too makes the first frame look through the views. Several
views make a split screen, drawn through the same passes. With its own cameras the stage splits through
`Cameras().Split`.

## Depth

A 2.5D world is drawn in the world pass in depth order, with heights and shadows:

- `DepthOrder<Key>` (`Stage/DepthOrder.h`) sorts keys by the y of the point each thing stands on; things that stand level
  keep the order they were added in, so they never swap from one frame to the next. A key of the game's own (a kind and an
  index) lets several kinds share one order. It holds no drawing and no callbacks.
- `Raised(paint, height)` (`Graphics/Draw/Elevation.h`) draws what follows that far above where it stands, and
  `DrawGroundShadow(paint, ground, height)` puts its shadow on the ground, shrinking and fading as it rises. Draw the
  shadows in `DrawFloor`, so none falls over a character.
- The background pass draws far layers at their own depth through `CameraView::Parallaxed`: 0 stays on the screen, 1 moves
  with the world.
- `FxStage::Mark` leaves a mark on the ground (a scorch, a splash, dust), drawn under everything that stands.

Recipe: [Depth stage](../../AgentDocs/Recipes/DepthStage.md); a simulation's camera: [Game camera](../../AgentDocs/Recipes/GameCamera.md).

## Guides

`SetGuides({.cameras = true, .lights = true})` draws, over each view, how each of the stage's own cameras follows and
where the lights stand and what stops them. A camera from a simulation has no `Camera2D` here: draw its guides in
`DrawHud` with `DrawCameraGuides`.
