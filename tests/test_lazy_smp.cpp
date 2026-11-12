#include <catch2/catch_test_macros.hpp>
#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include <memory>

using namespace mmb;

TEST_CASE("LazySmpEngine: Configuration & Interface", "[engine][lazy_smp]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    LazySmpEngine engine(eval, 4);

    SECTION("Engine properties are valid") {
        REQUIRE(engine.thread_count() == 4);
        REQUIRE(std::string(engine.name()).find("LazySmpEngine") != std::string::npos);
    }

    SECTION("Dynamic thread reconfiguration") {
        engine.set_thread_count(2);
        REQUIRE(engine.thread_count() == 2);

        engine.set_thread_count(8);
        REQUIRE(engine.thread_count() == 8);
    }
}

TEST_CASE("LazySmpEngine: Tactical Game Correctness", "[engine][lazy_smp]") {
    auto simple_eval = std::make_shared<SimpleEvaluator>();

    SECTION("Detects immediate Mate in 1 across 1, 2, and 4 threads") {
        Board board;
        // P1 has 3 pieces in column 3
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2

        for (std::size_t threads : {1ULL, 2ULL, 4ULL}) {
            LazySmpEngine engine(simple_eval, threads);
            const SearchResult res = engine.search(board, 3);
            REQUIRE(res.best_move == 3);
            REQUIRE(res.score > 90'000);
        }
    }

    SECTION("Blocks opponent Mate in 1 with multiple threads") {
        Board board;
        // P1 has 3 pieces in column 2; P2 must block column 2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1

        LazySmpEngine engine(simple_eval, 4);
        const SearchResult res = engine.search(board, 4);
        REQUIRE(res.best_move == 2);
    }

    SECTION("Mathematical Equivalence with Sequential Alpha-Beta on Tactical Mate") {
        // Build tactical mate position
        Board board;
        const std::vector<Column> moves = {3, 3, 2, 4, 3, 2, 4, 4, 3, 2, 2, 1, 3, 2};
        for (Column col : moves) {
            board.make_move(col);
        }

        AlphaBetaEngine seq_engine(simple_eval);
        const SearchResult seq_res = seq_engine.search(board, 7);

        LazySmpEngine par_engine(simple_eval, 4);
        const SearchResult par_res = par_engine.search(board, 7);

        // Both must discover the exact winning move and score
        REQUIRE(par_res.best_move == seq_res.best_move);
        REQUIRE(par_res.score == seq_res.score);
        REQUIRE(par_res.score > 90'000); // Winning mate
    }
}

TEST_CASE("LazySmpEngine: Multi-Threaded Search & Scaling", "[engine][lazy_smp]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    Board board;

    SECTION("Opening search selects valid center move and aggregates nodes") {
        LazySmpEngine engine(eval, 4);
        const SearchResult res = engine.search(board, 6);

        REQUIRE(res.best_move == 3); // Col 3 (0-based) center column
        REQUIRE(res.nodes_visited > 1'000);
        REQUIRE(res.depth_reached == 6);
        REQUIRE(res.duration.count() > 0);
    }

    SECTION("Consecutive searches execute cleanly without deadlocks") {
        LazySmpEngine engine(eval, 2);
        for (int d = 2; d <= 5; ++d) {
            const SearchResult res = engine.search(board, d);
            REQUIRE(res.best_move != -1);
            REQUIRE(res.nodes_visited > 0);
        }
    }
}
