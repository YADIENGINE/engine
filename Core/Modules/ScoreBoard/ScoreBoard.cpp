#include "Modules/ScoreBoard/ScoreBoard.h"
#include "Modules/ScoreBoard/Internals/State.h"

ScoreBoard::ScoreBoard(const Options& options)
    : _state(std::make_unique<State>(options)) {}
ScoreBoard::~ScoreBoard() = default;

bool ScoreBoard::ScoreToBoard(const Input& input) {
    const auto validated = ValidateScore(input);
    if (!validated || validated->playerId.empty()) return false;
    _state->Append(*validated);
    return true;
}
std::vector<ScoreBoard::Entry> ScoreBoard::GetScoreBoard() const { return _state->Ranked(); }
std::vector<ScoreBoard::Entry> ScoreBoard::GetScoreBoardN(std::size_t count) const {
    if (count == 0) return {};
    auto entries = _state->Ranked();
    if (entries.size() > count) entries.resize(count);
    return entries;
}
std::vector<ScoreBoard::Entry> ScoreBoard::GetScoreBoardPerPlayer() const { return _state->PerPlayer(); }
void ScoreBoard::ResetScoreBoard() { _state->Reset(); }
