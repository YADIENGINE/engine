#pragma once
#include "Tuning/TuningLive.h"

// Bir tuning dosyasının değerleri (Docs/Tuning.md). T, `X.tuning.json`'dan üretilen `XTuning` struct'ıdır:
//
//     #include "Effects/Glow.tuning.h"
//     const auto& tuning = Tuning<GlowTuning>();
//
// Geliştirmede (ENGINE_TUNING_LIVE) ilk çağrı dosyayı kaydeder ve okur; dosya değişince değerler bir sonraki karenin
// başında yenilenir. Okuma ana iş parçacığındadır. Release'de derlenmiş `constexpr` değerlerdir.
template <class T> [[nodiscard]] const T& Tuning() {
#if ENGINE_TUNING_LIVE
    static TuningSlot<T> slot;
    return slot.Value();
#else
    static constexpr T Values{};
    return Values;
#endif
}
