#include "Modules/ScoreBoard/ScoreBoard.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include "Tests/Support/TempDirectory.h"

namespace {
using Document = JsonIO::Document;

class TestBoard final : public ScoreBoard {
public:
    explicit TestBoard(const Options& options);
    mutable int validations = 0;
    bool reject = false;
    bool throwOnValidation = false;
    std::optional<ValidatedScore> replacement;

protected:
    [[nodiscard]] std::optional<ValidatedScore> ValidateScore(const Input& input) const override;
};

TestBoard::TestBoard(const Options& options)
    : ScoreBoard(options) {}
std::optional<ScoreBoard::ValidatedScore> TestBoard::ValidateScore(const Input& input) const {
    ++validations;
    if (throwOnValidation) throw std::runtime_error("Validator failed");
    if (reject) return std::nullopt;
    if (replacement) return replacement;
    return ValidatedScore{.score = input.score,
                          .playerId = input.playerId,
                          .playerName = input.playerName,
                          .metadata = Document::Object{{"approved", true}}};
}
ScoreBoard::Options OptionsAt(const TempDirectory& temp) {
    return {.boardId = "test-board", .file = temp.Path() / "scores.json"};
}
ScoreBoard::Input Score(std::int64_t value, const std::string& playerId = "alice") {
    return {.score = value, .playerId = playerId, .playerName = "Shared display name"};
}
std::vector<std::int64_t> Scores(const std::vector<ScoreBoard::Entry>& entries) {
    std::vector<std::int64_t> scores;
    for (const auto& entry : entries)
        scores.push_back(entry.result.score);
    return scores;
}
std::vector<std::uint64_t> Sequences(const std::vector<ScoreBoard::Entry>& entries) {
    std::vector<std::uint64_t> sequences;
    for (const auto& entry : entries)
        sequences.push_back(entry.sequence);
    return sequences;
}
static_assert(std::is_abstract_v<ScoreBoard>);
static_assert(!std::is_copy_constructible_v<TestBoard> && !std::is_move_constructible_v<TestBoard>);
static_assert(!std::is_copy_assignable_v<TestBoard> && !std::is_move_assignable_v<TestBoard>);
} // namespace

TEST_CASE("ScoreBoard.Validation.RejectionAndQueriesNeverCreateMissingFiles", "[scoreboard]") {
    TempDirectory temp;
    const auto options = OptionsAt(temp);
    {
        TestBoard board(options);
        CHECK(board.validations == 0);
        CHECK(board.GetScoreBoard().empty());
        CHECK(board.GetScoreBoardN(10).empty());
        CHECK(board.GetScoreBoardPerPlayer().empty());
        board.reject = true;
        CHECK_FALSE(board.ScoreToBoard(Score(100)));
        CHECK(board.validations == 1);
        board.reject = false;
        CHECK_FALSE(board.ScoreToBoard(Score(100, "")));
        CHECK(board.validations == 2);
        board.throwOnValidation = true;
        CHECK_THROWS_AS(board.ScoreToBoard(Score(100)), std::runtime_error);
        CHECK(board.validations == 3);
        CHECK(board.GetScoreBoard().empty());
    }
    CHECK_FALSE(std::filesystem::exists(options.file));
    CHECK_FALSE(std::filesystem::exists(options.file.string() + ".bak"));
}

TEST_CASE("ScoreBoard.Validation.PersistsOnlyApprovedOwnedValues", "[scoreboard]") {
    TempDirectory temp;
    const auto options = OptionsAt(temp);
    TestBoard board(options);
    auto input = Score(999);
    input.evidence = Document::Object{{"private_evidence", "must-not-be-persisted"}};
    board.replacement = ScoreBoard::ValidatedScore{.score = 12,
                                                   .playerId = "approved-id",
                                                   .playerName = "Approved name",
                                                   .metadata = Document::Object{{"level", 3}}};
    REQUIRE(board.ScoreToBoard(input));
    CHECK(board.validations == 1);
    auto entries = board.GetScoreBoard();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].result.score == 12);
    CHECK(entries[0].result.playerId == "approved-id");
    CHECK(entries[0].result.playerName == "Approved name");
    CHECK(entries[0].result.metadata.At("level").AsInt64() == 3);
    CHECK(entries[0].sequence == 1);
    const auto recordedAt = entries[0].recordedAtUnixMs;
    entries[0].result.metadata["level"] = 99;
    board.replacement->metadata["level"] = 88;
    TestBoard reopened(options);
    REQUIRE(reopened.GetScoreBoard().size() == 1);
    CHECK(reopened.validations == 0);
    CHECK(reopened.GetScoreBoard()[0].result.metadata.At("level").AsInt64() == 3);
    CHECK(board.GetScoreBoard()[0].result.metadata.At("level").AsInt64() == 3);
    CHECK(reopened.GetScoreBoard()[0].recordedAtUnixMs == recordedAt);
    const auto stored = JsonIO::Serialize(JsonIO::Read(options.file));
    CHECK(stored.find("private_evidence") == std::string::npos);
    CHECK(stored.find("must-not-be-persisted") == std::string::npos);
}

