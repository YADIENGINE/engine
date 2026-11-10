#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "AgentDocs/Recipes/Code/CharacterScene.h"
#include "AgentDocs/Recipes/Code/AnimatorFighter.h"
#include "AgentDocs/Recipes/Code/CollisionBullets.h"
#include "AgentDocs/Recipes/Code/DepthStage.h"
#include "AgentDocs/Recipes/Code/TimelineBoss.h"
#include "AgentDocs/Recipes/Code/TweenMenu.h"
#include "AgentDocs/Recipes/Code/UiMenu.h"
#include "AgentDocs/Recipes/Code/UiOptions.h"
#include "AgentDocs/Recipes/Code/FxBurst.h"
#include "AgentDocs/Recipes/Code/FxCues.h"
#include "AgentDocs/Recipes/Code/GameCamera.h"
#include "AgentDocs/Recipes/Code/Lighting.h"
#include "AgentDocs/Recipes/Code/GraphicAsset.h"
#include "AgentDocs/Recipes/Code/PixelSprite.h"
#include "AgentDocs/Recipes/Code/IconShape.h"
#include "AgentDocs/Recipes/Code/PoolTargets.h"
#include "AgentDocs/Recipes/Code/PopupEffects.h"
#include "AgentDocs/Recipes/Code/ProceduralLevel.h"
#include "AgentDocs/Recipes/Code/SaveProgress.h"
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include "AgentDocs/Recipes/Code/SimulationPlay.h"
#include "AgentDocs/Recipes/Code/SoundTable.h"
#include "AgentDocs/Recipes/Code/SpatialSound.h"
#include "AgentDocs/Recipes/Code/SpriteLayer.h"
#include "AgentDocs/Recipes/Code/TextLayout.h"
#include "AgentDocs/Recipes/Code/TuningValue.h"
#include "AgentDocs/Recipes/Code/UiSounds.h"
#include "AgentDocs/Recipes/Code/Weather.h"
#include "AgentDocs/Recipes/Code/WalkerSlide.h"
#include "Audio/SoundPlayer.h"
#include "Graphics/Surface/Surface.h"
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Modules/Persistence/Persistence.h"
#include "Options/GameOptions.h"
#include "Runtime/GameSettings.h"
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Simulation/ScenarioScript.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/StateHash.h"
#include "Tests/PaintHarness.h"
#include "Scene/Internals/SceneManager.h"
#include "Simulation/SimulationHost.h"
#include "Tests/Support/FakeSoundOutput.h"
#include "Tests/Support/TestSceneServices.h"
#include "Tests/Support/TempDirectory.h"
#include "Ui/Kernel/UiListener.h"
#include "Ui/Sound/UiSoundListener.h"

TEST_CASE("Engine.Recipes.MenuPressesAndAsksInADialog", "[engine][recipes]") {
    const FontFace font = FontFace::FromFile(ENGINE_RECIPE_FONT);
    RecipeMenu menu(&font);
    const auto press = [&menu](InputKey key) {
        DeviceInput input;
        input.keysPressed[static_cast<std::size_t>(key)] = true;
        menu.Input(input);
        menu.Update(0.05f);
    };
    menu.Update(0.05f);
    press(InputKey::Enter);
    CHECK(menu.Plays() == 1);
    press(InputKey::Down);
    press(InputKey::Enter);
    CHECK(menu.TilePresses() == 1); // The game's own widget takes the cursor like any other
    press(InputKey::Down);
    press(InputKey::Enter);
    CHECK(menu.AskingToQuit());
    menu.Update(0.05f);
    press(InputKey::Escape);
    CHECK_FALSE(menu.AskingToQuit());
    CHECK_FALSE(menu.Quit());
    PaintFrame([&menu](Painter& paint) { menu.Draw(paint); });
}

TEST_CASE("Engine.Recipes.OptionsScreenTakesTheGamesRows", "[engine][recipes]") {
    const FontFace font = FontFace::FromFile(ENGINE_RECIPE_FONT);
    RecipeOptions options(&font);
    const auto press = [&options](InputKey key) {
        DeviceInput input;
        input.keysPressed[static_cast<std::size_t>(key)] = true;
        options.Input(input);
        options.Update(0.02f);
        options.Update(0.02f);
    };
    options.Update(0.02f);
    options.Update(0.02f);
    press(InputKey::Q); // Back from the first tab to the last: the game's own
    press(InputKey::Down);
    press(InputKey::Enter);
    CHECK_FALSE(options.ShowHints());
    press(InputKey::Down);
    press(InputKey::Right);
    CHECK(options.Difficulty() == 2);
    press(InputKey::Escape);
    CHECK(options.Closed());
    PaintFrame([&options](Painter& paint) { options.Draw(paint); });
}

