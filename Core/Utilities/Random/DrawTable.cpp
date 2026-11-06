#include "Utilities/Random/DrawTable.h"

namespace {
// The entry whose pity is most overdue, or -1 when none is due.
int DueEntry(const std::vector<DrawEntry>& entries, const DrawMemory& memory) {
    int due = -1;
    int overdue = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const int pity = entries[i].pity;
        if (pity <= 0 || memory.sinceDrawn[i] < pity) continue;
        const int by = memory.sinceDrawn[i] - pity;
        if (due < 0 || by > overdue) {
            due = static_cast<int>(i);
            overdue = by;
        }
    }
    return due;
}

int RolledEntry(const DrawTable& table, const DrawMemory& memory, Rng& rng) {
    const auto& entries = table.entries;
    const auto may = [&](std::size_t i, bool avoidLast) {
        return entries[i].weight > 0.0 && !(avoidLast && static_cast<int>(i) == memory.last);
    };
    bool avoidLast = false;
    if (table.noRepeat)
        for (std::size_t i = 0; i < entries.size() && !avoidLast; ++i)
            avoidLast = may(i, true);
    double total = 0.0;
    for (std::size_t i = 0; i < entries.size(); ++i)
        if (may(i, avoidLast)) total += entries[i].weight;
    if (!(total > 0.0)) return 0;
    double remaining = rng.Double01() * total;
    int last = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!may(i, avoidLast)) continue;
        remaining -= entries[i].weight;
        if (remaining < 0.0) return static_cast<int>(i);
        last = static_cast<int>(i); // Rounding can leave a tiny remainder after the last entry that may come up
    }
    return last;
}
} // namespace

std::size_t DrawTable::Draw(DrawMemory& memory, Rng& rng) const {
    if (entries.empty()) return 0;
    memory.sinceDrawn.resize(entries.size(), 0); // A table that grew keeps the counters it had
    if (memory.last >= static_cast<int>(entries.size())) memory.last = -1;
    const int due = DueEntry(entries, memory);
    const int drawn = due >= 0 ? due : RolledEntry(*this, memory, rng);
    for (int& since : memory.sinceDrawn)
        ++since;
    memory.sinceDrawn[static_cast<std::size_t>(drawn)] = 0;
    memory.last = drawn;
    return static_cast<std::size_t>(drawn);
}
