#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "minimax/board.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/opponent/bayesian_model.hpp"
#include <numeric>

using namespace mmb;
using Catch::Approx;

TEST_CASE("BayesianOpponentModel: Initialization & Priors", "[opponent][bayesian]") {
    BayesianOpponentModel model;

    SECTION("Initial distribution is uniform over all 5 hypotheses") {
        for (auto type : BayesianOpponentModel::HYPOTHESES) {
            REQUIRE(model.belief(type) == Approx(0.20).margin(1e-4));
        }
        REQUIRE(model.moves_observed() == 0);
    }
}

TEST_CASE("BayesianOpponentModel: Evidence Updating & MAP Inference", "[opponent][bayesian]") {
    BayesianOpponentModel model;
    SimpleEvaluator eval;

    SECTION("Observing blunder sharply penalizes OptimalMinimax hypothesis") {
        Board board;
        // P2 has 3 in column 1 (winning move available in col 1)
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(6); // P1

        // P2 to move: Col 1 is an immediate win, but P2 blunders and plays Col 5!
        model.update(board, 5, Player::Player2, eval);

        REQUIRE(model.moves_observed() == 1);
        // OptimalMinimax belief should drop drastically
        REQUIRE(model.belief(OpponentType::OptimalMinimax) < 0.05);
        // Novice and Random belief should rise
        REQUIRE((model.belief(OpponentType::HumanNovice) > 0.30 || 
                 model.belief(OpponentType::Random) > 0.30));
    }

    SECTION("Observing consecutive high-quality moves elevates rational hypotheses") {
        Board board;
        // P1 has 3 in column 3, P2 must block col 3
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1

        // P2 plays the perfect defensive block in Col 3!
        model.update(board, 3, Player::Player2, eval);

        // Optimal or Greedy should have higher probability than Random
        REQUIRE(model.belief(OpponentType::OptimalMinimax) > model.belief(OpponentType::Random));
        REQUIRE(model.belief(OpponentType::Greedy) > model.belief(OpponentType::Random));
    }
}

TEST_CASE("BayesianOpponentModel: Marginal Predictive Distribution", "[opponent][bayesian]") {
    BayesianOpponentModel model;
    SimpleEvaluator eval;
    Board board;

    SECTION("Marginals sum to 1.0 across valid moves") {
        const auto probs = model.marginal_move_probabilities(board, Player::Player2, eval);
        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);

        REQUIRE(sum == Approx(1.0).margin(1e-6));
        for (double p : probs) {
            REQUIRE(p > 0.0);
        }
    }

    SECTION("Full columns receive strictly 0.0 marginal probability") {
        // Fill column 3
        for (int i = 0; i < 6; ++i) {
            board.make_move(3);
        }

        const auto probs = model.marginal_move_probabilities(board, Player::Player1, eval);
        REQUIRE(probs[3] == 0.0);
        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
        REQUIRE(sum == Approx(1.0).margin(1e-6));
    }
}
