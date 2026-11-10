#include <catch2/catch_test_macros.hpp>
#include "minimax/concurrency/result_reducer.hpp"
#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_pool.hpp"
#include <chrono>
#include <cstdint>
#include <vector>

using namespace mmb;

TEST_CASE("Reduction: 64-bit Atomic Packing & Torn-Read Prevention", "[concurrency][reduction]") {
    SECTION("Pack and unpack preserve negative and positive scores accurately") {
        const std::vector<std::tuple<Column, Score, int>> cases = {
            {3, 99'995, 7},
            {0, -99'991, 9},
            {6, 0, 1},
            {4, -1, 12},
            {2, 2'850, 5}
        };

        for (const auto& [col, score, depth] : cases) {
            const uint64_t packed = SharedSearchState::pack(col, score, depth);
            const UnpackedBest unpacked = SharedSearchState::unpack(packed);

            REQUIRE(unpacked.valid);
            REQUIRE(unpacked.move == col);
            REQUIRE(unpacked.score == score);
            REQUIRE(unpacked.depth == depth);
        }
    }

    SECTION("Multi-threaded CAS stress test produces zero torn reads") {
        const std::size_t num_threads = 8;
        WorkerPool pool(num_threads);
        SharedSearchState shared;

        pool.execute([](int thread_id, WorkerData& data, SharedSearchState& state) {
            for (int i = 1; i <= 5'000; ++i) {
                const Column col = static_cast<Column>((thread_id + i) % 7);
                const int depth = 6 + (i % 3); // 6, 7, 8
                // Encode column into score so any torn read would cause mismatch
                const Score score = (i * 10) + col;
                state.update_best(col, score, depth);
                data.nodes_visited++;
            }

            // Worker 3 injects a known unique global optimum at depth 9
            if (thread_id == 3) {
                state.update_best(4, 99'994, 9);
            }
        }, shared);

        const UnpackedBest best = shared.read_best();
        REQUIRE(best.valid);
        REQUIRE(best.depth == 9);
        REQUIRE(best.score == 99'994);
        REQUIRE(best.move == 4);
    }
}

TEST_CASE("Reduction: ResultReducer Game-Theoretic Dominance Rules", "[concurrency][reduction]") {
    SECTION("Aggregates nodes across all workers including partial helpers") {
        WorkerPool pool(4);
        SharedSearchState shared;

        pool.worker_data(0).reset(0);
        pool.worker_data(0).nodes_visited = 1'000;
        pool.worker_data(0).best_move = 3;
        pool.worker_data(0).best_score = 120;
        pool.worker_data(0).depth_reached = 6;
        pool.worker_data(0).search_completed = true;

        pool.worker_data(1).reset(1);
        pool.worker_data(1).nodes_visited = 2'500;
        pool.worker_data(1).best_move = 2;
        pool.worker_data(1).best_score = 500; // Partial interrupted worker
        pool.worker_data(1).depth_reached = 7;
        pool.worker_data(1).search_completed = false;

        pool.worker_data(2).reset(2);
        pool.worker_data(2).nodes_visited = 1'500;
        pool.worker_data(2).best_move = 4;
        pool.worker_data(2).best_score = 90;
        pool.worker_data(2).depth_reached = 6;
        pool.worker_data(2).search_completed = true;

        pool.worker_data(3).reset(3);
        pool.worker_data(3).nodes_visited = 500;

        const SearchResult res = ResultReducer::reduce(pool, shared, std::chrono::microseconds{1500});

        REQUIRE(res.nodes_visited == 5'500);
        REQUIRE(res.duration.count() == 1500);
        // Interrupted Worker 1 is ignored; Master (Col 3, score 120) beats Worker 2 (Col 4, score 90)
        REQUIRE(res.best_move == 3);
        REQUIRE(res.score == 120);
        REQUIRE(res.depth_reached == 6);
    }

    SECTION("Deeper completed worker overrides shallower Master") {
        WorkerPool pool(2);
        SharedSearchState shared;

        pool.worker_data(0).reset(0);
        pool.worker_data(0).best_move = 3;
        pool.worker_data(0).best_score = 200;
        pool.worker_data(0).depth_reached = 6;
        pool.worker_data(0).search_completed = true;

        pool.worker_data(1).reset(1);
        pool.worker_data(1).best_move = 2;
        pool.worker_data(1).best_score = 150;
        pool.worker_data(1).depth_reached = 7; // Completed 1 ply deeper!
        pool.worker_data(1).search_completed = true;

        const SearchResult res = ResultReducer::reduce(pool, shared);
        REQUIRE(res.best_move == 2);
        REQUIRE(res.score == 150);
        REQUIRE(res.depth_reached == 7);
    }

    SECTION("Forced winning mate dominates non-mate and prefers shortest distance-to-mate") {
        WorkerPool pool(3);
        SharedSearchState shared;

        pool.worker_data(0).reset(0);
        pool.worker_data(0).best_move = 3;
        pool.worker_data(0).best_score = 800; // High heuristic, non-mate
        pool.worker_data(0).depth_reached = 8;
        pool.worker_data(0).search_completed = true;

        pool.worker_data(1).reset(1);
        pool.worker_data(1).best_move = 5;
        pool.worker_data(1).best_score = 99'993; // Mate in 7 plies
        pool.worker_data(1).depth_reached = 7;
        pool.worker_data(1).search_completed = true;

        pool.worker_data(2).reset(2);
        pool.worker_data(2).best_move = 1;
        pool.worker_data(2).best_score = 99'997; // Faster mate in 3 plies
        pool.worker_data(2).depth_reached = 5;
        pool.worker_data(2).search_completed = true;

        const SearchResult res = ResultReducer::reduce(pool, shared);
        REQUIRE(res.best_move == 1);
        REQUIRE(res.score == 99'997);
    }

    SECTION("Falls back to SharedSearchState when all workers are interrupted mid-search") {
        WorkerPool pool(2);
        SharedSearchState shared;

        pool.worker_data(0).reset(0);
        pool.worker_data(0).nodes_visited = 300;
        pool.worker_data(0).search_completed = false;

        pool.worker_data(1).reset(1);
        pool.worker_data(1).nodes_visited = 400;
        pool.worker_data(1).search_completed = false;

        shared.update_best(4, 340, 5);

        const SearchResult res = ResultReducer::reduce(pool, shared);
        REQUIRE(res.nodes_visited == 700);
        REQUIRE(res.best_move == 4);
        REQUIRE(res.score == 340);
        REQUIRE(res.depth_reached == 5);
    }
}
