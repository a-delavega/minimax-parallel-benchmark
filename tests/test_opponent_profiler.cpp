#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "minimax/board.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/opponent/heuristic_agents.hpp"
#include "minimax/opponent/opponent_profiler.hpp"
#include <memory>
#include <numeric>

using namespace mmb;
using Catch::Approx;

TEST_CASE("OpponentProfiler: Reference Heuristic Agents", "[opponent][agents]") {
    auto eval = std::make_shared<SimpleEvaluator>();

    SECTION("RandomEngine selects valid legal moves") {
        RandomEngine rng_engine(12345ULL);
        Board board;
        board.make_move(3);
        board.make_move(3);

        const SearchResult res = rng_engine.search(board, 1);
        REQUIRE(res.best_move >= 0);
        REQUIRE(res.best_move < COLS);
        REQUIRE(board.is_valid_move(res.best_move));
    }

    SECTION("GreedyEngine executes immediate winning move") {
        GreedyEngine greedy(eval);
        Board board;
        // P1 has 3 in column 2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2
        // P1 to move -> col 2 is immediate win
        const SearchResult res = greedy.search(board, 1);
        REQUIRE(res.best_move == 2);
        REQUIRE(res.score >= MATE_THRESHOLD);
    }

    SECTION("GreedyEngine blocks opponent immediate winning threat") {
        GreedyEngine greedy(eval);
        Board board;
        // P1 has 3 in column 4
        board.make_move(4); // P1
        board.make_move(0); // P2
        board.make_move(4); // P1
        board.make_move(0); // P2
        board.make_move(4); // P1
        // P2 to move -> P2 must block col 4
        const SearchResult res = greedy.search(board, 1);
        REQUIRE(res.best_move == 4);
    }
}

TEST_CASE("OpponentProfiler: Dynamic Move Recording & Classification", "[opponent][profiler]") {
    OpponentProfiler profiler;

    SECTION("Clean moves maintain high tactical awareness") {
        Board board;
        profiler.record_move(board, 3, Player::Player2);
        board.make_move(3);
        profiler.record_move(board, 3, Player::Player2);

        REQUIRE(profiler.moves_observed() == 2);
        REQUIRE(profiler.missed_wins() == 0);
        REQUIRE(profiler.unblocked_losses() == 0);
        REQUIRE(profiler.blunder_rate() == 0.0);
        REQUIRE(profiler.center_bias() == 1.0);
    }

    SECTION("Detects missed immediate win and classifies as flawed agent") {
        Board board;
        // P2 has 3 in col 1 (vertical win available in col 1)
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(6); // P1

        // P2 to move: Col 1 wins! But P2 plays Col 5 instead!
        profiler.record_move(board, 5, Player::Player2);

        REQUIRE(profiler.missed_wins() == 1);
        REQUIRE(profiler.blunder_rate() == 1.0);
        REQUIRE(profiler.profile().tactical_awareness < 0.9);
        REQUIRE(profiler.profile().type != OpponentType::OptimalMinimax);
    }

    SECTION("Detects unblocked immediate loss") {
        Board board;
        // P1 has 3 in col 3 (P1 will win if col 3 not blocked)
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        // P2 to move: Col 3 must be blocked! But P2 plays Col 6!
        profiler.record_move(board, 6, Player::Player2);

        REQUIRE(profiler.unblocked_losses() == 1);
        REQUIRE(profiler.blunder_rate() == 1.0);
    }
}

TEST_CASE("OpponentProfiler: Move Probability Prediction", "[opponent][profiler]") {
    SimpleEvaluator eval;
    OpponentProfiler profiler(OpponentProfile::preset_intermediate());

    SECTION("Predicts heavy probability on immediate winning move") {
        Board board;
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(6); // P1
        // P2 has 3 in col 0 -> col 0 wins immediately for P2
        const auto probs = profiler.predict_move_probabilities(board, Player::Player2, eval);

        REQUIRE(probs[0] > 0.50);
        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
        REQUIRE(sum == Approx(1.0).margin(1e-6));
    }

    SECTION("Predicts heavy probability on urgent defensive block") {
        Board board;
        board.make_move(4); // P1
        board.make_move(0); // P2
        board.make_move(4); // P1
        board.make_move(0); // P2
        board.make_move(4); // P1
        // P1 threatens col 4 -> P2 must block col 4
        const auto probs = profiler.predict_move_probabilities(board, Player::Player2, eval);

        REQUIRE(probs[4] > 0.40);
        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
        REQUIRE(sum == Approx(1.0).margin(1e-6));
    }
}
