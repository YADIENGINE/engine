#include "Fx/FxCueSet.h"
#include <algorithm>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <utility>
#include "Fx/Internals/FxChecks.h"

FxCueSet::FxCueSet(std::initializer_list<FxCueSetEntry> entries)
    : FxCueSet(std::vector<FxCueSetEntry>(entries)) {}
FxCueSet::FxCueSet(std::vector<FxCueSetEntry> entries) {
    if (entries.empty()) throw std::invalid_argument("A cue set needs at least one cue");
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.empty()) throw std::invalid_argument("A cue needs a name");
        if (!FxNotNegative(entries[i].cooldown))
            throw std::invalid_argument(std::format("Cue \"{}\" has an invalid cooldown", entries[i].name));
        for (std::size_t j = 0; j < i; ++j)
            if (entries[j].name == entries[i].name)
                throw std::invalid_argument(std::format("Cue \"{}\" appears twice", entries[i].name));
    }
    _entries = std::make_shared<const std::vector<FxCueSetEntry>>(std::move(entries));
}

const FxCueSetEntry* FxCueSet::Search(std::string_view name) const {
    for (const FxCueSetEntry& entry : *_entries)
        if (entry.name == name) return &entry;
    return nullptr;
}
const FxCueSetEntry& FxCueSet::Find(std::string_view name) const {
    const FxCueSetEntry* entry = Search(name);
    if (entry == nullptr) throw std::invalid_argument(std::format("The cue set has no cue \"{}\"", name));
    return *entry;
}
bool FxCueSet::Contains(std::string_view name) const { return Search(name) != nullptr; }
const FxCue& FxCueSet::Get(std::string_view name) const { return Find(name).cue; }
float FxCueSet::Cooldown(std::string_view name) const { return Find(name).cooldown; }
std::vector<std::string_view> FxCueSet::Names() const {
    std::vector<std::string_view> names;
    names.reserve(_entries->size());
    for (const FxCueSetEntry& entry : *_entries)
        names.emplace_back(entry.name);
    return names;
}
FxCueSet FxCueSet::With(std::initializer_list<FxCueSetEntry> entries) const {
    std::vector<FxCueSetEntry> merged;
    merged.reserve(_entries->size() + entries.size());
    for (const FxCueSetEntry& entry : *_entries) {
        const bool replaced =
            std::ranges::any_of(entries, [&](const FxCueSetEntry& added) { return added.name == entry.name; });
        if (!replaced) merged.push_back(entry);
    }
    merged.insert(merged.end(), entries.begin(), entries.end());
    return FxCueSet(std::move(merged));
}

const FxCueSet& DefaultFxCues() {
    static const FxCueSet cues{
        {"hit", {FxBurst{ParticleRecipe::Spark, 14}, FxRing{}, FxShake{0.4f}}, 0.05f},
        {"explosion",
         {FxBurst{ParticleRecipe::Debris, 36}, FxBurst{ParticleRecipe::Soft, 24}, FxShake{0.8f}, FxPunch{0.06f},
          {0.06f, FxShockwave{160.0f}}}},
        {"pickup", {FxBurst{ParticleRecipe::Sparkle, 10}, FxRing{}}},
    };
    return cues;
}
