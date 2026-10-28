#pragma once
#include <algorithm>
#include <optional>
#include <vector>

// Süreli ve kalıcı etkiler (buff, debuff, durum): süreli etki tekrar alınınca süresi kalana eklenir ve yığını artar,
// kalıcı etkiler ayrı kopyalar olarak üst üste biner. Eklenme sırası korunur. Etkinin ne yaptığını (yığının gücü dahil)
// oyun bilir (Id'ye göre); bu sınıf sadece süreleri ve yığınları tutar. Header-only.
template <class Id> struct ActiveEffect {
    Id id;
    float duration = 0.0f;          // Kalan süre barının tamamı (eklenince kalanla birlikte büyür); kalıcıda 0
    std::optional<float> remaining; // nullopt: kalıcı
    int stacks = 1;                 // Süreli etkinin üst üste alınma sayısı (en çok 'maxStacks'); kalıcıda 1
};

template <class Id> class TimedEffectSet {
public:
    // Süreli etki: aktifse 'duration' kalan süreye eklenir ve yığın 'maxStacks'e kadar artar, değilse sona eklenir.
    void AddTimed(Id id, float duration, int maxStacks) {
        const auto it = std::find_if(_active.begin(), _active.end(),
                                     [&](const auto& effect) { return effect.id == id && effect.remaining; });
        if (it != _active.end()) {
            *it->remaining += duration;
            it->duration = *it->remaining;
            it->stacks = std::min(it->stacks + 1, std::max(maxStacks, 1));
            return;
        }
        _active.push_back({id, duration, duration});
    }
    // Kalıcı etki: her ekleme ayrı bir kopyadır, hiç bitmez.
    void AddPersistent(Id id) { _active.push_back({id, 0.0f, std::nullopt}); }

    // Aktif süreli etkileri mevcut çubuklarının tam süresine doldurur; yığınlar ve kalıcı etkiler değişmez.
    // 'except' verilirse o kimlik yenilenmez. Biten etkileri geri getirmez, süre eklemez.
    void RefreshTimed(std::optional<Id> except = std::nullopt) {
        for (auto& effect : _active)
            if (effect.remaining && (!except || effect.id != *except)) *effect.remaining = effect.duration;
    }

    // Süreleri ilerletir. Süresi dolan etki olduysa true döner.
    bool Update(float deltaSeconds) {
        for (auto& effect : _active)
            if (effect.remaining) *effect.remaining -= deltaSeconds;
        return std::erase_if(_active,
                             [](const auto& effect) { return effect.remaining && *effect.remaining <= 0.0f; }) > 0;
    }

    [[nodiscard]] const std::vector<ActiveEffect<Id>>& Active() const { return _active; }
    // Aktif süreli etkilerin eklenme sırasındaki ilki; yoksa nullopt.
    [[nodiscard]] std::optional<Id> FirstTimed() const {
        for (const auto& effect : _active)
            if (effect.remaining) return effect.id;
        return std::nullopt;
    }

private:
    std::vector<ActiveEffect<Id>> _active;
};
