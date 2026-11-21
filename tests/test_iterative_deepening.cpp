#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/transposition/transposition_table.hpp"
#include <memory>
#include <vector>

using namespace mmb;

TEST_CASE("IterativeDeepening: AlphaBeta Sequential Execution", "[iterative_deepening]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine engine(eval);

    Board board;
    // Play a couple opening moves: col 3 (P1), col 3 (P2)
    board.make_move(3);
    board.make_move(3);

    SECTION("Reaches expected max depth and completed iterations") {
        const int max_d = 5;
        SearchResult res = engine.search_iterative(board, max_d);

        REQUIRE(res.best_move >= 0);
        REQUIRE(res.best_move < COLS);
        REQUIRE(res.depth_reached == max_d);
        REQUIRE(res.iterations_completed == max_d);
        REQUIRE(res.nodes_visited > 0);
        REQUIRE(res.duration.count() >= 0);
    }

    SECTION("Intermediate iteration callbacks report strictly increasing depths") {
        std::vector<int> completed_depths;
        std::vector<uint64_t> cumulative_nodes;

        const int target_d = 4;
        SearchResult res = engine.search_iterative(board, target_d, [&](const SearchResult& iter_res) {
            completed_depths.push_back(iter_res.depth_reached);
            cumulative_nodes.push_back(iter_res.nodes_visited);
            return true; // continue
        });

        REQUIRE(completed_depths.size() == 4);
        REQUIRE(completed_depths == std::vector<int>{1, 2, 3, 4});
        REQUIRE(res.depth_reached == 4);

        // Nodes must accumulate strictly monotonically
        for (std::size_t i = 1; i < cumulative_nodes.size(); ++i) {
            REQUIRE(cumulative_nodes[i] > cumulative_nodes[i - 1]);
        }
    }

    SECTION("Callback can abort search early gracefully") {
        int callback_calls = 0;
        const int abort_at = 3;

        SearchResult res = engine.search_iterative(board, 6, [&](const SearchResult& iter_res) {
            callback_calls++;
            return iter_res.depth_reached < abort_at; // abort once depth 3 is reached
        });

        REQUIRE(callback_calls == 3);
        REQUIRE(res.depth_reached == abort_at);
        REQUIRE(res.iterations_completed == abort_at);
        REQUIRE(res.best_move >= 0);
    }
}

TEST_CASE("IterativeDeepening: Transposition Table Synergy", "[iterative_deepening]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    auto tt = std::make_shared<TranspositionTable>(1); // 1MB
    AlphaBetaEngine engine(eval, tt);

    Board board;
    board.make_move(3);
    board.make_move(2);
    board.make_move(3);
    board.make_move(4);

    SECTION("Iterative deepening populates TT and caches best moves") {
        SearchResult res = engine.search_iterative(board, 6);

        REQUIRE(res.depth_reached == 6);
        REQUIRE(tt->stats().stores.load() > 0);
        REQUIRE(tt->stats().probes.load() > 0);

        // Root entry should be in TT
        TTEntry root_entry;
        const bool found = tt->probe(board.zobrist_hash, root_entry);
        REQUIRE(found);
        REQUIRE(root_entry.best_move == res.best_move);
    }
}

TEST_CASE("IterativeDeepening: Early Mate Termination", "[iterative_deepening]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine engine(eval);

    Board board;
    // Set up an immediate mate-in-1 for Player 1 at column 0:
    // P1: (0,0), (0,1), (0,2)
    // P2: (1,0), (1,1), (1,2)
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2

    // Now Player 1 can win immediately by playing col 0.
    // Iterative deepening with max_depth=8 should terminate early at depth 1!
    int iterations = 0;
    SearchResult res = engine.search_iterative(board, 8, [&](const SearchResult&) {
        iterations++;
        return true;
    });

    REQUIRE(res.best_move == 0);
    REQUIRE(res.score >= SCORE_WIN - 100);
    REQUIRE(iterations == 1);
    REQUIRE(res.depth_reached == 1);
}

TEST_CASE("IterativeDeepening: Multi-Threaded Lazy SMP Search", "[iterative_deepening]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    auto tt = std::make_shared<TranspositionTable>(1); // 1MB
    LazySmpEngine engine(eval, 2, tt);

    Board board;
    board.make_move(3);
    board.make_move(3);

    SECTION("Executes multi-threaded iterative deepening up to max depth") {
        SearchResult res = engine.search_iterative(board, 4);

        REQUIRE(res.best_move >= 0);
        REQUIRE(res.best_move < COLS);
        REQUIRE(res.depth_reached == 4);
        REQUIRE(res.iterations_completed == 4);
        REQUIRE(res.nodes_visited > 0);
    }

    SECTION("Lazy SMP respects early cooperative abort via callback") {
        int callbacks = 0;
        SearchResult res = engine.search_iterative(board, 6, [&](const SearchResult& iter_res) {
            callbacks++;
            return iter_res.depth_reached < 2;
        });

        REQUIRE(callbacks == 2);
        REQUIRE(res.depth_reached == 2);
        REQUIRE(res.iterations_completed == 2);
    }
}
