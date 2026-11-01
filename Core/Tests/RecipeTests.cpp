#include <catch2/catch_test_macros.hpp>
#include "AgentDocs/Recipes/Code/AnimatedCharacter.h"
#include "AgentDocs/Recipes/Code/CharacterScene.h"
#include "AgentDocs/Recipes/Code/FxBurst.h"
#include "AgentDocs/Recipes/Code/IconShape.h"
#include "AgentDocs/Recipes/Code/SaveProgress.h"
#include "AgentDocs/Recipes/Code/SceneTransition.h"
#include "AgentDocs/Recipes/Code/SpriteLayer.h"
#include "AgentDocs/Recipes/Code/TextLayout.h"
#include "AgentDocs/Recipes/Code/ThemedMenu.h"
#include "AgentDocs/Recipes/Code/TuningValue.h"
#include "Graphics/Surface/Surface.h"
#include "Input/Input.h"
#include "Modules/Persistence/Persistence.h"
#include "Options/GameOptions.h"
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
