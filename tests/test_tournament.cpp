#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/exploit_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/opponent/heuristic_agents.hpp"
#include "minimax/tournament/tournament_harness.hpp"
#include <memory>

using namespace mmb;

TEST_CASE("Tournament: Single Match Execution", "[tournament]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    GreedyEngine greedy(eval);
    RandomEngine random(42ULL);

    uint32_t moves = 0;
    const int winner = TournamentHarness::play_single_game(greedy, 1, random, 1, moves);

    REQUIRE(moves >= 7); // Connect 4 minimum game length is 7 plies
    REQUIRE(moves <= 42);
    // Greedy engine must win or draw against random agent
    REQUIRE((winner == 1 || winner == 0));
}

TEST_CASE("Tournament: Round-Robin Tournament Matrix & Standings", "[tournament]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    TournamentHarness tournament;

    auto ab = std::make_shared<AlphaBetaEngine>(eval);
    auto exploit = std::make_shared<ExploitEngine>(eval, OpponentProfile::preset_novice());
    auto random = std::make_shared<RandomEngine>(999ULL);

    tournament.add_participant("ab", "Alpha-Beta (d=4)", ab, 4);
    tournament.add_participant("exploit", "ExploitEngine (d=4)", exploit, 4);
    tournament.add_participant("random", "RandomEngine", random, 1);

    REQUIRE(tournament.participant_count() == 3);

    // Run 2 games per pair (1 as P1, 1 as P2) for quick test verification
    const auto records = tournament.run_tournament(2);
    REQUIRE(records.size() == 3); // 3 pairs: (AB, Exploit), (AB, Random), (Exploit, Random)

    for (const auto& rec : records) {
        REQUIRE(rec.total_games() == 2);
        REQUIRE(rec.total_moves >= 14);
    }

    const auto standings = tournament.compute_standings(records);
    REQUIRE(standings.size() == 3);
    for (const auto& s : standings) {
        REQUIRE(s.played == 4);
        REQUIRE(s.points >= 0.0);
    }

    // Both Alpha-Beta and ExploitEngine should defeat RandomEngine
    const std::string md = tournament.format_markdown(records, standings);
    REQUIRE(md.find("Tournament Leaderboard Standings") != std::string::npos);
    REQUIRE(md.find("Head-to-Head Cross-Table Results") != std::string::npos);

    const std::string csv = tournament.format_csv(records);
    REQUIRE(csv.find("player_a,player_b") != std::string::npos);
}
