#pragma once
#if ENGINE_TUNING_LIVE
#include "Tuning/TuningLive.h"
#endif

// Values of a tuning file (Docs/Tuning.md). T is the `XTuning` struct generated from `X.tuning.json`:
//
//     #include "Effects/Glow.tuning.h"
//     const auto& tuning = Tuning<GlowTuning>();
//
// In development (ENGINE_TUNING_LIVE), the first call registers and reads the file; changes refresh values at the
// start of the next frame, and each call tells the editor which files the open scene reads. Reading runs on the main
// thread. Release uses compiled `constexpr` values.
template <class T> [[nodiscard]] const T& Tuning() {
#if ENGINE_TUNING_LIVE
    static TuningSlot<T> slot;
    return slot.Use();
#else
    static constexpr T Values{};
    return Values;
#endif
}
