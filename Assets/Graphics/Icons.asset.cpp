#include <array>
#include <string_view>
#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Icons/Icons.h"

namespace {
constexpr Color4 IconColor{235, 240, 255, 255};
constexpr Color4 IconDetail{70, 200, 255, 255};
constexpr float IconLineShare = 0.03f; // Of the box

// clang-format off
// In Icon's order.
constexpr std::array<std::string_view, IconCount> IconNames{
    "Star", "Egg", "Hourglass", "Plus", "Minus", "UpArrow", "Chevrons", "Apple", "Magnet", "Ghost",
    "Brick", "Rain", "Clock", "Book", "Shield", "HornedEye", "Sun", "Gem", "Bolt", "Rune",
    "Mountain", "Lock", "Coin", "Dice", "Cards", "LoopArrow", "Cloud", "StormCloud", "Tornado", "CrownedStar",
    "Compass", "Comet", "Gear", "Heart", "Power", "Lotus", "Goblet", "Stopwatch", "Sack", "Wings",
    "Flag", "Feather", "Chest", "Pickaxe", "Anchor", "Swords", "Coil", "Clover", "Swirl", "TurnArrow",
    "Copy", "Cat", "Couch", "Pedal", "Mask", "Hen", "Tire", "Bubble", "Scroll", "Flame"};
// clang-format on

GraphicAsset MakeIcons() {
    GraphicAsset asset;
    for (const auto name : IconNames)
        asset.items.push_back({std::string(name), "icon"});
    asset.usage = "DrawIcon(paint, Icon::{item}, center, size, style);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float) {
        DrawIcon(paint, static_cast<Icon>(item), box.Center(), box.width * 0.5f,
                 {IconColor, IconDetail, box.width * IconLineShare});
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeIcons);
} // namespace
