#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include "Ui/Kernel/Internals/UiFocusSearch.h"
#include "Ui/Layout/Internals/UiLayoutSolver.h"

using Catch::Approx;

namespace {
// Builds a tree in the order the UI does: parents before children.
class Tree {
public:
    int Add(int parent, const UiBox& box, const UiMeasure* measure = nullptr) {
        const int index = static_cast<int>(nodes.size());
        UiLayoutNode& node = nodes.emplace_back();
        node.box = box;
        node.parent = parent;
        node.measure = measure;
        if (parent >= 0) {
            UiLayoutNode& up = nodes[static_cast<std::size_t>(parent)];
            if (up.lastChild >= 0) {
                nodes[static_cast<std::size_t>(up.lastChild)].nextSibling = index;
            } else {
                up.firstChild = index;
            }
            up.lastChild = index;
        }
        return index;
    }
    void Solve(const FloatRect& canvas = {0.0f, 0.0f, 400.0f, 300.0f}) { SolveUiLayout(nodes, canvas); }
    [[nodiscard]] const FloatRect& Rect(int index) const { return nodes[static_cast<std::size_t>(index)].rect; }

    std::vector<UiLayoutNode> nodes;
};
void CheckRect(const FloatRect& rect, float x, float y, float width, float height) {
    CHECK(rect.x == Approx(x));
    CHECK(rect.y == Approx(y));
    CHECK(rect.width == Approx(width));
    CHECK(rect.height == Approx(height));
}
} // namespace

TEST_CASE("Engine.Ui.Layout.RootTakesTheCanvas", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {});
    tree.Solve({10.0f, 20.0f, 100.0f, 50.0f});
    CheckRect(tree.Rect(root), 10.0f, 20.0f, 100.0f, 50.0f);
}

TEST_CASE("Engine.Ui.Layout.ColumnStacksWithGapAndPadding", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {.gap = 10.0f, .padding = UiInsets::All(5.0f)});
    const int first = tree.Add(root, {.width = UiFixed(50.0f), .height = UiFixed(20.0f)});
    const int second = tree.Add(root, {.width = UiFixed(30.0f), .height = UiFixed(40.0f)});
    tree.Solve();
    CheckRect(tree.Rect(first), 5.0f, 5.0f, 50.0f, 20.0f);
    CheckRect(tree.Rect(second), 5.0f, 35.0f, 30.0f, 40.0f);
}

TEST_CASE("Engine.Ui.Layout.FitWrapsItsChildren", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {});
    const int panel = tree.Add(root, {.direction = UiDirection::Row, .gap = 4.0f, .padding = UiInsets::All(2.0f)});
    tree.Add(panel, {.width = UiFixed(10.0f), .height = UiFixed(30.0f)});
    tree.Add(panel, {.width = UiFixed(20.0f), .height = UiFixed(10.0f)});
    tree.Solve();
    CheckRect(tree.Rect(panel), 0.0f, 0.0f, 38.0f, 34.0f);
}

TEST_CASE("Engine.Ui.Layout.FillSharesWhatIsLeftByWeight", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {.direction = UiDirection::Row, .gap = 10.0f});
    const int fixed = tree.Add(root, {.width = UiFixed(90.0f), .height = UiFixed(10.0f)});
    const int one = tree.Add(root, {.width = UiFill(1.0f), .height = UiFill()});
    const int three = tree.Add(root, {.width = UiFill(3.0f), .height = UiFixed(10.0f)});
    tree.Solve();
    CheckRect(tree.Rect(fixed), 0.0f, 0.0f, 90.0f, 10.0f);
    CheckRect(tree.Rect(one), 100.0f, 0.0f, 72.5f, 300.0f);
    CheckRect(tree.Rect(three), 182.5f, 0.0f, 217.5f, 10.0f);
}

TEST_CASE("Engine.Ui.Layout.MinAndMaxBound", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {});
    const int wide = tree.Add(root, {.width = UiFill(1.0f, 0.0f, 120.0f), .height = UiFit(25.0f)});
    tree.Solve();
    CHECK(tree.Rect(wide).width == Approx(120.0f));
    CHECK(tree.Rect(wide).height == Approx(25.0f));
}

TEST_CASE("Engine.Ui.Layout.AlignmentCentersAndEnds", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {.alignX = UiAlign::Center, .alignY = UiAlign::End});
    const int item = tree.Add(root, {.width = UiFixed(100.0f), .height = UiFixed(50.0f)});
    tree.Solve();
    CheckRect(tree.Rect(item), 150.0f, 250.0f, 100.0f, 50.0f);
}

TEST_CASE("Engine.Ui.Layout.OverlayStacksInOnePlace", "[engine][ui]") {
    Tree tree;
    const int root =
        tree.Add(-1, {.direction = UiDirection::Overlay, .alignX = UiAlign::Center, .alignY = UiAlign::Center});
    const int under = tree.Add(root, {.width = UiFixed(100.0f), .height = UiFixed(100.0f)});
    const int over = tree.Add(root, {.width = UiFixed(20.0f), .height = UiFixed(20.0f)});
    tree.Solve();
    CheckRect(tree.Rect(under), 150.0f, 100.0f, 100.0f, 100.0f);
    CheckRect(tree.Rect(over), 190.0f, 140.0f, 20.0f, 20.0f);
}

