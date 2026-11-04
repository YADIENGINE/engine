#include "Audio/SoundTables.h"
#include <string_view>

namespace {
std::string WithoutPrefix(std::string name, std::string_view prefix) {
    if (name.starts_with(prefix)) name.erase(0, prefix.size());
    return name;
}
std::string VariantLabel(const std::string& label, std::size_t variant, std::size_t count) {
    if (count == 1) return label;
    return label + " (" + std::to_string(variant + 1) + "/" + std::to_string(count) + ")";
}
} // namespace

// This compiler's typeid names read "enum Sfx" or "enum `anonymous namespace'::Sfx".
std::string SoundEnumName(const std::type_info& type) {
    return WithoutPrefix(WithoutPrefix(type.name(), "enum "), "`anonymous namespace'::");
}

SoundTables::SoundTables(std::initializer_list<SoundTables> tables) {
    for (const SoundTables& group : tables)
        for (const SoundTable& table : group._tables)
            Add(table);
}

SoundTableRow SoundTables::EraseRow(const SoundFiles& files, const SoundOptions& options) {
    CheckSoundRow(files, options); // A table that is not constexpr can change a row after making it
    SoundTableRow row;
    for (const std::string_view file : files)
        if (!file.empty()) row.files.emplace_back(file);
    row.volume = options.volume;
    row.pitch = options.pitch;
    row.cooldown = options.cooldown;
    row.whenPlaying = options.whenPlaying;
    row.label = options.label;
    return row;
}

// The only place that gives out IDs: the catalog and the player both read them from the rows.
void SoundTables::Add(SoundTable table) {
    for (const SoundTable& other : _tables)
        if (*other.type == *table.type)
            throw std::invalid_argument("Two sound tables use the enum " + SoundEnumName(*table.type));
    const std::string name = SoundEnumName(*table.type);
    for (std::size_t i = 0; i < table.rows.size(); ++i) {
        SoundTableRow& row = table.rows[i];
        row.firstId = SoundId{_nextId};
        _nextId += row.files.size();
        if (row.label.empty()) row.label = name + "[" + std::to_string(i) + "]";
    }
    _tables.push_back(std::move(table));
}

AudioCatalog CatalogWithSounds(const AudioCatalog& catalog, const SoundTables& tables) {
    for (const EffectAsset& effect : catalog.effects)
        if (static_cast<std::size_t>(effect.id) >= SoundTables::FirstGeneratedId)
            throw std::invalid_argument("Sound ID " + std::to_string(static_cast<std::size_t>(effect.id)) +
                                        " is reserved for sound tables");
    AudioCatalog full = catalog;
    for (const SoundTable& table : tables.Tables())
        for (const SoundTableRow& row : table.rows) {
            const EffectSettings settings{.maxConcurrent = row.whenPlaying.Voices(),
                                          .volume = row.volume,
                                          .overflow = row.whenPlaying.IsSkip() ? EffectOverflow::Skip
                                                                               : EffectOverflow::RestartNext};
            for (std::size_t i = 0; i < row.files.size(); ++i)
                full.effects.push_back({SoundId{static_cast<std::size_t>(row.firstId) + i}, row.files[i], settings,
                                        VariantLabel(row.label, i, row.files.size())});
        }
    AudioSystem::ValidateCatalog(full);
    return full;
}