TEST_CASE("ScoreBoard.Queries.RankingTiesLimitsAndPlayerIdentity", "[scoreboard]") {
    TempDirectory temp;
    auto options = OptionsAt(temp);
    std::vector<std::int64_t> expectedScores;
    std::vector<std::uint64_t> expectedSequences;
    std::vector<std::uint64_t> expectedPlayers;
    SECTION("Highest first") {
        expectedScores = {20, 20, 20, 10, 0, -5};
        expectedSequences = {2, 3, 6, 1, 5, 4};
        expectedPlayers = {2, 3, 5, 4};
    }
    SECTION("Lowest first") {
        options.order = ScoreBoard::SortOrder::LowestFirst;
        expectedScores = {-5, 0, 10, 20, 20, 20};
        expectedSequences = {4, 5, 1, 2, 3, 6};
        expectedPlayers = {4, 5, 1, 2};
    }
    TestBoard board(options);
    REQUIRE(board.ScoreToBoard(Score(10)));
    REQUIRE(board.ScoreToBoard(Score(20, "bob")));
    REQUIRE(board.ScoreToBoard(Score(20)));
    REQUIRE(board.ScoreToBoard(Score(-5, "carol")));
    REQUIRE(board.ScoreToBoard(Score(0, "dave")));
    REQUIRE(board.ScoreToBoard(Score(20)));
    CHECK(Scores(board.GetScoreBoard()) == expectedScores);
    CHECK(Sequences(board.GetScoreBoard()) == expectedSequences);
    CHECK(Sequences(board.GetScoreBoardPerPlayer()) == expectedPlayers);
    CHECK(board.GetScoreBoardN(0).empty());
    CHECK(board.GetScoreBoardN(2).size() == 2);
    CHECK(board.GetScoreBoardN(2)[1].sequence == expectedSequences[1]);
    CHECK(Sequences(board.GetScoreBoardN(100)) == expectedSequences);
    TestBoard reopened(options);
    CHECK(Sequences(reopened.GetScoreBoard()) == expectedSequences);
    CHECK(Sequences(reopened.GetScoreBoardPerPlayer()) == expectedPlayers);
    REQUIRE(reopened.ScoreToBoard(Score(20)));
    const auto ranked = reopened.GetScoreBoard();
    REQUIRE(ranked.size() == 7);
    CHECK(ranked[options.order == ScoreBoard::SortOrder::HighestFirst ? 3 : 6].sequence == 7);
}

TEST_CASE("ScoreBoard.Queries.FullSignedScoreRange", "[scoreboard]") {
    TempDirectory temp;
    TestBoard board(OptionsAt(temp));
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(board.ScoreToBoard(Score(minimum)));
    REQUIRE(board.ScoreToBoard(Score(maximum)));
    CHECK(Scores(board.GetScoreBoard()) == std::vector<std::int64_t>{maximum, minimum});
}

TEST_CASE("ScoreBoard.Persistence.ResetIsDurableAndBoardsAreIndependent", "[scoreboard]") {
    TempDirectory temp;
    const auto first = OptionsAt(temp);
    auto second = first;
    second.boardId = "other-board";
    second.file = temp.Path() / "other.json";
    TestBoard board(first);
    TestBoard other(second);
    REQUIRE(board.ScoreToBoard(Score(10)));
    REQUIRE(other.ScoreToBoard(Score(20)));
    board.ResetScoreBoard();
    CHECK(board.GetScoreBoard().empty());
    CHECK(board.validations == 1);
    TestBoard reopened(first);
    CHECK(reopened.GetScoreBoard().empty());
    // Recover the reset snapshot too: the backup must not resurrect old results.
    std::filesystem::remove(first.file);
    TestBoard recovered(first);
    CHECK(recovered.GetScoreBoard().empty());
    TestBoard otherReopened(second);
    CHECK(Scores(otherReopened.GetScoreBoard()) == std::vector<std::int64_t>{20});
    REQUIRE(recovered.ScoreToBoard(Score(30)));
    CHECK(recovered.GetScoreBoard()[0].sequence == 1);
}

TEST_CASE("ScoreBoard.Persistence.RejectsIncompatibleSchemaWithoutOverwriting", "[scoreboard]") {
    TempDirectory temp;
    auto options = OptionsAt(temp);
    options.persistence.keepBackup = false;
    TestBoard seed(options);
    REQUIRE(seed.ScoreToBoard(Score(10)));
    REQUIRE(seed.ScoreToBoard(Score(20)));
    auto damaged = JsonIO::Read(options.file);
    auto& board = damaged["values"]["scoreboard"];
    auto& entries = board["entries"].Elements();
    SECTION("Unknown board version") { board["version"] = 999; }
    SECTION("Different board identity") { board["board_id"] = "wrong-board"; }
    SECTION("Missing player identity") { entries[0]["player_id"] = ""; }
    SECTION("Invalid score type") { entries[0]["score"] = 1.5; }
    SECTION("Duplicate sequence") { entries[1]["sequence"] = entries[0].At("sequence"); }
    SECTION("Missing metadata") { entries[0].Members().erase("metadata"); }
    SECTION("Missing board") { damaged["values"] = Document::Object{}; }
    JsonIO::Write(options.file, damaged);
    CHECK_THROWS_AS(TestBoard(options), SaveError);
    CHECK(JsonIO::Read(options.file) == damaged);
}

