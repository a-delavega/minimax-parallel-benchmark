#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include <memory>

using namespace mmb;

TEST_CASE("Engines: Mate in 1 Detection", "[engines]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    MinimaxEngine minimax(eval);
    AlphaBetaEngine alphabeta(eval);

    Board board;
    // Set up 3 vertical pieces for Player 1 in column 3
    // P1(3), P2(0), P1(3), P2(0), P1(3), P2(0)
    board.make_move(3); // P1
    board.make_move(0); // P2
    board.make_move(3); // P1
    board.make_move(0); // P2
    board.make_move(3); // P1
    board.make_move(0); // P2

    // Now it's P1's turn. Col 3 wins immediately!
    const auto res_mm = minimax.search(board, 2);
    const auto res_ab = alphabeta.search(board, 2);

    REQUIRE(res_mm.best_move == 3);
    REQUIRE(res_ab.best_move == 3);
    REQUIRE(res_mm.score > 90'000);
    REQUIRE(res_ab.score > 90'000);
    REQUIRE(res_mm.score == res_ab.score);
}

TEST_CASE("Engines: Block Opponent Mate in 1", "[engines]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine alphabeta(eval);

    Board board;
    // P1 sets up 3 horizontal pieces at the left boundary: cols 0, 1, 2 at row 0
    // Col -1 doesn't exist, so P1 can only win horizontally by playing col 3!
    board.make_move(0); // P1
    board.make_move(5); // P2
    board.make_move(1); // P1
    board.make_move(6); // P2
    board.make_move(2); // P1

    // It is P2's turn. P1 threatens immediate win at col 3.
    // P2 MUST play col 3 to block the win.
    const auto res = alphabeta.search(board, 2);
    REQUIRE(res.best_move == 3);
}

TEST_CASE("Engines: Mathematical Equivalence (Minimax vs AlphaBeta)", "[engines]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    MinimaxEngine minimax(eval);
    AlphaBetaEngine alphabeta(eval);

    Board board;
    // Play a few varied moves to reach an asymmetric middle position
    board.make_move(3); // P1
    board.make_move(3); // P2
    board.make_move(2); // P1
    board.make_move(4); // P2

    for (int depth = 1; depth <= 5; ++depth) {
        const auto res_mm = minimax.search(board, depth);
        const auto res_ab = alphabeta.search(board, depth);

        // AlphaBeta and Minimax MUST return the exact same evaluation score
        REQUIRE(res_mm.score == res_ab.score);

        // AlphaBeta MUST visit fewer or equal nodes
        REQUIRE(res_ab.nodes_visited <= res_mm.nodes_visited);
    }
}

TEST_CASE("Engines: AlphaBeta Pruning Efficiency", "[engines]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    MinimaxEngine minimax(eval);
    AlphaBetaEngine alphabeta(eval);

    Board board;
    const int depth = 5;

    const auto res_mm = minimax.search(board, depth);
    const auto res_ab = alphabeta.search(board, depth);

    REQUIRE(res_mm.score == res_ab.score);
    // At depth 5, AlphaBeta should prune at least 80% of nodes
    const double ratio = static_cast<double>(res_ab.nodes_visited) / res_mm.nodes_visited;
    REQUIRE(ratio < 0.20);
}
