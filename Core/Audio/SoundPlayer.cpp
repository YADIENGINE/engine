#include "Audio/SoundPlayer.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include "Audio/Internals/LiveVoices.h"
#include "Audio/Internals/Validation.h"

namespace {
// Long enough for the calls that start the voices to land before the frame they name, at any buffer size in use.
constexpr std::uint64_t TogetherLeadFrames = 2048;

template <class Tables> auto* FindTable(Tables& tables, const std::type_info& type) {
    for (auto& table : tables)
        if (*table.type == type) return &table;
    return static_cast<decltype(&tables.front())>(nullptr);
}

void ValidatePlay(const SoundPlay& play) {
    ValidateAudioVolume(play.volume);
    ValidateAudioPitch(play.pitch);
    ValidateAudioPan(play.pan);
    ValidateAudioMuffle(play.muffle);
    ValidateAudioFade(play.fadeIn);
    ValidateAudioSeek(play.startAt);
    if (play.at && (!std::isfinite(play.at->x) || !std::isfinite(play.at->y)))
        throw std::invalid_argument("A sound's position must be finite");
}
} // namespace

SoundPlayer::SoundPlayer(const SoundTables& tables, SoundOutput& output, std::uint64_t seed)
    : _output(output)
    , _rng(seed)
    , _live(std::make_shared<LiveVoices>(output)) {
    for (const SoundTable& table : tables.Tables()) {
        Table& own = _tables.emplace_back();
        own.type = table.type;
        for (const SoundTableRow& row : table.rows)
            own.rows.push_back({.firstId = row.firstId,
                                .files = row.files.size(),
                                .pitch = row.pitch,
                                .cooldown = row.cooldown,
                                .skipWhilePlaying = row.whenPlaying.IsSkip(),
                                .loop = row.loop,
                                .bus = row.bus,
                                .falloff = row.falloff});
    }
}

SoundPlayer::~SoundPlayer() = default;

bool SoundPlayer::Has(SoundKey key) const {
    if (!key) return false;
    const Table* table = FindTable(_tables, *key.Type());
    return table != nullptr && key.Index() < table->rows.size();
}

bool SoundPlayer::RowPlaying(const Row& row) const {
    const auto first = static_cast<std::size_t>(row.firstId);
    for (std::size_t i = 0; i < row.files; ++i)
        if (_output.IsPlaying(SoundId{first + i})) return true;
    return false;
}

bool SoundPlayer::IsPlaying(SoundKey key) const {
    return Has(key) && RowPlaying(FindTable(_tables, *key.Type())->rows[key.Index()]);
}

std::size_t SoundPlayer::PickFile(const Row& row) {
    if (row.files == 1) return 0;
    if (!row.lastFile) return _rng.Index(row.files);
    const std::size_t pick = _rng.Index(row.files - 1); // Among the others, so the same file never comes twice
    return pick >= *row.lastFile ? pick + 1 : pick;
}

SoundPlayer::Row& SoundPlayer::RowOf(SoundKey key) {
    if (!key) throw std::out_of_range("An empty sound key cannot play; check it first");
    const std::string name = SoundEnumName(*key.Type());
    Table* table = FindTable(_tables, *key.Type());
    if (table == nullptr)
        throw std::out_of_range("The sound enum " + name +
                                " was never registered: add its table to GameSettings::sounds");
    if (key.Index() >= table->rows.size())
        throw std::out_of_range("The sound enum " + name + " has no row " + std::to_string(key.Index()));
    return table->rows[key.Index()];
}

// What a row and a play's wishes come to. Picks the pitch; the caller picks the file last, so that a play the output
// drops has used up nothing.
VoiceStart SoundPlayer::Begin(Row& row, const SoundPlay& play) {
    const SoundPlacement placement =
        play.at ? _live->Place(*play.at, row.falloff) : SoundPlacement{1.0f, play.pan};
    return {.volume = play.volume,
            .pitch = _rng.Float(row.pitch.low, row.pitch.high) * play.pitch,
            .gain = placement.gain,
            .pan = placement.pan,
            .muffle = play.muffle,
            .fadeIn = play.fadeIn,
            .startAt = play.startAt,
            .loop = row.loop};
}

bool SoundPlayer::Play(SoundKey key, const SoundPlay& play) {
    Row& row = RowOf(key);
    ValidatePlay(play);
    if (row.loop)
        throw std::logic_error("A row with .loop never ends by itself: start it with Start, which hands back the sound");
    if (_seconds - row.lastPlayed < static_cast<double>(row.cooldown)) return false;
    if (row.skipWhilePlaying && RowPlaying(row)) return false;

    const auto first = static_cast<std::size_t>(row.firstId);
    const std::size_t file = PickFile(row);
    if (_output.StartVoice(SoundId{first + file}, Begin(row, play)) == VoiceId{}) return false;
    row.lastPlayed = _seconds;
    row.lastFile = file;
    return true;
}

LiveSound SoundPlayer::StartRow(Row& row, const SoundPlay& play, std::uint64_t startFrame) {
    VoiceStart start = Begin(row, play);
    start.owned = true;
    start.startFrame = startFrame;
    start.paused = _live->Held(row.bus);
    const auto first = static_cast<std::size_t>(row.firstId);
    const std::size_t file = PickFile(row);
    const VoiceId voice = _output.StartVoice(SoundId{first + file}, start);
    row.lastFile = file;
    _live->Add(voice, row.bus, row.falloff, play.at);
    return LiveSound(_live, voice);
}

LiveSound SoundPlayer::Start(SoundKey key, const SoundPlay& play) {
    Row& row = RowOf(key);
    ValidatePlay(play);
    return StartRow(row, play, 0);
}

std::vector<LiveSound> SoundPlayer::StartTogether(std::span<const SoundKey> keys, const SoundPlay& play) {
    ValidatePlay(play);
    for (const SoundKey key : keys)
        (void)RowOf(key); // All or none
    const std::uint64_t frame = _output.ClockFrames() + TogetherLeadFrames;
    std::vector<LiveSound> sounds;
    sounds.reserve(keys.size());
    for (const SoundKey key : keys)
        sounds.push_back(StartRow(RowOf(key), play, frame));
    return sounds;
}

void SoundPlayer::SetListener(const SoundListener& listener) { SetListeners(std::span(&listener, 1)); }
void SoundPlayer::SetListeners(std::span<const SoundListener> listeners) { _live->SetListeners(listeners); }
std::span<const SoundListener> SoundPlayer::Listeners() const { return _live->Listeners(); }

void SoundPlayer::FadeBus(SoundBus bus, float volume, float seconds) { _output.FadeBus(bus, volume, seconds); }
void SoundPlayer::MuffleBus(SoundBus bus, float amount, float seconds) { _output.MuffleBus(bus, amount, seconds); }
void SoundPlayer::SetBusPaused(SoundBus bus, bool paused) { _live->SetBusPaused(bus, paused); }
bool SoundPlayer::BusPaused(SoundBus bus) const { return _live->BusPaused(bus); }
void SoundPlayer::SetHold(bool held) { _live->SetHold(held); }

void SoundPlayer::Update(float realSeconds) {
    _seconds += static_cast<double>(realSeconds);
    _live->Sweep();
}
