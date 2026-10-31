#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include "Modules/Persistence/Persistence.h"

// Persistent state of tiered goals (achievements, quests, badges): each goal's completed tiers, tiers seen on screen,
// and progress carried over from finished sessions. The game defines goals and what counts; this class only stores
// state and writes it to Persistence using "<key>.tier/seen/progress" fields. Header-only.
template <std::size_t N> class TieredProgress {
public:
    // Maximum saved progress (remains an exact integer in float).
    static constexpr std::int64_t MaxStoredProgress = 1 << 24;

    // 'keyOf(i)': goal's save key root; 'tierCountOf(i)': tier count. Values are clamped: tiers 0..tier count,
    // seen 0..tiers, progress 0..MaxStoredProgress.
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
    // Progress is not written for goals where 'storesProgress(i)' is false (goals completed within a session).
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

    // Number of completed tiers.
    [[nodiscard]] int Tier(std::size_t index) const { return _tiers[index]; }
    void CompleteTier(std::size_t index) { ++_tiers[index]; }
    // Progress carried over from finished sessions.
    [[nodiscard]] float Progress(std::size_t index) const { return _progress[index]; }
    void SetProgress(std::size_t index, float value) { _progress[index] = value; }
    // Number of unseen tiers.
    [[nodiscard]] int Unseen(std::size_t index) const { return _tiers[index] - _seen[index]; }
    [[nodiscard]] int UnseenCount() const {
        int count = 0;
        for (std::size_t i = 0; i < N; ++i)
            count += Unseen(i);
        return count;
    }
    // All completed tiers have been seen.
    void MarkSeen() { _seen = _tiers; }

private:
    std::array<int, N> _tiers{};
    std::array<int, N> _seen{};
    std::array<float, N> _progress{};
};