TEST_CASE("Engine.Recipes.LevelsScrollUnderTabs", "[engine][recipes]") {
    const FontFace font = FontFace::FromFile(ENGINE_RECIPE_FONT);
    RecipeLevels levels(&font);
    const auto press = [&levels](InputKey key) {
        DeviceInput input;
        input.keysPressed[static_cast<std::size_t>(key)] = true;
        levels.Input(input);
        levels.Update(0.02f);
        levels.Update(0.02f);
    };
    levels.Update(0.02f);
    levels.Update(0.02f);
    press(InputKey::E);
    CHECK(levels.World() == 1);
    press(InputKey::Down); // From the tabs into the grid
    press(InputKey::Down); // A row down
    press(InputKey::Right);
    press(InputKey::Enter);
    CHECK(levels.Chosen() == 6);
    PaintFrame([&levels](Painter& paint) { levels.Draw(paint); });
}

TEST_CASE("Engine.Recipes.SimulationPlaysInASceneAndWithoutOne", "[engine][recipes]") {
    TestSceneServices host;
    RecipePlayScene scene(host.services, 1);
    scene.Activate();
    DeviceInput right;
    right.keysDown[static_cast<std::size_t>(InputKey::D)] = true;
    for (int frame = 0; frame < 60; ++frame) {
        scene.ReadInput(right);
        scene.Tick(host.time.Advance(1.0f / 60.0f));
    }
    CHECK(scene.Runner().StepIndex() == 60);
    CHECK(scene.Sim().ShipX(1.0f) > 230.0f);

    const auto definition = RecipePlayDefinition();
    const auto replay = VerifyTape(definition, scene.Runner().Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hash == HashState(scene.Sim()));
    scene.Shutdown();
}

TEST_CASE("Engine.Recipes.SimulationEventsReachTheScene", "[engine][recipes]") {
    // Headless: firing under the rock emits a Hit, which tools can see.
    Scenario scenario(RecipePlayDefinition(), {.seed = 1, .players = 1});
    const auto& rules = dynamic_cast<const RecipePlaySimulation&>(scenario.Sim());
    const float rock = rules.RockX();
    scenario.Hold("Right");
    while (rules.ShipX(1.0f) < rock - 10.0f)
        scenario.Run();
    scenario.Release("Right");
    scenario.Tap("Fire");
    scenario.Run();
    REQUIRE_FALSE(scenario.Events().empty());
    CHECK(scenario.Events().back().name == "Hit");
    CHECK(rules.Score() == 1);

    // In the scene, the same Hit starts a hitstop through SceneServices::time: the next frame owes no game time.
    TestSceneServices host;
    RecipePlayScene scene(host.services, 1);
    scene.Activate();
    DeviceInput right;
    right.keysDown[static_cast<std::size_t>(InputKey::D)] = true;
    while (scene.Sim().ShipX(1.0f) < rock - 10.0f) {
        scene.ReadInput(right);
        scene.Tick(host.time.Advance(1.0f / 60.0f));
    }
    DeviceInput fire;
    fire.keysPressed[static_cast<std::size_t>(InputKey::Space)] = true;
    fire.keysDown[static_cast<std::size_t>(InputKey::Space)] = true;
    scene.ReadInput(fire);
    scene.Tick(host.time.Advance(1.0f / 60.0f));
    CHECK(scene.Sim().Score() == 1);
    CHECK(host.time.Advance(1.0f / 60.0f) == 0.0f);
    scene.Shutdown();
}

namespace {
// The editor's side of HostedSimulation, reduced to what a test can see: which runner is attached.
struct RecordingHost final : SimulationHost {
    [[nodiscard]] SimulationStart StartFor(const SimulationSettings&, SimulationStart fallback) override {
        fallback.seed = 9; // A loaded replay's start would replace the scene's own
        return fallback;
    }
    void Attach(SimulationRunner& runner) override { attached = &runner; }
    void Detach(SimulationRunner& runner) override {
        if (attached == &runner) attached = nullptr;
        ++detached;
    }
    SimulationRunner* attached = nullptr;
    int detached = 0;
};
} // namespace

TEST_CASE("Engine.Recipes.OneSceneRunsInTheGameAndInTheCatalog", "[engine][recipes]") {
    DeviceInput right;
    right.keysDown[static_cast<std::size_t>(InputKey::D)] = true;
    // The game: the scene manager hosts the class; with no simulation host the rules simply run.
    TestSceneServices game;
    RecipePlayScene played(game.services, 1);
    SceneManager scenes;
    scenes.Register(played);
    scenes.Request(played);
    REQUIRE(scenes.Commit({}));
    for (int frame = 0; frame < 30; ++frame) {
        scenes.Current()->ReadInput(right);
        scenes.Current()->Tick(game.time.Advance(1.0f / 60.0f));
    }
    CHECK(played.Runner().StepIndex() == 30);
    CHECK(played.Runner().Tape().start.seed == 1);
    CHECK(played.ExecuteAction("add_score").empty());
    scenes.Shutdown();
    CHECK(played.IsShutdown());

    // The editor: the catalog makes the same class; the host attaches its runner and chooses the start.
    RecordingHost recorder;
    TestSceneServices editor(&recorder);
    const auto definition = RecipePlaySceneDefinition();
    auto opened = definition.create(editor.services, SceneDefaults(definition));
    REQUIRE(dynamic_cast<RecipePlayScene*>(opened.get()));
    const auto& scene = static_cast<const RecipePlayScene&>(*opened);
    CHECK(recorder.attached == &scene.Runner());
    CHECK(scene.Runner().Tape().start.seed == 9);
    opened->Activate();
    opened->ReadInput(right);
    opened->Tick(1.0f / 60.0f);
    CHECK(scene.Runner().StepIndex() == 1);
    CHECK(opened->ExecuteAction("add_score").empty());
    CHECK(opened->ApplyParameter("seed", std::int64_t{2}) == "Unsupported parameter: seed");
    opened->Shutdown();
    opened.reset();
    CHECK(recorder.attached == nullptr);
    CHECK(recorder.detached == 1);
}

