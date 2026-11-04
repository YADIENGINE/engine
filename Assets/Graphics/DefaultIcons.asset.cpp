#include <array>
#include <string>
#include <string_view>
#include <utility>
#include "Editor/Api/GraphicAsset.h"
#include "Graphics/DefaultIcons.h"

namespace {
struct IconItem {
    std::string_view name;
    std::string_view tags = {}; // Search words beyond the name
};

// clang-format off
// In Icon's order.
constexpr auto IconItems = std::to_array<IconItem>({
    {"Star"}, {"Egg"}, {"Hourglass"}, {"Plus"}, {"Minus"}, {"UpArrow"}, {"Chevrons"}, {"Apple"}, {"Magnet"}, {"Ghost"},
    {"Brick"}, {"Rain"}, {"Clock"}, {"Book"}, {"Shield"}, {"HornedEye"}, {"Sun"}, {"Gem"}, {"Bolt"}, {"Rune"},
    {"Mountain"}, {"Lock"}, {"Coin"}, {"Dice"}, {"Cards"}, {"LoopArrow"}, {"Cloud"}, {"StormCloud"}, {"Tornado"},
    {"CrownedStar"},
    {"Compass"}, {"Comet"}, {"Gear"}, {"Heart"}, {"Power"}, {"Lotus"}, {"Goblet"}, {"Stopwatch"}, {"Sack"}, {"Wings"},
    {"Flag"}, {"Feather"}, {"Chest"}, {"Pickaxe"}, {"Anchor"}, {"Swords"}, {"Coil"}, {"Clover"}, {"Swirl"},
    {"TurnArrow"},
    {"Copy"}, {"Cat"}, {"Couch"}, {"Pedal"}, {"Mask"}, {"Hen"}, {"Tire"}, {"Bubble"}, {"Scroll"}, {"Flame"},
    {"Play", "ui start resume triangle"},
    {"Pause", "ui stop hold bars"},
    {"Restart", "ui retry reset again reload circular arrow"},
    {"Check", "ui yes ok confirm done tick correct"},
    {"Cross", "ui no cancel close wrong remove x"},
    {"Home", "ui house menu main"},
    {"Info", "ui help about information"},
    {"Warning", "ui alert danger caution exclamation triangle"},
    {"SoundOn", "ui audio volume speaker unmute"},
    {"SoundOff", "ui audio volume speaker mute silent"},
    {"Music", "ui audio note song"},
    {"Save", "ui disk floppy store"},
    {"Trash", "ui delete remove bin"},
    {"Sword", "item weapon blade attack melee"},
    {"Bow", "item weapon arrow ranged"},
    {"Potion", "item flask bottle health drink"},
    {"Bomb", "item explosive fuse"},
    {"Skull", "death danger enemy bone"},
    {"Key", "item unlock open door"},
    {"Crown", "item king royal winner"},
    {"Trophy", "item cup win prize award"},
    {"Moon", "night crescent sleep"},
    {"Droplet", "water drop liquid"},
    {"Snowflake", "ice cold winter freeze"},
    {"Leaf", "plant nature green"},
    {"Eye", "see look vision visible"},
    {"Target", "aim bullseye goal rings"},
});
// clang-format on
static_assert(IconItems.size() == IconCount);

GraphicAsset MakeIcons() {
    GraphicAsset asset;
    for (const IconItem& item : IconItems) {
        std::string tags = "icon";
        if (!item.tags.empty()) tags.append(" ").append(item.tags);
        asset.items.push_back({std::string(item.name), std::move(tags)});
    }
    asset.usage = "DrawIcon(paint, Icon::{item}, center, size);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float) {
        DrawIcon(paint, static_cast<Icon>(item), box.Center(), box.width * 0.5f);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeIcons);
} // namespace
