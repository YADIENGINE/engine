#pragma once
#include <cstddef>
#include <vector>
#include "Utilities/Random/Rng.h"

// A weighted table that remembers: loot, enemy waves, card draws, bonus rolls. The table is fixed data (it can come
// from a tuning file); what it remembers between draws lives in a DrawMemory the caller owns, so a simulation keeps
// that in its state and a replay draws the same entries. The same table, memory and generator give the same entry in
// every build of the same program.
//     const DrawTable loot{.entries = {{.weight = 70}, {.weight = 25}, {.weight = 5, .pity = 20}}, .noRepeat = true};
//     struct RunState { Rng rng; DrawMemory lootMemory; ... };        // Both go into State
//     const std::size_t drop = loot.Draw(_run.lootMemory, _run.rng);  // Entry 2 at the latest on the 21st draw

struct DrawEntry {
    double weight = 1.0; // In proportion to the other entries; 0 or less never comes up by chance
    // Comes up for sure once this many draws in a row went without it ("pity"); 0 means never forced. A forced entry
    // comes up even when its weight is 0.
    int pity = 0;
};

// What a table remembers: a plain struct, so state.Field("lootMemory", memory) hashes and dumps every counter.
struct DrawMemory {
    std::vector<int> sinceDrawn; // Draws since each entry last came up (since the first draw, for one never drawn)
    int last = -1;               // The entry drawn last; -1 before the first draw
};

struct DrawTable {
    std::vector<DrawEntry> entries;
    // The entry drawn last never comes up twice in a row while another entry could come up instead.
    bool noRepeat = false;

    // The index of the entry drawn. An entry whose pity is due wins without a roll (the most overdue one; on a tie the
    // first). Otherwise a weighted roll among the entries that may come up. Returns 0 for a table without entries, or
    // where no entry has weight or pity.
    [[nodiscard]] std::size_t Draw(DrawMemory& memory, Rng& rng) const;
};