TEST_CASE("Engine.Recipes.LightingDarkensTheWorldBeforeTheSign", "[engine][recipes]") {
    PaintHarness h({1280.0f, 720.0f}, 1.0f);
    TestSceneServices services;
    RecipeLitView view(services.services);
    view.Lightning();
    view.UpdateReal(1.0f / 60.0f);
    view.Update(1.0f / 60.0f, {1.0f, 0.0f}, {800.0f, 500.0f});
    view.SetGuides(true);
    h.Begin();
    view.Draw(h.paint);
    h.End();
    CHECK(h.list.Rejected() == 0);
    // The world, the lights into their image, the stamp, then the sign and the guides undarkened.
    CHECK_FALSE(h.Draws().empty());
    const auto pipelines = h.device.CommandsOf(NullCommandType::SetPipeline);
    REQUIRE(pipelines.size() >= 4);
    CHECK(std::ranges::count_if(pipelines, [](const NullCommand* p) { return p->blend == GpuBlend::Multiply; }) == 1);
    CHECK(pipelines.back()->blend == GpuBlend::Premultiplied);
}

TEST_CASE("Engine.Recipes.WeatherTurnsIntoAStormAndLightningDraws", "[engine][recipes]") {
    TestSceneServices services;
    RecipeWeatherView view(services.services);
    view.StartStorm();
    view.Lightning({640.0f, 300.0f});
    for (int frame = 0; frame < 60; ++frame) {
        view.UpdateReal(1.0f / 60.0f);
        view.Update(1.0f / 60.0f);
    }
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    view.Draw(harness.paint);
    harness.End();
    CHECK(harness.list.Rejected() == 0);
    CHECK_FALSE(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.GameCameraFramesTwoPlayersOrSplits", "[engine][recipes]") {
    TestSceneServices services;
    RecipeCameraView view(services.services);
    for (int frame = 0; frame < 120; ++frame) // The players run apart
        view.Update(1.0f / 60.0f, {Float2{-1.0f, 0.0f}, Float2{1.0f, 0.3f}}, std::nullopt, false);
    const Camera2D& main = view.Views().Main();
    CHECK(main.Pose().zoom < 1.0f); // Zoomed out to keep both
    CHECK(main.Shows(view.Player(0)));
    CHECK(main.Shows(view.Player(1)));

    view.SetSplit(true);
    for (int frame = 0; frame < 120; ++frame)
        view.Update(1.0f / 60.0f, {Float2{-1.0f, 0.0f}, Float2{1.0f, 0.0f}}, Float2{900.0f, 300.0f}, frame == 0);
    REQUIRE(view.Views().Count() == 2);
    // Each at the field's edge, where its camera stops: the player's circle is in view.
    CHECK(view.Views()[0].Shows(view.Player(0), 22.0f));
    CHECK(view.Views()[1].Shows(view.Player(1), 22.0f));
    const Float2 aim = view.Views()[1].ToWorld({900.0f, 300.0f}); // The mouse is over the right view
    CHECK(view.Aim().x == Catch::Approx(aim.x));
    REQUIRE(services.sounds.Listeners().size() == 2); // Each view hears

    view.SetGuides(true);
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    view.Draw(harness.paint);
    harness.End();
    CHECK(harness.Draws().size() > 2);
    CHECK(harness.list.Rejected() == 0);
}

TEST_CASE("Engine.Recipes.CameraInTheRulesReplays", "[engine][recipes]") {
    const auto definition = RecipeScrollerDefinition();
    Scenario scenario(definition, {.players = 1});
    const auto& rules = dynamic_cast<const RecipeScrollerSimulation&>(scenario.Sim());
    CHECK_FALSE(rules.Awake(0));
    scenario.Hold("Right");
    scenario.Run(240);
    scenario.Release("Right");
    scenario.Run(120); // Let the camera settle
    CHECK(rules.Awake(0)); // Reached
    CHECK_FALSE(rules.Awake(RecipeScrollerSimulation::EnemyCount - 1));
    const float scrolled = rules.ScrolledTo();
    CHECK(scrolled > 0.0f);
    scenario.Hold("Left");
    scenario.Run(240);
    CHECK(rules.ScrolledTo() == scrolled);                          // Never back
    CHECK(rules.ShipX(1.0f) >= rules.Camera().VisibleArea().x);   // Held on screen
    CHECK(rules.Camera().VisibleArea().x >= scrolled - 0.01f);
    CHECK(scenario.State().Members().contains("camera.center.x"));
    const auto replay = VerifyTape(definition, scenario.Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hash == scenario.Hash());
}

TEST_CASE("Engine.Recipes.TheStageDrawsThroughTheRulesCamera", "[engine][recipes]") {
    TestSceneServices services;
    RecipeScrollerScene scene(services.services, true);
    scene.Activate();
    DeviceInput right;
    right.keysDown[static_cast<std::size_t>(InputKey::D)] = true;
    for (int frame = 0; frame < 240; ++frame) {
        scene.ReadInput(right);
        scene.TickReal(1.0f / 60.0f);
        scene.Tick(1.0f / 60.0f);
    }
    // The stage hears from the rules' camera, which has scrolled on.
    REQUIRE(services.sounds.Listeners().size() == 1);
    CHECK(services.sounds.Listeners()[0].position.x > 640.0f);
    CHECK(scene.Rules().ScrolledTo() > 0.0f);
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    scene.Render(harness.paint);
    harness.End();
    CHECK(harness.list.Rejected() == 0);
    CHECK_FALSE(harness.Draws().empty());
    scene.Shutdown();
}

TEST_CASE("Engine.Recipes.StreetSortsByDepthJumpsAndSplits", "[engine][recipes]") {
    TestSceneServices services;
    const auto definition = RecipeStreetSceneDefinition();
    auto opened = definition.create(services.services, SceneDefaults(definition));
    auto& street = dynamic_cast<RecipeStreetScene&>(*opened);
    street.Activate();
    DeviceInput jump;
    jump.keysPressed[static_cast<std::size_t>(InputKey::Space)] = true;
    street.ReadInput(jump);
    street.Tick(1.0f / 60.0f);
    CHECK(street.Walker(0).height > 0.0f);
    street.ReadInput({});
    for (int frame = 0; frame < 120; ++frame)
        street.Tick(1.0f / 60.0f);
    CHECK(street.Walker(0).height == 0.0f); // Landed
    CHECK(opened->ApplyParameter("split", true).empty());
    CHECK(street.Views().Count() == 2);
    street.Tick(1.0f / 60.0f);
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    street.Render(harness.paint);
    harness.End();
    CHECK(harness.list.Rejected() == 0);
    CHECK_FALSE(harness.Draws().empty());
    street.Shutdown();
}

TEST_CASE("Engine.Recipes.ProceduralLevelReplaysEveryLayout", "[engine][recipes]") {
    const auto definition = RecipeDungeonDefinition();
    for (const char* layout : {"rooms", "caves", "walk"}) {
        INFO(layout);
        SimulationStart start{.seed = 7, .players = 1};
        start.parameters["layout"] = layout;
        Scenario scenario(definition, start);
        const auto& rules = dynamic_cast<const RecipeDungeonSimulation&>(scenario.Sim());
        const RecipeDungeonRun& run = rules.Run();
        REQUIRE(run.map[run.player] == MapCell::Floor);
        CHECK(run.gems.size() >= 5);
        for (const Vec2 gem : run.gems)
            CHECK(run.map[gem] == MapCell::Floor);
        scenario.Hold("Right");
        scenario.Run(120);
        scenario.Release("Right");
        scenario.Hold("Down");
        scenario.Run(120);
        CHECK(scenario.State().Members().contains("loot.last"));
        const auto replay = VerifyTape(definition, scenario.Tape());
        CHECK_FALSE(replay.mismatch);
        CHECK(replay.hash == scenario.Hash());

        PaintHarness harness({1280.0f, 720.0f}, 1.0f);
        harness.Begin();
        DrawRecipeDungeon(harness.paint, rules);
        harness.End();
        CHECK(harness.list.Rejected() == 0);
    }
    // Another seed, another level; the layouts differ from each other too.
    Scenario first(definition, {.seed = 7, .players = 1});
    Scenario second(definition, {.seed = 8, .players = 1});
    CHECK_FALSE(dynamic_cast<const RecipeDungeonSimulation&>(first.Sim()).Run().map ==
                dynamic_cast<const RecipeDungeonSimulation&>(second.Sim()).Run().map);
}
TEST_CASE("Engine.Recipes.ProceduralLootComesWithPity", "[engine][recipes]") {
    DrawMemory memory;
    Rng rng(1);
    int sinceCrown = 0, longest = 0;
    for (int gem = 0; gem < 500; ++gem) {
        sinceCrown = RecipeDungeonLoot().Draw(memory, rng) == 3 ? 0 : sinceCrown + 1;
        longest = std::max(longest, sinceCrown);
    }
    CHECK(longest <= 15);
    CHECK(RecipeDungeonSceneDefinition().parameters.back().choices.size() == 3);
}

TEST_CASE("Engine.Recipes.CameraScenesAreDefined", "[engine][recipes]") {
    CHECK(RecipeCameraSceneDefinition().parameters.front().id == "split");
    CHECK(RecipeScrollerSceneDefinition().parameters.front().id == "guides");
}

TEST_CASE("Engine.Recipes.CollisionBulletsReplayAndDraw", "[engine][recipes]") {
    const auto definition = RecipeShooterDefinition();
    Scenario scenario(definition, {.seed = 4, .players = 1});
    int shots = 0;
    const auto& simulation = dynamic_cast<const RecipeShooterSimulation&>(scenario.Sim());
    for (int round = 0; round < 40 && simulation.Score() == 0; ++round) {
        scenario.Tap("Fire");
        ++shots;
        scenario.Hold("Left");
        scenario.Run(15);
        scenario.Release("Left");
        scenario.Tap("Fire");
        scenario.Hold("Right");
        scenario.Run(30);
        scenario.Release("Right");
    }
    CHECK(shots > 0);
    CHECK(simulation.Score() >= 1);
    CHECK(simulation.Enemies().Count() + static_cast<std::size_t>(simulation.Score()) == 8);
    CHECK(simulation.Collision().Count() == simulation.Enemies().Count() + 2); // Removed enemies took their colliders
    for (const RecipeEnemy& enemy : simulation.Enemies()) // Every collider stands where its enemy is
        CHECK(simulation.Collision().PoseOf(enemy.collider) == enemy.pose);
    CHECK_FALSE(VerifyTape(definition, scenario.Tape()).mismatch);

    PaintHarness harness;
    harness.Begin();
    DrawRecipeShooter(harness.paint, simulation, true);
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.AnimatorFighterPlaysReplaysAndDraws", "[engine][recipes]") {
    const auto definition = RecipeFighterDefinition();
    Scenario scenario(definition, {.seed = 1, .players = 1});
    const auto& simulation = dynamic_cast<const RecipeFighterSimulation&>(scenario.Sim());
    scenario.Hold("Walk");
    scenario.Run(30);
    scenario.Release("Walk");
    CHECK(simulation.Pose(1.0f).frame >= 0);
    scenario.Tap("Attack");
    scenario.Run(20);
    CHECK(simulation.Hits() == 1);
    CHECK(simulation.Recoil(1.0f) > 0.0f);
    scenario.Tap("Attack"); // Starts over before the last one ended
    scenario.Run(60);
    CHECK(simulation.Hits() == 2);
    const auto replay = VerifyTape(definition, scenario.Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK_FALSE(FindDivergence(definition, scenario.Tape()));

    PaintHarness harness;
    harness.Begin();
    DrawRecipeFighter(harness.paint, simulation, 0.5f);
    harness.End();
    CHECK(harness.Draws().size() >= 1);
}

TEST_CASE("Engine.Recipes.AnimatorScenarioInTheRecipePasses", "[engine][recipes]") {
    SimulationRegistry registry;
    registry.Add(RecipeFighterDefinition());
    const auto script = JsonIO::Parse(R"json({ "simulation": "myproject.fighter", "seed": 1, "script": [
  { "expect": { "animator.clip": "idle", "hits": 0 } },
  { "tap": "Attack" }, { "run": 1 }, { "expect": { "animator.clip": "attack" } },
  { "run": 18 }, { "expect": { "hits": 1 } },
  { "run": 11 }, { "expect": { "animator.clip": "idle" } } ] })json");
    const auto result = RunScenarioScript(registry, script);
    CHECK(result.passed);
}

