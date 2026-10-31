#pragma once

#include "Modules/ScoreBoard/ScoreBoard.h"

class ScoreBoard::State final {
public:
    explicit State(const Options& options);
    void Append(const ValidatedScore& result);
    [[nodiscard]] std::vector<Entry> Ranked() const;
    [[nodiscard]] std::vector<Entry> PerPlayer() const;
    void Reset();

private:
    void Commit(std::vector<Entry> candidate);

    std::string _boardId;
    SortOrder _order;
    Persistence _store;
    std::vector<Entry> _entries;
};
