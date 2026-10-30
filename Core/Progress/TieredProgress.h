#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include "Modules/Persistence/Persistence.hpp"

// Kademeli hedeflerin (başarım, görev, rozet) kalıcı durumu: her hedefin tamamlanan kademe sayısı, ekranda görülmüş
// kademe sayısı ve bitmiş oturumlardan taşınan ilerleme değeri. Hedefleri ve neyin sayıldığını oyun tanımlar; bu sınıf
// sadece durumu tutar ve Persistence'e "<anahtar>.tier/seen/progress" alanlarıyla yazar. Header-only.
template <std::size_t N> class TieredProgress {
public:
    // Kayıttaki ilerleme üst sınırı (float'ta tam sayı olarak kalır).
    static constexpr std::int64_t MaxStoredProgress = 1 << 24;

    // 'keyOf(i)': hedefin kayıt anahtarı kökü; 'tierCountOf(i)': kademe sayısı. Değerler kırpılır: kademe
    // 0..kademe sayısı, görülme 0..kademe, ilerleme 0..MaxStoredProgress.
    template <class KeyOf, class TierCountOf> void Load(const Persistence& save, KeyOf keyOf, TierCountOf tierCountOf) {
        for (std::size_t i = 0; i < N; ++i) {
            const std::string key = keyOf(i);
            const auto tierCount = static_cast<std::int64_t>(tierCountOf(i));
            _tiers[i] = static_cast<int>(std::clamp<std::int64_t>(save.GetInt(key + ".tier"), 0, tierCount));
            _seen[i] = static_cast<int>(std::clamp<std::int64_t>(save.GetInt(key + ".seen"), 0, _tiers[i]));
            _progress[i] =
                static_cast<float>(std::clamp<std::int64_t>(save.GetInt(key + ".progress"), 0, MaxStoredProgress));
        }
    }
    // 'storesProgress(i)' false olan hedeflerin ilerlemesi yazılmaz (oturum içinde biten hedefler).
    template <class KeyOf, class StoresProgress>
    void Store(Persistence& save, KeyOf keyOf, StoresProgress storesProgress) const {
        for (std::size_t i = 0; i < N; ++i) {
            const std::string key = keyOf(i);
            save.SetInt(key + ".tier", _tiers[i]);
            save.SetInt(key + ".seen", _seen[i]);
            if (storesProgress(i)) {
                save.SetInt(key + ".progress", static_cast<std::int64_t>(std::floor(
                                                   std::min(_progress[i], static_cast<float>(MaxStoredProgress)))));
            }
        }
    }

    // Tamamlanan kademe sayısı.
    [[nodiscard]] int Tier(std::size_t index) const { return _tiers[index]; }
    void CompleteTier(std::size_t index) { ++_tiers[index]; }
    // Bitmiş oturumlardan taşınan ilerleme.
    [[nodiscard]] float Progress(std::size_t index) const { return _progress[index]; }
    void SetProgress(std::size_t index, float value) { _progress[index] = value; }
    // Henüz görülmemiş kademe sayısı.
    [[nodiscard]] int Unseen(std::size_t index) const { return _tiers[index] - _seen[index]; }
    [[nodiscard]] int UnseenCount() const {
        int count = 0;
        for (std::size_t i = 0; i < N; ++i)
            count += Unseen(i);
        return count;
    }
    // Tamamlanan bütün kademeler görüldü.
    void MarkSeen() { _seen = _tiers; }

private:
    std::array<int, N> _tiers{};
    std::array<int, N> _seen{};
    std::array<float, N> _progress{};
};