TEST_CASE("Engine.Recipes.AnimatorSceneBuilds", "[engine][recipes]") {
    const auto definition = RecipeFighterSceneDefinition();
    CHECK(definition.id == "myproject.fighter");
    CHECK(definition.create);
}

TEST_CASE("Engine.Recipes.TweenMenuMovesAndDraws", "[engine][recipes]") {
    RecipeTweenMenu menu;
    menu.Open();
    CHECK(menu.TitleScale() == 0.0f);
    menu.AddCoin({10.0f, 10.0f});
    menu.AddCoin({20.0f, 10.0f});
    menu.MoveCursor({100.0f, 100.0f});
    for (int frame = 0; frame < 15; ++frame)
        menu.Update(1.0f / 60.0f);
    CHECK(menu.TitleScale() > 1.0f); // Overshooting
    CHECK(menu.TitleBeats() == 0);
    menu.CollectCoin(0);
    for (int frame = 0; frame < 30; ++frame)
        menu.Update(1.0f / 60.0f);
    CHECK(menu.TitleScale() == 1.0f);
    CHECK(menu.TitleBeats() == 1);
    CHECK(menu.Coins() == 1); // Collected and gone

    PaintHarness harness;
    harness.Begin();
    menu.Draw(harness.paint);
    harness.End();
    CHECK(harness.Draws().size() >= 1);
}

