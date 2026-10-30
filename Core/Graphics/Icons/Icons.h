#pragma once
#include <raylib.h>
#include <cstddef>

// Ortak ikon seti: buff, ödül kartı, wildcard ve HUD aynı şekilleri paylaşır. İkonlar oyun kavramıyla değil şekille
// adlandırılır (Hourglass, Magnet); hangi nesnenin hangi ikonu kullandığı o nesnenin View'ındaki eşlemededir.
// Dolu: Star, Egg, Hourglass, Plus, Minus, UpArrow, Chevrons, Apple, Magnet, Ghost, Brick, Rain, Cloud,
// CrownedStar (taçlı yıldız; taç 'detail' renginde), Heart, Clover (dört yapraklı yonca).
// Çizgi (+ yarı saydam dolgu): Clock, Book, Shield, HornedEye, Sun, Gem, Bolt, Rune, Mountain, Lock, Coin, Dice,
// Cards (yelpaze gibi açılmış üç kart), LoopArrow (kendi etrafında dönen ok), Lotus (nilüfer), Goblet (kadeh),
// Stopwatch, Sack (para kesesi), Wings, Flag (dama desenli bitiş bayrağı), Feather, Chest (hazine sandığı),
// Pickaxe, Anchor, Swords (çapraz iki kılıç), Coil (kıvrılmış yılan; gözü 'detail'), Swirl (girdap), TurnArrow
// (yukarı çıkıp sağa kıvrılan ok), Copy (üst üste iki sayfa).
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
    Color color;          // Şeklin rengi
    Color detail = BLANK; // İçindeki ayrıntı: hayalet gözü, tuğla çatlağı, yumurta parlaması
    float line = 2.0f;    // Çizgi ikonlarının kalınlığı
};

// 'size' yarı genişlik: ikon merkezden her yöne yaklaşık bu kadar uzanır.
void DrawIcon(Icon icon, Vector2 center, float size, const IconStyle& style);
