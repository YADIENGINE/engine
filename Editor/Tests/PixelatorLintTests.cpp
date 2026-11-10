#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorLint.h"
#include "Editor/Internals/Protocol/PixelatorProtocol.h"

// Pixel-art lint (D65): each rule on small hand-made sprites, with what it must find and what it must leave alone.
namespace {
// Inks: 1 outline (near black), 2 fill, 3 a tone almost the same as 2, 4 a bright highlight, 5 a darker fill, 6 unused.
const std::vector<Color4> Colors{{0, 0, 0, 0},       {20, 18, 28, 255},    {100, 110, 130, 255}, {103, 112, 133, 255},
                                 {240, 236, 220, 255}, {60, 66, 80, 255},  {200, 40, 40, 255}};

// '.' is ink 0, digits are inks.
PixelatorImage Image(std::initializer_list<std::string_view> rows) {
    PixelatorImage image;
    image.height = static_cast<int>(rows.size());
    image.width = static_cast<int>(rows.begin()->size());
    for (const auto row : rows)
        for (const char c : row) {
            image.inks.push_back(c == '.' ? Ink{0} : static_cast<Ink>(c - '0'));
            image.parts.push_back(0);
        }
    return image;
}

PixelatorAsset Asset(std::vector<PixelatorImage> images, std::vector<Color4> palette = Colors) {
    PixelatorAsset asset;
    asset.id = "asset:Graphics/Test";
    asset.palettes = {std::move(palette)};
    for (std::size_t index = 0; index < images.size(); ++index)
        asset.frames.push_back({index, 0, 0.1f});
    asset.images = std::move(images);
    return asset;
}

std::vector<PixelatorFinding> Rule(const std::vector<PixelatorFinding>& findings, std::string_view rule) {
    std::vector<PixelatorFinding> found;
    for (const auto& finding : findings)
        if (finding.rule == rule) found.push_back(finding);
    return found;
}

// A 9 x 9 rounded blob of fill 2 ringed by outline 1, as PixelCanvas::Outline draws it.
std::vector<std::string> Blob() {
    return {".........", "..11111..", ".1222221.", ".1222221.", ".1222221.",
            ".1222221.", ".1222221.", "..11111..", "........."};
}
PixelatorImage ImageOf(const std::vector<std::string>& rows) {
    PixelatorImage image;
    image.height = static_cast<int>(rows.size());
    image.width = static_cast<int>(rows[0].size());
    for (const auto& row : rows)
        for (const char c : row) {
            image.inks.push_back(c == '.' ? Ink{0} : static_cast<Ink>(c - '0'));
            image.parts.push_back(0);
        }
    return image;
}
} // namespace

TEST_CASE("Editor.PixelLint.StrayPixelsButNotDetailsOrTextures", "[engine-editor]") {
    // A low-contrast lone pixel in a flat area is noise.
    auto findings = LintPixelator(Asset({Image({"22222", "22222", "22322", "22222", "22222"})}));
    auto stray = Rule(findings, "stray-pixel");
    REQUIRE(stray.size() == 1);
    CHECK(stray[0].pixels == std::vector<Vec2>{{2, 2}});
    CHECK(stray[0].inks == std::vector<int>{3, 2});
    CHECK(stray[0].severity == PixelatorLintSeverity::Warning);
    CHECK(stray[0].frames == std::vector<std::size_t>{0});
    // A bright one is a deliberate detail (an eye glint, a star).
    CHECK(Rule(LintPixelator(Asset({Image({"22222", "22222", "22422", "22222", "22222"})})), "stray-pixel").empty());
    // Many of the same kind are a texture (speckle).
    CHECK(Rule(LintPixelator(Asset({Image({"2222222", "2322232", "2222222", "2232222", "2222222"})})), "stray-pixel")
              .empty());
    // Dithering: every pixel has its own ink diagonally.
    CHECK(Rule(LintPixelator(Asset({Image({"232323", "323232", "232323", "323232"})})), "stray-pixel").empty());
}

TEST_CASE("Editor.PixelLint.OutlineDoublesOnlyWhereTheLineThickens", "[engine-editor]") {
    // The kit's outline around a shape has no doubles, square corners included.
    auto blob = Blob();
    CHECK(Rule(LintPixelator(Asset({ImageOf(blob)})), "outline-double").empty());
    blob[1] = ".1111111.";
    CHECK(Rule(LintPixelator(Asset({ImageOf(blob)})), "outline-double").empty()); // Corners wrap the fill
    // A stair of L-shaped steps in the outline ink (a hand-drawn edge): each step thickens the line once.
    const std::vector<std::string> stair{"1......", "11.....", ".11....", "..11...", "...11..", "....11.", ".....11"};
    const auto doubles = Rule(LintPixelator(Asset({ImageOf(stair)})), "outline-double");
    REQUIRE(doubles.size() == 6);
    CHECK(doubles[0].pixels == std::vector<Vec2>{{0, 1}});
    CHECK(doubles[1].pixels == std::vector<Vec2>{{1, 2}});
    // The same edge drawn as a clean diagonal has none.
    const std::vector<std::string> clean{"1......", ".1.....", "..1....", "...1...", "....1..", ".....1.", "......1",
                                         "......1", ".....1.", "....1..", "...1...", "..1....", ".1.....", "1......"};
    CHECK(Rule(LintPixelator(Asset({ImageOf(clean)})), "outline-double").empty());
    PixelatorLintOptions capped;
    capped.maxPerRule = 1;
    const auto one = Rule(LintPixelator(Asset({ImageOf(stair)}), capped), "outline-double");
    REQUIRE(one.size() == 1);
    CHECK(one[0].text.ends_with("(and 5 more like it)"));
}