TEST_CASE("Engine.Recipes.TimelineBossReplaysAndDraws", "[engine][recipes]") {
    const auto definition = RecipeBossDefinition();
    Scenario scenario(definition, {.seed = 1, .players = 1});
    const auto& simulation = dynamic_cast<const RecipeBossSimulation&>(scenario.Sim());
    scenario.Run(120);
    CHECK_FALSE(simulation.BossIn());
    scenario.Run(1);
    CHECK(simulation.BossIn());
    CHECK(simulation.Charge(1.0f) == 0.0f);
    scenario.Run(44);
    CHECK(simulation.Shots() == 0);
    CHECK(simulation.Charge(1.0f) > 0.9f);
    scenario.Run(1);
    CHECK(simulation.Shots() == 1); // The step whose clock is 45 meets "fire"
    scenario.Run(90);
    CHECK(simulation.Shots() == 2);
    CHECK_FALSE(VerifyTape(definition, scenario.Tape()).mismatch);

    PaintHarness harness;
    harness.Begin();
    DrawRecipeBoss(harness.paint, simulation, 0.5f);
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.TimelineScenarioInTheRecipePasses", "[engine][recipes]") {
    SimulationRegistry registry;
    registry.Add(RecipeBossDefinition());
    const auto script = JsonIO::Parse(R"json({ "simulation": "myproject.boss", "seed": 1, "script": [
  { "expect": { "phase": "intro", "shots": 0 } },
  { "run": 120 }, { "expect": { "phase": "boss", "bossStart": -1 } },
  { "run": 46 }, { "expect": { "bossStart": 120, "attackPhase": "fire", "shots": 1 } } ] })json");
    const auto result = RunScenarioScript(registry, script);
    CHECK(result.passed);
}