TEST_CASE("ScoreBoard.Persistence.RecoveryValidatesBoardSchemaInBothFiles", "[scoreboard]") {
    TempDirectory temp;
    const auto options = OptionsAt(temp);
    TestBoard seed(options);
    REQUIRE(seed.ScoreToBoard(Score(10)));
    auto damaged = JsonIO::Read(options.file);
    damaged["values"]["scoreboard"]["entries"].Elements()[0]["player_id"] = "";
    JsonIO::Write(options.file, damaged);
    SECTION("Valid backup restores semantically invalid primary") {
        TestBoard recovered(options);
        CHECK(Scores(recovered.GetScoreBoard()) == std::vector<std::int64_t>{10});
        CHECK(recovered.validations == 0);
        TestBoard reopened(options);
        CHECK(Scores(reopened.GetScoreBoard()) == std::vector<std::int64_t>{10});
    }
    SECTION("Invalid backup never becomes an empty board") {
        JsonIO::Write(options.file.string() + ".bak", damaged);
        CHECK_THROWS_AS(TestBoard(options), SaveError);
        CHECK(JsonIO::Read(options.file) == damaged);
    }
}

TEST_CASE("ScoreBoard.Persistence.WriteFailuresKeepPublishedSnapshotAndDoNotLeakCandidates", "[scoreboard]") {
    TempDirectory temp;
    const auto options = OptionsAt(temp);
    TestBoard board(options);
    REQUIRE(board.ScoreToBoard(Score(10)));
    const auto backup = std::filesystem::path(options.file.string() + ".bak");
    // A directory at the backup destination blocks the write before primary replacement.
    std::filesystem::remove(backup);
    std::filesystem::create_directory(backup);
    CHECK_THROWS_AS(board.ScoreToBoard(Score(20)), SaveError);
    CHECK(Scores(board.GetScoreBoard()) == std::vector<std::int64_t>{10});
    CHECK_THROWS_AS(board.ResetScoreBoard(), SaveError);
    CHECK(Scores(board.GetScoreBoard()) == std::vector<std::int64_t>{10});
    TestBoard unchanged(options);
    CHECK(Scores(unchanged.GetScoreBoard()) == std::vector<std::int64_t>{10});
    std::filesystem::remove(backup);
    REQUIRE(board.ScoreToBoard(Score(30)));
    CHECK(Scores(board.GetScoreBoard()) == std::vector<std::int64_t>{30, 10});
    CHECK(Sequences(board.GetScoreBoard()) == std::vector<std::uint64_t>{2, 1});
}

TEST_CASE("ScoreBoard.Persistence.BackupRefreshFailureMayFollowPrimaryCommit", "[scoreboard]") {
    TempDirectory temp;
    const auto options = OptionsAt(temp);
    TestBoard board(options);
    // With no old primary, the first backup write happens after primary commit.
    std::filesystem::create_directory(options.file.string() + ".bak");
    CHECK_THROWS_AS(board.ScoreToBoard(Score(10)), SaveError);
    CHECK(board.GetScoreBoard().empty());
    TestBoard reopened(options);
    CHECK(Scores(reopened.GetScoreBoard()) == std::vector<std::int64_t>{10});
}

TEST_CASE("ScoreBoard.Persistence.FileLimitsAndCallerSchemaArePreserved", "[scoreboard]") {
    TempDirectory temp;
    auto options = OptionsAt(temp);
    options.persistence.maxFileBytes = 4096;
    bool allow = true;
    options.persistence.acceptsValues = [&allow](const Document::Object&) { return allow; };
    TestBoard board(options);
    REQUIRE(board.ScoreToBoard(Score(10)));
    SECTION("Size failure never evicts previous results") {
        board.replacement = ScoreBoard::ValidatedScore{
            .score = 20, .playerId = "bob", .metadata = std::string(options.persistence.maxFileBytes, 'x')};
        CHECK_THROWS_AS(board.ScoreToBoard(Score(20)), SaveError);
    }
    SECTION("Caller predicate is combined with the board predicate") {
        allow = false;
        CHECK_THROWS_AS(board.ScoreToBoard(Score(20)), SaveError);
        CHECK_THROWS_AS(TestBoard(options), SaveError);
        allow = true;
    }
    CHECK(Scores(board.GetScoreBoard()) == std::vector<std::int64_t>{10});
    TestBoard reopened(options);
    CHECK(Scores(reopened.GetScoreBoard()) == std::vector<std::int64_t>{10});
}
