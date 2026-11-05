#pragma once
#include <array>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>
#include "Audio/AudioSystem.h"
#include "Audio/SoundRow.h"
#include "Utilities/EnumTable.h"

// A row without its enum type: what the player and the catalog read. Owns its text.
struct SoundTableRow {
    SoundId firstId{}; // The row's files have consecutive catalog IDs from here
    std::vector<std::string> files;
    float volume = 1.0f;
    SoundPitch pitch{};
    float cooldown = 0.0f;
    SoundOverlap whenPlaying = SoundOverlap::Copies(4);
    bool loop = false;
    bool streamed = false;
    SoundBus bus = SoundBus::World;
    SoundFalloff falloff{};
    std::string label; // The row's own, or generated
};
struct SoundTable {
    const std::type_info* type = nullptr;
    std::vector<SoundTableRow> rows;
};

// An enum's name for messages and labels: "Sfx" for an enum Sfx.
[[nodiscard]] std::string SoundEnumName(const std::type_info& type);

// The game's sound tables, one per enum type: `settings.sounds = SfxTable;` or `settings.sounds = {SfxTable, Ui};`.
// A table that misses rows fails to compile. Rows out of order, or two tables of one enum, throw std::invalid_argument;
// static_assert(RowsInIdOrder(table)) makes the first a compile error as well.
class SoundTables {
public:
    // Generated catalog IDs start here, so that a game's own low IDs in GameSettings::audio stay out of their way.
    static constexpr std::size_t FirstGeneratedId = std::size_t{1} << 32U;

    SoundTables() = default;
    template <SoundEnum Enum, std::size_t Size>
        requires(Size == static_cast<std::size_t>(Enum::Count))
    SoundTables(const std::array<SoundRow<Enum>, Size>& table) { // Implicit on purpose: `settings.sounds = SfxTable`
        if (!RowsInIdOrder(table))
            throw std::invalid_argument("The sound table of enum " + SoundEnumName(typeid(Enum)) +
                                        " has rows out of enum order");
        SoundTable erased{&typeid(Enum), {}};
        erased.rows.reserve(Size);
        for (const SoundRow<Enum>& row : table)
            erased.rows.push_back(EraseRow(row.files, row.options));
        Add(std::move(erased));
    }
    SoundTables(std::initializer_list<SoundTables> tables);

    [[nodiscard]] const std::vector<SoundTable>& Tables() const { return _tables; }

private:
    [[nodiscard]] static SoundTableRow EraseRow(const SoundFiles& files, const SoundOptions& options);
    void Add(SoundTable table);

    std::vector<SoundTable> _tables;
    std::size_t _nextId = FirstGeneratedId;
};

// `catalog` with one effect for each file of each table. Throws std::invalid_argument when the result is invalid, or
// when `catalog` itself uses an effect ID at or above SoundTables::FirstGeneratedId.
[[nodiscard]] AudioCatalog CatalogWithSounds(const AudioCatalog& catalog, const SoundTables& tables);