TEST_CASE("Engine.Recipes.CollisionScenarioInTheRecipePasses", "[engine][recipes]") {
    SimulationRegistry registry;
    registry.Add(RecipeShooterDefinition());
    const auto script = JsonIO::Parse(R"json({ "simulation": "myproject.shooter", "seed": 4, "script": [
  { "expect": { "collision.colliders": 10, "enemies.count": 8 } },
  { "hold": "Right" }, { "run": 60 },
  { "expect": { "playerX": { "min": 639, "max": 641 },
                "collision.collider[0].position.x": { "min": 639, "max": 641 } } } ] })json");
    const auto result = RunScenarioScript(registry, script);
    CHECK(result.passed);
}

TEST_CASE("Engine.Recipes.PoolTargetsReplayAndDraw", "[engine][recipes]") {
    const auto definition = RecipeHomingDefinition();
    Scenario scenario(definition, {.seed = 3, .players = 1});
    const auto& rules = dynamic_cast<const RecipeHomingSimulation&>(scenario.Sim());
    for (int volley = 0; volley < 12; ++volley) {
        scenario.Tap("Fire");
    scenario.Run(20);
    }
    CHECK(rules.Run().score >= 2);
    CHECK(rules.Run().drones.Count() == 5);
    const auto state = scenario.State();
    CHECK(state.Members().contains("drones[0].handle.generation"));
    CHECK(state.Members().contains("drones.generations[0]"));
    const auto replay = VerifyTape(definition, scenario.Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hash == scenario.Hash());

    PaintHarness harness;
    harness.Begin();
    DrawRecipeHoming(harness.paint, rules);
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.PoolScenarioInTheRecipePasses", "[engine][recipes]") {
    SimulationRegistry registry;
    registry.Add(RecipeHomingDefinition());
    const auto script = JsonIO::Parse(R"json({ "simulation": "myproject.homing", "seed": 3, "script": [
  { "run": 1 }, { "expect": { "drones.count": 5, "missiles.count": 0 } },
  { "tap": "Fire" }, { "run": 3 }, { "tap": "Fire" }, { "run": 3 },
  { "expect": { "missiles.count": 2, "missiles[0].target.index": 3, "missiles[1].target.index": 3 } },
  { "until": { "score": 1 }, "limit": 300 },
  { "expect": { "drones.count": 4, "drones.free[0]": 3, "drones.generations[3]": 2 } } ] })json");
    const auto result = RunScenarioScript(registry, script);
    CHECK(result.passed);
}

TEST_CASE("Engine.Recipes.CollisionSceneOffersTheSwitch", "[engine][recipes]") {
    const auto definition = RecipeShooterSceneDefinition();
    REQUIRE(definition.parameters.size() == 2);
    CHECK(definition.parameters[1].id == "showColliders");
    CHECK(std::get<bool>(definition.parameters[1].initial));
    const RecipeShooterSimulation simulation(SimulationStart{.seed = 1, .players = 1});
    const auto drawn = [&](bool showColliders) {
        PaintHarness harness;
        harness.Begin();
        DrawRecipeShooter(harness.paint, simulation, showColliders);
        harness.End();
        std::size_t vertices = 0;
        for (std::size_t i = 0; i < harness.Draws().size(); ++i)
            vertices += harness.Drawn(i).size();
        return vertices;
    };
    CHECK(drawn(true) > drawn(false));
}

TEST_CASE("Engine.Recipes.SimulationSceneRunsOutsideTheEditor", "[engine][recipes]") {
    const auto definition = RecipePlaySceneDefinition();
    CHECK(definition.actions.front().id == "add_score");
    CHECK(definition.parameters.front().id == "seed");
}

TEST_CASE("Engine.Recipes.SavedProgressSurvivesReopening", "[engine][recipes]") {
    TempDirectory directory;
    CHECK(SaveRecipeProgress(directory.Path(), 7) == 7);
    Persistence progress(directory.Path() / "progress.json");
    REQUIRE(progress.Load() == SaveLoadResult::Loaded);
    CHECK(progress.GetInt("completed") == 7);
    GameOptions options(directory.Path() / "settings.json");
    REQUIRE(options.Load() == OptionsLoadResult::Loaded);
    CHECK(options.GetInt("myproject.hints") == 1);
}