TEST_CASE("Engine.Ui.Layout.AnchoredLeavesTheFlow", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {.gap = 10.0f});
    const int first = tree.Add(root, {.width = UiFixed(50.0f), .height = UiFixed(50.0f)});
    const int corner = tree.Add(root, {.width = UiFixed(20.0f),
                                       .height = UiFixed(10.0f),
                                       .anchor = UiAnchor::BottomRight,
                                       .offset = {-4.0f, -2.0f}});
    const int second = tree.Add(root, {.width = UiFixed(50.0f), .height = UiFixed(50.0f)});
    tree.Solve();
    CheckRect(tree.Rect(first), 0.0f, 0.0f, 50.0f, 50.0f);
    CheckRect(tree.Rect(corner), 376.0f, 288.0f, 20.0f, 10.0f);
    CheckRect(tree.Rect(second), 0.0f, 60.0f, 50.0f, 50.0f);
}

TEST_CASE("Engine.Ui.Layout.EveryAnchorReachesItsPlace", "[engine][ui]") {
    Tree tree;
    const int root = tree.Add(-1, {});
    const int center = tree.Add(root, {.width = UiFixed(10.0f), .height = UiFixed(10.0f), .anchor = UiAnchor::Center});
    const int right = tree.Add(root, {.width = UiFixed(10.0f), .height = UiFixed(10.0f), .anchor = UiAnchor::Right});
    const int top = tree.Add(root, {.width = UiFixed(10.0f), .height = UiFixed(10.0f), .anchor = UiAnchor::Top});
    const int low = tree.Add(root, {.width = UiFixed(10.0f), .height = UiFixed(10.0f), .anchor = UiAnchor::BottomLeft});
    tree.Solve();
    CheckRect(tree.Rect(center), 195.0f, 145.0f, 10.0f, 10.0f);
    CheckRect(tree.Rect(right), 390.0f, 145.0f, 10.0f, 10.0f);
    CheckRect(tree.Rect(top), 195.0f, 0.0f, 10.0f, 10.0f);
    CheckRect(tree.Rect(low), 0.0f, 290.0f, 10.0f, 10.0f);
}

TEST_CASE("Engine.Ui.Layout.HeightFollowsTheWidthATextGets", "[engine][ui]") {
    // A text of 300 units that wraps: its height is known only after its width.
    const UiMeasure wrapping = [](float maxWidth) {
        if (!(maxWidth < UiUnlimited)) return Float2{300.0f, 20.0f};
        const float lines = std::ceil(300.0f / maxWidth);
        return Float2{std::min(maxWidth, 300.0f), lines * 20.0f};
    };
    Tree tree;
    const int root = tree.Add(-1, {});
    const int panel = tree.Add(root, {.width = UiFixed(110.0f), .padding = UiInsets::All(5.0f)});
    const int text = tree.Add(panel, {}, &wrapping);
    tree.Solve();
    CHECK(tree.Rect(text).width == Approx(100.0f));
    CHECK(tree.Rect(text).height == Approx(60.0f));
    CHECK(tree.Rect(panel).height == Approx(70.0f));
}

TEST_CASE("Engine.Ui.Focus.MovesToWhatIsAhead", "[engine][ui]") {
    const std::vector<FloatRect> list{
        {0.0f, 0.0f, 100.0f, 20.0f}, {0.0f, 30.0f, 100.0f, 20.0f}, {0.0f, 60.0f, 100.0f, 20.0f}};
    CHECK(FindUiFocusTarget(list, list[0], UiMove::Down, false) == std::size_t{1});
    CHECK(FindUiFocusTarget(list, list[2], UiMove::Up, false) == std::size_t{1});
    CHECK_FALSE(FindUiFocusTarget(list, list[2], UiMove::Down, false));
}

TEST_CASE("Engine.Ui.Focus.WrapsToTheFarSideOfTheSameColumn", "[engine][ui]") {
    const std::vector<FloatRect> grid{{0.0f, 0.0f, 40.0f, 40.0f},   {50.0f, 0.0f, 40.0f, 40.0f},
                                      {0.0f, 50.0f, 40.0f, 40.0f},  {50.0f, 50.0f, 40.0f, 40.0f},
                                      {0.0f, 100.0f, 40.0f, 40.0f}, {50.0f, 100.0f, 40.0f, 40.0f}};
    CHECK(FindUiFocusTarget(grid, grid[5], UiMove::Down, true) == std::size_t{1});
    CHECK(FindUiFocusTarget(grid, grid[0], UiMove::Left, true) == std::size_t{1});
    CHECK(FindUiFocusTarget(grid, grid[2], UiMove::Right, true) == std::size_t{3});
}

TEST_CASE("Engine.Ui.Focus.PrefersItsOwnColumnToANearerOne", "[engine][ui]") {
    // Below, slightly nearer but off to the side, against straight below and farther.
    const FloatRect from{0.0f, 0.0f, 100.0f, 20.0f};
    const std::vector<FloatRect> options{{300.0f, 25.0f, 100.0f, 20.0f}, {0.0f, 60.0f, 100.0f, 20.0f}};
    CHECK(FindUiFocusTarget(options, from, UiMove::Down, false) == std::size_t{1});
}

TEST_CASE("Engine.Ui.Focus.NothingToTheSideOfASingleRow", "[engine][ui]") {
    const std::vector<FloatRect> row{{0.0f, 0.0f, 40.0f, 40.0f}, {50.0f, 0.0f, 40.0f, 40.0f}};
    CHECK_FALSE(FindUiFocusTarget(row, row[0], UiMove::Down, true));
}

TEST_CASE("Engine.Ui.Focus.NearestFindsWhereACursorGoes", "[engine][ui]") {
    const std::vector<FloatRect> items{{0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 100.0f, 10.0f, 10.0f}};
    CHECK(FindNearestUiFocus(items, {0.0f, 80.0f, 10.0f, 10.0f}) == std::size_t{1});
    CHECK_FALSE(FindNearestUiFocus({}, {0.0f, 80.0f, 10.0f, 10.0f}));
}
