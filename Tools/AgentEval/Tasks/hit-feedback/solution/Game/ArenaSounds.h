#pragma once
#include <array>
#include "Audio/SoundRow.h"
#include "Utilities/EnumTable.h"

enum class ArenaSfx { Hit, Count };

inline constexpr std::array ArenaSfxTable{
    SoundRow{ArenaSfx::Hit, {"Audio/Hit.wav"}, {.pitch = {0.95f, 1.05f}, .label = "Hit"}},
};
static_assert(RowsInIdOrder(ArenaSfxTable));