TEST_CASE("Engine.Recipes.CuesPlayFromRulesAndMoments", "[engine][recipes]") {
    TestSceneServices services;
    TimeScale& time = services.time;
    RecipeCueView view(services.services);
    const auto drawn = [&] {
        PaintHarness harness;
        harness.Begin();
        view.Draw(harness.paint);
        harness.End();
        return !harness.Draws().empty();
    };
    view.UpdateReal(0.1f);
    view.Update(0.1f, 0);
    CHECK(time.Advance(0.01f) == 0.01f);
    view.UpdateReal(0.1f);
    view.Update(0.1f, 1); // The count went up: "player_hit" and its hitstop
    CHECK(time.Advance(0.01f) == 0.0f);
    CHECK(drawn());

    RecipeCueView swinging(services.services);
    swinging.Swing();
    swinging.Update(0.1f, 0);
    swinging.Update(0.15f, 0); // Crosses the moment "swing_land"
    PaintHarness harness;
    harness.Begin();
    swinging.Draw(harness.paint);
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.ViewsRecordAndCachedLayerInvalidates", "[engine][recipes]") {
    PaintHarness harness;
    RecipeCharacterView character;
    character.Pose({12.0f, 0.0f}, 0.1f);
    Surface layer({320.0f, 240.0f});
    TestSceneServices services;
    Stage stage(services.services);
    TriggerRecipeFx(stage.Fx(), {160.0f, 100.0f});
    stage.UpdateReal(0.01f);
    stage.Update(0.01f);
    struct Field final : StageView {
        void DrawWorld(Painter&, const StagePass&) const override {} // The burst is the whole world here
    };
    harness.Begin();
    CHECK(harness.paint.Stale(layer));
    DrawRecipeLayer(harness.paint, layer, harness.paint.Bounds(), 0.5f);
    character.Draw(harness.paint);
    DrawRecipeIcons(harness.paint, {200.0f, 100.0f}, 16.0f, Rgb(0xffffff));
    DrawGoblin(harness.paint, {240.0f, 20.0f, 60.0f, 100.0f}, GoblinKind::Forest, 0.5f);
    DrawGoblin(harness.paint, {240.0f, 120.0f, 100.0f, 60.0f}, GoblinKind::Ice, 0.5f);
    DrawGuard(harness.paint, {300.0f, 20.0f, 60.0f, 90.0f}, GuardKind::Royal, 0.6f, {.hurt = 0.5f});
    stage.Draw(harness.paint, Field{});
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
    harness.Begin();
    CHECK_FALSE(harness.paint.Stale(layer));
    harness.End();
    layer.Resize({160.0f, 120.0f});
    harness.Begin();
    CHECK(harness.paint.Stale(layer));
    harness.End();
    stage.Fx().Clear();
}

TEST_CASE("Engine.Recipes.SceneAndTextWorkWithoutWindow", "[engine][recipes]") {
    TestSceneServices host;
    RecipeScene scene(host.services);
    scene.Activate();
    scene.Tick(0.1f);
    scene.Deactivate();
    scene.Shutdown();
    CHECK(scene.IsShutdown());
    const auto definition = RecipeCharacterDefinition();
    REQUIRE(definition.create);
    auto opened = definition.create(host.services, {}); // The same class, made by the catalog
    opened->Activate();
    opened->Tick(0.1f);
    opened->Shutdown();
    RecipeTextView text(ENGINE_RECIPE_FONT);
    CHECK(text.Measured().x > 0.0f);
    CHECK(RecipeRadius() > 0.0f);
    CHECK_FALSE(RecipeRuns());
}

TEST_CASE("Engine.Recipes.PopupEffectsDrawWithThePublicApi", "[engine][recipes]") {
    const auto font = FontFace::FromFile(ENGINE_RECIPE_FONT);
    PaintHarness harness;
    harness.context.font = &font;
    RecipePopupEffects effects;
    effects.SetQuality({.particlePercent = 50, .flashes = false});
    effects.Hit({120.0f, 100.0f}, 120, RecipeHitGroup::Melee, 1);
    effects.Hit({120.0f, 100.0f}, 30, RecipeHitGroup::Melee, 1);
    effects.Heal({180.0f, 100.0f}, 12);
    effects.Reward({250.0f, 100.0f}, "BONUS");
    effects.Notice({320.0f, 100.0f}, "SAVED");
    effects.Update(0.125f);
    harness.Begin();
    CHECK_NOTHROW(effects.Draw(harness.paint));
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
    effects.Clear();
    harness.Begin();
    effects.Draw(harness.paint);
    harness.End();
    CHECK(harness.Draws().empty());
}

