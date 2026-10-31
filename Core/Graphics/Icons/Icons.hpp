#pragma once
#include <cstddef>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

class Painter;

// The shared icon set: buffs, reward cards, wildcards and the HUD use the same shapes. Icons are named after their
// shape, not a game concept (Hourglass, Magnet); which object uses which icon is mapped in that object's view.
// Solid: Star, Egg, Hourglass, Plus, Minus, UpArrow, Chevrons, Apple, Magnet, Ghost, Brick, Rain, Cloud,
// CrownedStar (crown in the 'detail' colour), Heart, Clover (four-leaf).
// Line (+ translucent fill): Clock, Book, Shield, HornedEye, Sun, Gem, Bolt, Rune, Mountain, Lock, Coin, Dice,
// Cards (three cards fanned out), LoopArrow (arrow turning around itself), Lotus, Goblet, Stopwatch, Sack (money bag),
// Wings, Flag (chequered finish flag), Feather, Chest (treasure chest), Pickaxe, Anchor, Swords (two crossed), Coil
// (coiled snake; eye in 'detail'), Swirl, TurnArrow (up, then curling right), Copy (two stacked pages).
enum class Icon {
    Star,
    Egg,
    Hourglass,
    Plus,
    Minus,
    UpArrow,
    Chevrons,
    Apple,
    Magnet,
    Ghost,
    Brick,
    Rain,
    Clock,
    Book,
    Shield,
    HornedEye,
    Sun,
    Gem,
    Bolt,
    Rune,
    Mountain,
    Lock,
    Coin,
    Dice,
    Cards,
    LoopArrow,
    Cloud,
    StormCloud,
    Tornado,
    CrownedStar,
    Compass,
    Comet,
    Gear,
    Heart,
    Power,
    Lotus,
    Goblet,
    Stopwatch,
    Sack,
    Wings,
    Flag,
    Feather,
    Chest,
    Pickaxe,
    Anchor,
    Swords,
    Coil,
    Clover,
    Swirl,
    TurnArrow,
    Copy,
    Cat,
    Couch,
    Pedal,
    Mask,
    Hen,
    Tire,
    Bubble,
    Scroll,
    Flame,
    Count
};
inline constexpr std::size_t IconCount = static_cast<std::size_t>(Icon::Count);

struct IconStyle {
    Color4 color;              // The shape's colour
    Color4 detail{0, 0, 0, 0}; // A detail inside it: ghost eyes, brick crack, egg shine
    float line = 2.0f;         // Stroke width of line icons
};

// 'size' is half the width: the icon reaches about this far from its centre in every direction.
void DrawIcon(Painter& paint, Icon icon, Float2 center, float size, const IconStyle& style);
