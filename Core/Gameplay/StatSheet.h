#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

// Player / unit stats: each value is (base + sum of Add + instant bonus) × product of Multiply. The game supplies
// stat IDs through its enum (Id::Count last) and a small rules type; the engine does not know game stats. Header-only.
//
//   struct MyStatRules {
//       static float Base(MyStat stat);                // Base value without modifiers
//       static float Finalize(MyStat stat, float v);   // Final clamp (e.g. chance 0..1); 'v' if unnecessary
//   };
//   using MyStats = StatSheet<MyStat, MyStatRules>;

// Add: added to the base. Multiply: multiplies the sum.
enum class ModOp { Add, Multiply };

// One effect modifying a stat (wildcard, card, buff ...).
template <class Id> struct StatModifierOf {
    Id stat;
    ModOp op;
    float value;
    bool operator==(const StatModifierOf&) const = default;
};

template <class Id, class Rules> class StatSheet {
public:
    using Modifier = StatModifierOf<Id>;

    StatSheet() { Clear(); }

    // Removes all modifiers, restoring stats to their base values.
    void Clear() {
        _add.fill(0.0f);
        _multiply.fill(1.0f);
    }
    void Apply(std::span<const Modifier> modifiers) {
        for (const Modifier& modifier : modifiers) {
            switch (modifier.op) {
            case ModOp::Add     : _add[Index(modifier.stat)] += modifier.value; break;
            case ModOp::Multiply: _multiply[Index(modifier.stat)] *= modifier.value; break;
            }
        }
    }

    // The instant bonus is added alongside other Add effects, before Multiply effects.
    [[nodiscard]] float Get(Id stat, float additiveBonus = 0.0f) const {
        const float value = (Rules::Base(stat) + _add[Index(stat)] + additiveBonus) * _multiply[Index(stat)];
        return Rules::Finalize(stat, value);
    }
    // A stat used as a count (radius, quantity, level): nearest integer.
    [[nodiscard]] int GetRounded(Id stat) const { return static_cast<int>(std::lround(Get(stat))); }

private:
    static constexpr std::size_t Count = static_cast<std::size_t>(Id::Count);
    static constexpr std::size_t Index(Id stat) { return static_cast<std::size_t>(stat); }

    std::array<float, Count> _add{};
    std::array<float, Count> _multiply{};
};
