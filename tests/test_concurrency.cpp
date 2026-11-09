#include <catch2/catch_test_macros.hpp>
#include "minimax/board.hpp"
#include "minimax/concurrency/cache_aligned.hpp"
#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_data.hpp"
#include "minimax/concurrency/worker_pool.hpp"
#include <atomic>
#include <cstdint>
#include <vector>

using namespace mmb;

TEST_CASE("Concurrency: Cache Line Alignment & False Sharing Prevention", "[concurrency][cache]") {
    SECTION("Cache line size is standard 64 bytes") {
        REQUIRE(CACHE_LINE_SIZE == 64);
    }

    SECTION("WorkerData structure is 64-byte aligned and padded") {
        REQUIRE(alignof(WorkerData) >= 64);
        REQUIRE(sizeof(WorkerData) % 64 == 0);
    }

    SECTION("SharedSearchState is 64-byte aligned and padded") {
        REQUIRE(alignof(SharedSearchState) >= 64);
        REQUIRE(sizeof(SharedSearchState) % 64 == 0);
    }

    SECTION("Array elements of WorkerData reside on distinct cache lines") {
        WorkerPool pool(4);
        const uintptr_t addr0 = reinterpret_cast<uintptr_t>(&pool.worker_data(0));
        const uintptr_t addr1 = reinterpret_cast<uintptr_t>(&pool.worker_data(1));
        const uintptr_t addr2 = reinterpret_cast<uintptr_t>(&pool.worker_data(2));
        const uintptr_t addr3 = reinterpret_cast<uintptr_t>(&pool.worker_data(3));

        REQUIRE(addr0 % 64 == 0);
        REQUIRE(addr1 % 64 == 0);
        REQUIRE(addr2 % 64 == 0);
        REQUIRE(addr3 % 64 == 0);

        REQUIRE((addr1 - addr0) >= 64);
        REQUIRE((addr2 - addr1) >= 64);
        REQUIRE((addr3 - addr2) >= 64);
    }
}

TEST_CASE("Concurrency: SharedSearchState Lock-Free Operations", "[concurrency][shared_state]") {
    SharedSearchState shared;

    SECTION("Initial state is clean") {
        REQUIRE_FALSE(shared.should_stop());
        REQUIRE(shared.global_best_move.load() == -1);
        REQUIRE(shared.global_best_score.load() == std::numeric_limits<Score>::lowest());
        REQUIRE(shared.completed_workers.load() == 0);
    }

    SECTION("Stop signaling is atomic and immediate") {
        REQUIRE_FALSE(shared.should_stop());
        shared.request_stop();
        REQUIRE(shared.should_stop());

        shared.reset();
        REQUIRE_FALSE(shared.should_stop());
    }

    SECTION("Concurrent opportunistic update_best chooses the maximum score") {
        shared.reset();

        // Sequential verification
        REQUIRE(shared.update_best(3, 100));
        REQUIRE(shared.global_best_move.load() == 3);
        REQUIRE(shared.global_best_score.load() == 100);

        // Lower score does not overwrite
        REQUIRE_FALSE(shared.update_best(2, 50));
        REQUIRE(shared.global_best_move.load() == 3);
        REQUIRE(shared.global_best_score.load() == 100);

        // Higher score successfully updates
        REQUIRE(shared.update_best(4, 250));
        REQUIRE(shared.global_best_move.load() == 4);
        REQUIRE(shared.global_best_score.load() == 250);
    }
}

TEST_CASE("Concurrency: WorkerPool Execution & Telemetry Aggregation", "[concurrency][pool]") {
    SECTION("WorkerPool initializes with proper thread count") {
        WorkerPool pool;
        REQUIRE(pool.thread_count() >= 1);

        pool.set_thread_count(4);
        REQUIRE(pool.thread_count() == 4);
    }

    SECTION("Parallel execution distributes work without mutex contention") {
        const std::size_t num_threads = 4;
        WorkerPool pool(num_threads);
        SharedSearchState shared;

        const uint64_t iterations_per_thread = 50'000;

        pool.execute([iterations_per_thread](int thread_id, WorkerData& data, SharedSearchState& /*state*/) {
            REQUIRE(data.thread_id == thread_id);
            for (uint64_t i = 0; i < iterations_per_thread; ++i) {
                data.nodes_visited++;
            }
        }, shared);

        REQUIRE(shared.completed_workers.load() == num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            REQUIRE(pool.worker_data(i).nodes_visited == iterations_per_thread);
        }
        REQUIRE(pool.total_nodes_visited() == num_threads * iterations_per_thread);
    }

    SECTION("Master stop signal cleanly terminates long-running helpers") {
        const std::size_t num_threads = 4;
        WorkerPool pool(num_threads);
        SharedSearchState shared;

        pool.execute([](int thread_id, WorkerData& data, SharedSearchState& state) {
            if (thread_id == 0) {
                // Master does small amount of work and completes
                data.nodes_visited = 10;
            } else {
                // Helpers run a long loop until stopped
                while (!state.should_stop()) {
                    data.nodes_visited++;
                    std::this_thread::yield();
                }
            }
        }, shared);

        REQUIRE(shared.completed_workers.load() == num_threads);
        REQUIRE(shared.should_stop());
        REQUIRE(pool.worker_data(0).nodes_visited == 10);
    }

    SECTION("Trivially copyable Board on thread stacks maintains total isolation") {
        WorkerPool pool(4);
        SharedSearchState shared;

        Board root_board;
        root_board.make_move(3); // First move in column 3

        pool.execute([&root_board](int thread_id, WorkerData& data, SharedSearchState& /*state*/) {
            // Each thread works on its own independent local stack copy of Board
            Board local_board = root_board;
            REQUIRE(local_board.moves_played == 1);
            REQUIRE(local_board.bitboard[0] == root_board.bitboard[0]);

            // Thread-specific mutation
            const Column move_col = static_cast<Column>(thread_id);
            local_board.make_move(move_col);

            REQUIRE(local_board.moves_played == 2);
            data.best_move = move_col;
            data.nodes_visited = 1;
        }, shared);

        // Root board is untouched by worker thread mutations
        REQUIRE(root_board.moves_played == 1);
        for (std::size_t i = 0; i < 4; ++i) {
            REQUIRE(pool.worker_data(i).best_move == static_cast<Column>(i));
            REQUIRE(pool.worker_data(i).nodes_visited == 1);
        }
    }
}