TEST_CASE("Engine.Recipes.UiSoundsPlayWithNoCodeOnTheScreen", "[engine][recipes]") {
    struct Output final : FakeSoundOutput {
        [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start) override {
            ++plays;
            return FakeSoundOutput::StartVoice(id, start);
        }
        int plays = 0;
    };
    GameSettings settings;
    AddRecipeUiSounds(settings);
    Output output;
    SoundPlayer sounds(settings.sounds, output);
    CheckUiSounds(settings.uiSounds, sounds, "settings");
    UiSoundListener listener(sounds);
    SetActiveUiListener(&listener);
    {
        const FontFace font = FontFace::FromFile(ENGINE_RECIPE_FONT);
        RecipeShop shop(&font);
        const auto press = [&shop](InputKey key) {
            DeviceInput input;
            input.keysPressed[static_cast<std::size_t>(key)] = true;
            input.keysDown[static_cast<std::size_t>(key)] = true;
            shop.Input(input);
            shop.Update(1.0f / 60.0f);
        };
        shop.Update(1.0f / 60.0f);
        shop.Update(1.0f / 60.0f); // Silent: its own sounds are not given here, so the look set is empty
        CHECK(output.plays == 0);
        press(InputKey::Enter);
        CHECK(output.plays == 0);

        RecipeShop own(&font, true);
        own.Update(1.0f / 60.0f);
        own.Update(1.0f / 60.0f);
        DeviceInput down;
        down.keysPressed[static_cast<std::size_t>(InputKey::Down)] = true;
        own.Input(down);
        own.Update(1.0f / 60.0f);
        CHECK(output.plays == 1); // The cursor's move
        DeviceInput enter;
        enter.keysPressed[static_cast<std::size_t>(InputKey::Enter)] = true;
        own.Input(enter);
        own.Update(1.0f / 60.0f);
        CHECK(own.Purchases() == 1);
        CHECK(output.plays == 1); // `.sound = None`: the game plays its own
    }
    SetActiveUiListener(nullptr);
}

TEST_CASE("Engine.Recipes.SoundTablePlaysThroughAPlayer", "[engine][recipes]") {
    struct Output final : FakeSoundOutput {
        [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start) override {
            ++plays;
            return FakeSoundOutput::StartVoice(id, start);
        }
        int plays = 0;
    };
    GameSettings settings;
    AddRecipeSounds(settings);
    CHECK(CatalogWithSounds(settings.audio, settings.sounds).effects.size() == 4);
    Output output;
    SoundPlayer sounds(settings.sounds, output);
    CHECK(sounds.Has(RecipeSfx::UiMove));
    CHECK(RecipeHit(sounds));
    CHECK_FALSE(RecipeHit(sounds)); // Within the row's cooldown
    sounds.Update(0.25f);
    CHECK(RecipeHit(sounds));
    CHECK(output.plays == 2);
}

TEST_CASE("Engine.Recipes.SpatialSoundFollowsTheState", "[engine][recipes]") {
    GameSettings settings;
    AddRecipeNoises(settings);
    CHECK(CatalogWithSounds(settings.audio, settings.sounds).effects.size() == 4);
    FakeSoundOutput output;
    SoundPlayer sounds(settings.sounds, output);
    RecipeHauntSounds haunt(sounds);
    REQUIRE(output.started.size() == 3); // Hum, heart and the first ambience layer
    CameraView view;
    view.target = {0.0f, 0.0f};
    view.offset = {640.0f, 360.0f};
    view.viewport = {0.0f, 0.0f, 1280.0f, 720.0f};

    sounds.SetListener({.position = view.target, .panWidth = view.VisibleArea().width / 2.0f}); // A Stage does this
    haunt.Update({.player = {0.0f, 0.0f}, .ghost = {600.0f, 0.0f}});
    CHECK(output.Of(0).pan > 0.0f); // Right of the camera, heard on the right
    CHECK(output.Of(1).volume == 0.0f);
    CHECK(haunt.BeatsPerSecond() == Catch::Approx(0.9f));
    haunt.Update({.player = {0.0f, 0.0f}, .ghost = {-50.0f, 0.0f}});
    CHECK(output.Of(0).pan < 0.0f);
    CHECK(output.Of(1).volume > 0.9f);
    CHECK(output.Of(1).pitch > 2.0f);

    haunt.Update({.player = {0.0f, 0.0f}, .ghost = {-50.0f, 0.0f}, .indoors = true});
    REQUIRE(output.started.size() == 4); // Rain joined
    CHECK(output.Of(2).volume == 0.0f);  // Wind out
    CHECK(output.Of(3).volume == 0.8f);
    CHECK(output.Of(3).muffle == 0.8f);
}
TEST_CASE("Engine.Recipes.WalkerSlideFindsTheDoorway", "[engine][recipes]") {
    Scenario scenario(RecipeWalkerDefinition(), {.players = 1});
    const auto& rules = dynamic_cast<const RecipeWalkerSimulation&>(scenario.Sim());
    scenario.Hold("Down"); // Straight at the wall, 1 unit beside the doorway's edge: corner assist finds it
    scenario.Run(100);
    CHECK(rules.Feet().y > 110.0f);
    CHECK(rules.Feet().x > 154.0f);
    CHECK(rules.Feet().x < 166.0f);
    scenario.Release("Down");
    scenario.Hold("Up"); // Up and right: past the doorway, the feet meet the wall's underside and slide along it
    scenario.Hold("Right");
    scenario.Run(90);
    CHECK(rules.AgainstWall());
    CHECK(rules.Feet().x > 175.0f);
    CHECK(rules.Feet().y > 110.0f);
}
