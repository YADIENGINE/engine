#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

// Oyuncu / birim stat'ları: her değer (temel + toplam Add + anlık bonus) × çarpım Multiply. Oyun stat kimliklerini
// kendi enum'uyla (Id::Count sonda) ve küçük bir kural tipiyle verir; Engine oyunun stat'larını bilmez. Header-only.
//
//   struct MyStatRules {
//       static float Base(MyStat stat);                // Modifier uygulanmamış temel değer
//       static float Finalize(MyStat stat, float v);   // Son kırpma (ör. şans 0..1); gerekmiyorsa 'v'
//   };
//   using MyStats = StatSheet<MyStat, MyStatRules>;

// Add: temele eklenir. Multiply: toplamı çarpar.
enum class ModOp { Add, Multiply };

// Bir stat'ı değiştiren tek etki (wildcard, kart, buff ...).
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

    // Tüm modifier'ları kaldırır, stat'lar temel değerlerine döner.
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

    // Anlık bonus da diğer Add etkileriyle birlikte, Multiply etkilerinden önce eklenir.
    [[nodiscard]] float Get(Id stat, float additiveBonus = 0.0f) const {
        const float value = (Rules::Base(stat) + _add[Index(stat)] + additiveBonus) * _multiply[Index(stat)];
        return Rules::Finalize(stat, value);
    }
    // Sayı olarak kullanılan stat (yarıçap, adet, seviye): en yakın tam sayı.
    [[nodiscard]] int GetRounded(Id stat) const { return static_cast<int>(std::lround(Get(stat))); }

private:
    static constexpr std::size_t Count = static_cast<std::size_t>(Id::Count);
    static constexpr std::size_t Index(Id stat) { return static_cast<std::size_t>(stat); }

    std::array<float, Count> _add{};
    std::array<float, Count> _multiply{};
};