TEST_CASE("Editor.PixelLint.OutlineGapsButNotParticles", "[engine-editor]") {
    auto blob = Blob();
    CHECK(Rule(LintPixelator(Asset({ImageOf(blob)})), "outline-gap").empty());
    blob[3] = ".2222221."; // The fill reaches the outside on the left
    blob[0] = "4........"; // A loose spark needs no outline
    const auto gaps = Rule(LintPixelator(Asset({ImageOf(blob)})), "outline-gap");
    REQUIRE(gaps.size() == 1);
    CHECK(gaps[0].pixels == std::vector<Vec2>{{1, 3}});
    CHECK(gaps[0].inks == std::vector<int>{2});
    // No outline at all: nothing to judge.
    CHECK(Rule(LintPixelator(Asset({Image({"22222", "22222", "22222", "22222", "22222"})})), "outline-gap").empty());
}

TEST_CASE("Editor.PixelLint.RampStepsByOklabLightness", "[engine-editor]") {
    // Registered ramps: 2 and 3 are nearly the same lightness; 4 then 5 goes darker.
    auto asset = Asset({Image({"25522", "23344", "23344"})});
    asset.ramps = {{5, 5, 2, 3, 4}, {2, 2, 4, 5, 5}};
    asset.inkNames = {"", "Line", "Steel1", "Steel2", "Shine", "Steel0"};
    const auto ramps = Rule(LintPixelator(asset), "ramp-contrast");
    REQUIRE(ramps.size() == 2);
    CHECK(ramps[0].inks == std::vector<int>{2, 3});
    CHECK(ramps[0].text.find("2 Steel1") != std::string::npos);
    CHECK(ramps[1].inks == std::vector<int>{4, 5});
    CHECK(ramps[1].text.starts_with("Ramp out of order"));
    // Without ramps: touching inks that look the same.
    asset.ramps.clear();
    const auto similar = Rule(
        LintPixelator(Asset({Image({"222333", "222333", "222333", "222333", "222333", "222333"})})), "ramp-contrast");
    REQUIRE(similar.size() == 1);
    CHECK(similar[0].inks == std::vector<int>{2, 3});
    CHECK(Rule(LintPixelator(Asset({Image({"222444", "222444", "222444", "222444", "222444", "222444"})})),
               "ramp-contrast")
              .empty());
}

TEST_CASE("Editor.PixelLint.UnusedInksAndColourCount", "[engine-editor]") {
    auto palette = Colors;
    palette.push_back({0, 0, 0, 0}); // A transparent slot is not "unused"
    const auto findings = LintPixelator(Asset({Image({"1245", "1245"})}, palette));
    const auto unused = Rule(findings, "unused-ink");
    REQUIRE(unused.size() == 1);
    CHECK(unused[0].inks == std::vector<int>{3, 6});
    CHECK(unused[0].severity == PixelatorLintSeverity::Info);
    CHECK_FALSE(unused[0].image);

    CHECK(PixelatorColorBudget(16, 12) == 8);
    CHECK(PixelatorColorBudget(20, 32) == 16);
    CHECK(PixelatorColorBudget(112, 92) == 32);
    CHECK(PixelatorColorBudget(640, 360) == 48);
    std::vector<Color4> many{{0, 0, 0, 0}};
    for (int ink = 1; ink <= 9; ++ink)
        many.push_back({static_cast<std::uint8_t>(ink * 25), 0, 0, 255});
    CHECK(Rule(LintPixelator(Asset({Image({"123456789"})}, many)), "color-count").size() == 1);
    CHECK(Rule(LintPixelator(Asset({Image({"12345678."})}, many)), "color-count").empty());
}

TEST_CASE("Editor.PixelLint.ReportAsJson", "[engine-editor]") {
    auto asset = Asset({Image({"22222", "22222", "22322", "22222", "22222"})});
    asset.items = {{"Idle", "asset:Graphics/Test#Idle"}};
    const auto report = PixelatorLintReport(asset);
    CHECK(report.at("asset") == "asset:Graphics/Test");
    CHECK(report.at("item") == "Idle");
    CHECK(report.at("warnings") == 1);
    CHECK(report.at("findings")[0].at("rule") == "stray-pixel");
    CHECK(report.at("findings")[0].at("pixels") == nlohmann::json::array({nlohmann::json::array({2, 2})}));
    CHECK(PixelatorLintReport(PixelatorAsset{.id = "asset:Graphics/Rock", .error = "broken"}).at("error") == "broken");
    CHECK(LintPixelator(PixelatorAsset{}).empty());
}
