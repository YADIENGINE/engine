#include <catch2/catch_test_macros.hpp>
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "AgentDocs/Recipes/Code/CharacterScene.h"
#include "AgentDocs/Recipes/Code/FxBurst.h"
#include "AgentDocs/Recipes/Code/GraphicAsset.h"
#include "AgentDocs/Recipes/Code/IconShape.h"
#include "AgentDocs/Recipes/Code/PopupEffects.h"
#include "AgentDocs/Recipes/Code/SaveProgress.h"
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include "AgentDocs/Recipes/Code/SimulationPlay.h"
#include "AgentDocs/Recipes/Code/SpriteLayer.h"
#include "AgentDocs/Recipes/Code/TextLayout.h"
#include "AgentDocs/Recipes/Code/ThemedMenu.h"
#include "AgentDocs/Recipes/Code/TuningValue.h"
#include "Graphics/Surface/Surface.h"
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Modules/Persistence/Persistence.h"
#include "Options/GameOptions.h"
#include "Simulation/ReplayCheck.h"
#include "Simulation/StateHash.h"
#include "Tests/PaintHarness.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Engine.Recipes.MenuNavigatesAndConfirms", "[engine][recipes]") {
    RecipeMenu menu;
    DeviceInput down;
    down.keysPressed[static_cast<std::size_t>(InputKey::Down)] = true;
    CHECK_FALSE(menu.Navigate(down));
    CHECK(menu.Selected() == RecipeMenuItem::Quit);
    DeviceInput confirm;
    confirm.keysPressed[static_cast<std::size_t>(InputKey::Enter)] = true;
    CHECK(menu.Navigate(confirm));
    menu.Update(0.1f);
}

TEST_CASE("Engine.Recipes.SimulationPlaysInASceneAndWithoutOne", "[engine][recipes]") {
    TimeScale time;
    RecipePlayScene scene(time, 1);
    scene.Activate();
    DeviceInput right;
    right.keysDown[static_cast<std::size_t>(InputKey::D)] = true;
    for (int frame = 0; frame < 60; ++frame) {
        scene.ReadInput(right);
        scene.Tick(time.Advance(1.0f / 60.0f));
    }
    CHECK(scene.Runner().StepIndex() == 60);
    CHECK(scene.Sim().ShipX(1.0f) > 230.0f);

    const auto definition = RecipePlayDefinition();
    const auto replay = VerifyTape(definition, scene.Runner().Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hash == HashState(scene.Sim()));
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

TEST_CASE("Engine.Recipes.ViewsRecordAndCachedLayerInvalidates", "[engine][recipes]") {
    PaintHarness harness;
    RecipeCharacterView character;
    character.Pose({12.0f, 0.0f}, 0.1f);
    Surface layer({320.0f, 240.0f});
    auto fx = MakeRecipeFx({0.0f, 0.0f, 400.0f, 200.0f});
    TriggerRecipeFx(fx, {160.0f, 100.0f});
    fx.Update(0.01f);
    harness.Begin();
    CHECK(harness.paint.Stale(layer));
    DrawRecipeLayer(harness.paint, layer, harness.paint.Bounds(), 0.5f);
    character.Draw(harness.paint);
    DrawRecipeIcons(harness.paint, {200.0f, 100.0f}, 16.0f, Rgb(0xffffff));
    DrawGoblin(harness.paint, {240.0f, 20.0f, 60.0f, 100.0f}, GoblinKind::Forest, 0.5f);
    DrawGoblin(harness.paint, {240.0f, 120.0f, 100.0f, 60.0f}, GoblinKind::Ice, 0.5f);
    DrawRecipeFx(harness.paint, fx, harness.paint.Bounds());
    harness.End();
    CHECK_FALSE(harness.Draws().empty());
    harness.Begin();
    CHECK_FALSE(harness.paint.Stale(layer));
    harness.End();
    layer.Resize({160.0f, 120.0f});
    harness.Begin();
    CHECK(harness.paint.Stale(layer));
    harness.End();
    fx.Clear();
}

TEST_CASE("Engine.Recipes.EditorSceneAndTextWorkWithoutWindow", "[engine][recipes]") {
    RecipeScene scene;
    scene.Activate();
    scene.Tick(0.1f);
    scene.Deactivate();
    scene.Shutdown();
    CHECK(scene.IsShutdown());
    const auto definition = RecipeCharacterDefinition();
    REQUIRE(definition.create);
    auto editorScene = std::make_unique<RecipeCharacterScene>();
    ObjectList objects;
    objects.Add(*editorScene);
    objects.Update(0.1f);
    objects.Stop();
    RecipeTextView text(ENGINE_RECIPE_FONT);
    CHECK(text.Measured().x > 0.0f);
    CHECK(RecipeRadius() > 0.0f);
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
