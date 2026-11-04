#include "Audio/SoundPlayer.h"
#include <stdexcept>
#include <string>
#include "Audio/Internals/Validation.h"

namespace {
template <class Tables> auto* FindTable(Tables& tables, const std::type_info& type) {
    for (auto& table : tables)
        if (*table.type == type) return &table;
    return static_cast<decltype(&tables.front())>(nullptr);
}
} // namespace

SoundPlayer::SoundPlayer(const SoundTables& tables, SoundOutput& output, std::uint64_t seed)
    : _output(output)
    , _rng(seed) {
    for (const SoundTable& table : tables.Tables()) {
        Table& own = _tables.emplace_back();
        own.type = table.type;
        for (const SoundTableRow& row : table.rows)
            own.rows.push_back({.firstId = row.firstId,
                                .files = row.files.size(),
                                .pitch = row.pitch,
                                .cooldown = row.cooldown,
                                .skipWhilePlaying = row.whenPlaying.IsSkip()});
    }
}

bool SoundPlayer::Has(SoundKey key) const {
    if (!key) return false;
    const Table* table = FindTable(_tables, *key.Type());
    return table != nullptr && key.Index() < table->rows.size();
}

std::size_t SoundPlayer::PickFile(const Row& row) {
    if (row.files == 1) return 0;
    if (!row.lastFile) return _rng.Index(row.files);
    const std::size_t pick = _rng.Index(row.files - 1); // Among the others, so the same file never comes twice
    return pick >= *row.lastFile ? pick + 1 : pick;
}

bool SoundPlayer::Play(SoundKey key, float pitch) {
    if (!key) throw std::out_of_range("An empty sound key cannot play; check it first");
    const std::string name = SoundEnumName(*key.Type());
    Table* table = FindTable(_tables, *key.Type());
    if (table == nullptr)
        throw std::out_of_range("The sound enum " + name +
                                " was never registered: add its table to GameSettings::sounds");
    if (key.Index() >= table->rows.size())
        throw std::out_of_range("The sound enum " + name + " has no row " + std::to_string(key.Index()));
    ValidateAudioPitch(pitch);

    Row& row = table->rows[key.Index()];
    if (_seconds - row.lastPlayed < static_cast<double>(row.cooldown)) return false;
    const auto first = static_cast<std::size_t>(row.firstId);
    if (row.skipWhilePlaying)
        for (std::size_t i = 0; i < row.files; ++i)
            if (_output.IsPlaying(SoundId{first + i})) return false;

    const std::size_t file = PickFile(row);
    _output.Play(SoundId{first + file}, _rng.Float(row.pitch.low, row.pitch.high) * pitch);
    row.lastPlayed = _seconds;
    row.lastFile = file;
    return true;
}

void SoundPlayer::Update(float realSeconds) { _seconds += static_cast<double>(realSeconds); }
