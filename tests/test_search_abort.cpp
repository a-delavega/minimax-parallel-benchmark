#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/time/time_manager.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace mmb;

TEST_CASE("Cooperative Abort: AlphaBetaEngine throws SearchAbortedException during bounded search", "[abort][engine]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine engine(eval);
    Board board;

    std::atomic<bool> stop_flag{true};
    engine.set_stop_flag(&stop_flag);

    REQUIRE_THROWS_AS(
        engine.search_bounded(board, 12, -10'000, 10'000),
        SearchAbortedException
    );

    engine.set_stop_flag(nullptr);
}

TEST_CASE("Cooperative Abort: AlphaBetaEngine preserves completed depth in search_iterative", "[abort][engine]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine engine(eval);
    Board board;

    std::atomic<bool> stop_flag{false};
    engine.set_stop_flag(&stop_flag);

    // Launch a background thread that triggers stop after 15ms
    std::jthread stopper([&stop_flag] {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        stop_flag.store(true, std::memory_order_relaxed);
    });

    // Start deep iterative search (depth 30 would take minutes without stop)
    SearchResult result = engine.search_iterative(board, 30);

    REQUIRE(result.depth_reached >= 1);
    REQUIRE(result.depth_reached < 30);
    REQUIRE(result.best_move >= 0);
    REQUIRE(result.best_move < COLS);
    REQUIRE(board.is_valid_move(result.best_move));

    engine.set_stop_flag(nullptr);
}

TEST_CASE("Cooperative Abort: LazySmpEngine stops across all worker threads", "[abort][lazy_smp]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    auto tt = std::make_shared<TranspositionTable>(1); // 1 MB shared TT
    LazySmpEngine engine(eval, 4, tt);
    Board board;

    std::atomic<bool> stop_flag{false};
    engine.set_stop_flag(&stop_flag);

    // Trigger stop after 20ms
    std::jthread stopper([&stop_flag] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        stop_flag.store(true, std::memory_order_relaxed);
    });

    SearchResult result = engine.search_iterative(board, 30);

    REQUIRE(result.depth_reached >= 1);
    REQUIRE(result.depth_reached < 30);
    REQUIRE(result.best_move >= 0);
    REQUIRE(result.best_move < COLS);
    REQUIRE(board.is_valid_move(result.best_move));

    engine.set_stop_flag(nullptr);
}

TEST_CASE("Cooperative Abort: search_timed terminates within time budget", "[abort][timed]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    Board board;

    SECTION("AlphaBetaEngine with 30ms budget") {
        AlphaBetaEngine engine(eval);
        TimeSettings settings;
        settings.mode = TimeMode::FixedPerMove;
        settings.fixed_time_per_move = std::chrono::milliseconds(30);
        settings.safety_buffer = std::chrono::milliseconds(5);
        TimeManager tm(settings);

        const auto start = std::chrono::high_resolution_clock::now();
        SearchResult result = engine.search_timed(board, tm, 35);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start
        );

        REQUIRE(result.depth_reached >= 1);
        REQUIRE(result.best_move >= 0);
        REQUIRE(result.best_move < COLS);
        // Ensure engine stopped in a timely manner (allowing generous margin for OS scheduling)
        REQUIRE(elapsed.count() <= 100);
    }

    SECTION("LazySmpEngine (4 threads) with 30ms budget") {
        auto tt = std::make_shared<TranspositionTable>(1);
        LazySmpEngine engine(eval, 4, tt);
        TimeSettings settings;
        settings.mode = TimeMode::FixedPerMove;
        settings.fixed_time_per_move = std::chrono::milliseconds(30);
        settings.safety_buffer = std::chrono::milliseconds(5);
        TimeManager tm(settings);

        const auto start = std::chrono::high_resolution_clock::now();
        SearchResult result = engine.search_timed(board, tm, 35);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start
        );

        REQUIRE(result.depth_reached >= 1);
        REQUIRE(result.best_move >= 0);
        REQUIRE(result.best_move < COLS);
        REQUIRE(elapsed.count() <= 100);
    }
}
