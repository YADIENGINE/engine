#include "Tests/Support/Diagnostics.h"
#include "Menus/GridMenuSelection.h"
#include "Menus/HeaderGridNavigation.h"

TEST_CASE("Engine.GridMenu.ClampsAndCrossesRowsWithHorizontalPriority", "[engine][menu]") {
    GridMenuSelection menu(10);
    CHECK(menu.ItemCount() == 10);
    CHECK(menu.Selected() == 0);
    CHECK(menu.Focus() == GridMenuFocus::Grid);
    CHECK(menu.Move(-1, 1, 4) == GridMenuMove::Unchanged);
    CHECK(menu.Selected() == 0);
    menu.Select(3);
    CHECK(menu.Move(1, 1, 4) == GridMenuMove::Changed);
    CHECK(menu.Selected() == 4);
    menu.Select(100);
    CHECK(menu.Selected() == 9);
    CHECK(menu.Move(1, 0, 4) == GridMenuMove::Unchanged);
    menu.Select(-100);
    CHECK(menu.Selected() == 0);
}

TEST_CASE("Engine.GridMenu.ClampsIncompleteRowsAndRestoresFooterSelection", "[engine][menu]") {
    GridMenuSelection menu(10);
    menu.Select(7);
    REQUIRE(menu.Move(0, 1, 4) == GridMenuMove::Changed);
    CHECK(menu.Selected() == 9);
    REQUIRE(menu.Move(0, 1, 4) == GridMenuMove::Changed);
    CHECK(menu.Focus() == GridMenuFocus::Footer);
    CHECK(menu.Move(1, 0, 4) == GridMenuMove::Unchanged);
    CHECK(menu.Move(0, 1, 4) == GridMenuMove::Unchanged);
    REQUIRE(menu.Move(0, -1, 4) == GridMenuMove::Changed);
    CHECK(menu.Focus() == GridMenuFocus::Grid);
    CHECK(menu.Selected() == 9);
    REQUIRE(menu.Move(0, -1, 4) == GridMenuMove::Changed);
    CHECK(menu.Selected() == 5);
    menu.Focus(GridMenuFocus::Footer);
    menu.Select(2);
    CHECK(menu.Focus() == GridMenuFocus::Grid);
}

TEST_CASE("Engine.GridMenu.ReportsTopExitAndRejectsInvalidColumns", "[engine][menu]") {
    GridMenuSelection menu(3);
    menu.Select(2);
    CHECK(menu.Move(0, -1, 4) == GridMenuMove::ExitAbove);
    CHECK(menu.Selected() == 2);
    CHECK(menu.Focus() == GridMenuFocus::Grid);
    CHECK(menu.Move(0, 1, 0) == GridMenuMove::Unchanged);
    CHECK(menu.Move(-1, 0, -1) == GridMenuMove::Unchanged);
    menu.Focus(GridMenuFocus::Footer);
    CHECK(menu.Move(0, -1, 0) == GridMenuMove::Unchanged);
    CHECK(menu.Focus() == GridMenuFocus::Footer);
}

TEST_CASE("Engine.GridMenu.ResizesAndHandlesEmptyGrids", "[engine][menu]") {
    GridMenuSelection menu(10);
    menu.Select(9);
    menu.SetItemCount(3);
    CHECK(menu.Selected() == 2);
    menu.SetItemCount(7);
    CHECK(menu.Selected() == 2);
    menu.SetItemCount(0);
    CHECK(menu.ItemCount() == 0);
    CHECK(menu.Selected() == -1);
    CHECK(menu.Focus() == GridMenuFocus::Footer);
    menu.Focus(GridMenuFocus::Grid);
    menu.Select(0);
    CHECK(menu.Focus() == GridMenuFocus::Footer);
    CHECK(menu.Selected() == -1);
    CHECK(menu.Move(0, -1, 3) == GridMenuMove::Unchanged);
    menu.SetItemCount(5);
    CHECK(menu.Selected() == 0);
    menu.Select(1);
    CHECK(menu.Focus() == GridMenuFocus::Grid);
    menu.SetItemCount(-1);
    CHECK(menu.ItemCount() == 0);
    CHECK(menu.Selected() == -1);
    GridMenuSelection empty(0);
    CHECK(empty.Selected() == -1);
    CHECK(empty.Focus() == GridMenuFocus::Footer);
}

TEST_CASE("Engine.HeaderGridNavigation.MovesThroughHeadersAndResetsCursors", "[engine][menu]") {
    HeaderGridNavigation navigation(6, {3, 4});
    navigation.Focus(HeaderGridFocus::Header, 1, 2);
    CHECK(navigation.HeaderRow() == 1);
    CHECK(navigation.HeaderIndex(1) == 2);
    CHECK(navigation.Move(1, 0, 3));
    CHECK(navigation.HeaderIndex(1) == 3);
    CHECK_FALSE(navigation.Move(1, 0, 3));
    CHECK(navigation.Move(0, -1, 3));
    CHECK(navigation.HeaderRow() == 0);
    navigation.Focus(HeaderGridFocus::Header, 0, 1);
    navigation.ResetHeaders();
    CHECK(navigation.HeaderIndex(0) == 0);
    CHECK(navigation.HeaderIndex(1) == 0);
    CHECK(navigation.Focus() == HeaderGridFocus::Header);
    CHECK(navigation.Move(0, 1, 3));
    CHECK(navigation.Move(0, 1, 3));
    CHECK(navigation.Focus() == HeaderGridFocus::Grid);
    navigation.SetItemCount(0);
    CHECK(navigation.Focus() == HeaderGridFocus::Footer);
    CHECK(navigation.Move(0, -1, 3));
    CHECK(navigation.Focus() == HeaderGridFocus::Empty);
}
