#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <fstream>
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/AssetNavigation.h"
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Tests/Support/TempDirectory.h"

namespace {
void Touch(const std::filesystem::path& file) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "x";
}
const AssetNode& Child(const AssetNode& node, const std::string& name) {
    const auto found = std::ranges::find(node.children, name, &AssetNode::name);
    REQUIRE(found != node.children.end());
    return *found;
}
GraphicAsset Drawable() {
    GraphicAsset asset;
    asset.draw = [](Painter&, const FloatRect&, std::size_t, float) {};
    return asset;
}
} // namespace

TEST_CASE("Editor.AssetLibrary.GroupsFilesByNameAndKind", "[engine-editor]") {
    TempDirectory temp;
    const auto assets = temp.Path() / "Game" / "Assets";
    Touch(assets / "Graphics" / "Enemies" / "Goblin.h");
    Touch(assets / "Graphics" / "Enemies" / "Goblin.cpp");
    Touch(assets / "Graphics" / "Enemies" / "Goblin.asset.cpp");
    Touch(assets / "Graphics" / "Enemies" / "Goblin.tuning.json");
    Touch(assets / "Graphics" / "Enemies" / "Helpers.h");
    Touch(assets / "Graphics" / ".gitkeep");
    Touch(assets / "Audio" / "Hit.ogg");
    Touch(assets / "Fonts" / "Main.ttf");
    Touch(assets / "Prefabs" / "Later.h");

    const auto roots = ScanAssetLibrary(temp.Path() / "Engine", assets);
    REQUIRE(roots.size() == 2);
    CHECK(roots[0].readOnly);
    CHECK(Child(roots[0], "Graphics").children.empty()); // Missing on disk: an empty, read-only folder
    const auto& game = roots[1];
    CHECK_FALSE(game.readOnly);
    CHECK(std::ranges::find(game.children, "Prefabs", &AssetNode::name) == game.children.end());
    const auto& enemies = Child(Child(game, "Graphics"), "Enemies");
    REQUIRE(enemies.children.size() == 2);
    const auto& goblin = Child(enemies, "Goblin");
    CHECK(goblin.kind == AssetKind::Graphic);
    CHECK(goblin.path.filename() == "Goblin.asset.cpp");
    CHECK(goblin.files.size() == 4);
    CHECK(Child(enemies, "Helpers").kind == AssetKind::Code);
    CHECK(Child(Child(game, "Audio"), "Hit").kind == AssetKind::Audio);
    CHECK(Child(Child(game, "Fonts"), "Main").kind == AssetKind::Font);
    CHECK(Child(game, "Graphics").children.size() == 1); // Dot files stay hidden
}

TEST_CASE("Editor.AssetLibrary.GraphicsFolderShowsBeforeItExists", "[engine-editor]") {
    TempDirectory temp;
    const auto roots = ScanAssetLibrary({}, temp.Path());
    REQUIRE(roots.size() == 1);
    CHECK(Child(roots[0], "Graphics").kind == AssetKind::Folder);
}

TEST_CASE("Editor.GraphicAsset.SingleDrawingAndSetIds", "[engine-editor]") {
    const std::filesystem::path engine = "C:/Project/Engine";
    auto single = Drawable();
    const auto one =
        GraphicAssetDefinitions(single, "C:/Project/Game/Assets/Graphics/Enemies/Goblin.asset.cpp", engine);
    REQUIRE(one.size() == 1);
    CHECK(one[0].id == "asset:Graphics/Enemies/Goblin");
    CHECK(one[0].name == "Goblin");
    REQUIRE(one[0].asset);
    CHECK(one[0].asset->file == "c:/project/game/assets/graphics/enemies/goblin.asset.cpp");
    CHECK(one[0].asset->item.empty());

    auto set = Drawable();
    set.items = {{"Star", "shape"}, {"Moon", ""}};
    set.usage = "Draw(Icon::{item});";
    const auto many = GraphicAssetDefinitions(set, "C:/Project/Engine/Assets/Graphics/Icons.asset.cpp", engine);
    REQUIRE(many.size() == 2);
    CHECK(many[0].id == "asset:Engine/Graphics/Icons#Star");
    CHECK(many[0].asset->usage == "Draw(Icon::Star);");
    CHECK(many[0].description.find("shape") != std::string::npos);
    CHECK(many[1].asset->item == "Moon");
}

TEST_CASE("Editor.GraphicAsset.RejectsMisplacedOrIncompleteAssets", "[engine-editor]") {
    const std::filesystem::path engine = "C:/Project/Engine";
    const std::filesystem::path good = "C:/Project/Game/Assets/Graphics/Goblin.asset.cpp";
    CHECK_THROWS(GraphicAssetDefinitions(GraphicAsset{}, good, engine));
    CHECK_THROWS(GraphicAssetDefinitions(Drawable(), "C:/Project/Game/Goblin.asset.cpp", engine));
    CHECK_THROWS(GraphicAssetDefinitions(Drawable(), "C:/Project/Game/Assets/Graphics/Goblin.cpp", engine));
    auto unnamed = Drawable();
    unnamed.items = {{"", ""}};
    CHECK_THROWS(GraphicAssetDefinitions(unnamed, good, engine));
}

TEST_CASE("Editor.GraphicAsset.CatalogKeepsAssetInfoOverTheProtocol", "[engine-editor]") {
    auto set = Drawable();
    set.items = {{"Star", ""}};
    set.usage = "Draw({item});";
    auto definitions =
        GraphicAssetDefinitions(set, "C:/Project/Game/Assets/Graphics/Icons.asset.cpp", "C:/Project/Engine");
    const auto decoded = DecodeSceneCatalog(EncodeSceneCatalog(definitions));
    REQUIRE(decoded.size() == 1);
    REQUIRE(decoded[0].asset);
    CHECK(*decoded[0].asset == *definitions[0].asset);
}

TEST_CASE("Editor.GraphicAsset.ThumbnailFilesAreSafeAndDistinct", "[engine-editor]") {
    const auto star = AssetThumbnailFile("asset:Engine/Graphics/Icons#Star");
    CHECK(star == AssetThumbnailFile("asset:Engine/Graphics/Icons#Star"));
    CHECK(star != AssetThumbnailFile("asset:Engine/Graphics/Icons#Moon"));
    CHECK(star.find_first_of(":/#") == std::string::npos);
    CHECK(star.ends_with(".png"));
}

TEST_CASE("Editor.AssetNavigation.BackUndoesBrowsingAndOpeningInOrder", "[engine-editor]") {
    AssetNavigation navigation;
    navigation.Correct({"graphics", {}});
    CHECK(navigation.Peek() == nullptr);
    CHECK_FALSE(navigation.Back());

    navigation.Go({"graphics", {}}, ""); // Same place: nothing to undo
    CHECK(navigation.Peek() == nullptr);

    navigation.SceneOpened("", "egg");
    navigation.Go({"graphics", "icons"}, "egg");
    navigation.SceneOpened("egg", "star");
    navigation.SceneOpened("star", "star"); // Same scene again: nothing to undo

    auto step = navigation.Back();
    REQUIRE(step);
    CHECK(step->scene == "egg");
    CHECK(navigation.Location().set == "icons");
    step = navigation.Back();
    REQUIRE(step);
    CHECK(step->scene == "egg");
    CHECK(navigation.Location().set.empty());
    step = navigation.Back();
    REQUIRE(step);
    CHECK(step->scene.empty());
    CHECK_FALSE(navigation.Back());
}
