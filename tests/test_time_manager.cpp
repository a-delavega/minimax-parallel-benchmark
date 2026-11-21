#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/time/time_manager.hpp"
#include <chrono>
#include <memory>
#include <thread>

using namespace mmb;

TEST_CASE("TimeManager: FixedPerMove Budgeting", "[time_manager]") {
    TimeSettings settings;
    settings.mode = TimeMode::FixedPerMove;
    settings.fixed_time_per_move = std::chrono::milliseconds(100);
    settings.safety_buffer = std::chrono::milliseconds(5);

    TimeManager mgr(settings);
    Board board;

    TimeBudget budget = mgr.compute_budget(board);
    REQUIRE(budget.hard_deadline == std::chrono::milliseconds(95));
    REQUIRE(budget.soft_deadline == std::chrono::milliseconds(47));

    mgr.start_turn(board);
    REQUIRE(mgr.current_budget().hard_deadline == std::chrono::milliseconds(95));
    REQUIRE(mgr.current_budget().soft_deadline == std::chrono::milliseconds(47));
    REQUIRE_FALSE(mgr.is_time_up());
    REQUIRE(mgr.remaining().count() > 0);
}

TEST_CASE("TimeManager: Tactical Complexity Heuristics", "[time_manager]") {
    TimeSettings settings;
    settings.mode = TimeMode::DynamicGameClock;
    settings.game_clock_remaining = std::chrono::milliseconds(30'000);
    TimeManager mgr(settings);

    SECTION("Immediate Winning Move yields low complexity factor") {
        Board board;
        // P1 has 3 pieces aligned vertically at col 0, can win on next move
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2

        const double factor = mgr.evaluate_tactical_complexity(board);
        REQUIRE(factor <= 0.4); // Fast forced win
    }

    SECTION("Urgent Defensive Block yields reduced factor") {
        Board board;
        // P2 has 3 pieces at col 0, P1 must block
        board.make_move(1); // P1
        board.make_move(0); // P2
        board.make_move(1); // P1
        board.make_move(0); // P2
        board.make_move(2); // P1
        board.make_move(0); // P2

        const double factor = mgr.evaluate_tactical_complexity(board);
        REQUIRE(factor <= 0.5); // Urgent defense
    }

    SECTION("Midgame opening contest yields higher complexity factor") {
        Board board;
        // 6 opening moves in center
        board.make_move(3);
        board.make_move(3);
        board.make_move(2);
        board.make_move(4);
        board.make_move(2);
        board.make_move(4);

        const double factor = mgr.evaluate_tactical_complexity(board);
        REQUIRE(factor >= 1.2);
    }
}

TEST_CASE("TimeManager: Depth Progression Decision", "[time_manager]") {
    TimeSettings settings;
    settings.mode = TimeMode::FixedPerMove;
    settings.fixed_time_per_move = std::chrono::milliseconds(100);
    settings.safety_buffer = std::chrono::milliseconds(5);
    settings.default_ebf = 3.0;

    TimeManager mgr(settings);
    Board board;
    mgr.start_turn(board);

    SECTION("Always allows starting depth 1") {
        REQUIRE(mgr.should_start_next_depth(0, std::chrono::microseconds(100)));
        REQUIRE(mgr.should_start_next_depth(1, std::chrono::microseconds(100)));
    }

    SECTION("Rejects depth progression if last iteration projected exceeds hard deadline") {
        // If depth 3 took 35ms and EBF=3.0, next depth projected = 105ms > 95ms hard deadline
        const auto last_iter = std::chrono::microseconds(35'000);
        const bool allow = mgr.should_start_next_depth(3, last_iter);
        REQUIRE_FALSE(allow);
    }
}

TEST_CASE("TimeManager: Game Clock Deduction & Increment", "[time_manager]") {
    TimeSettings settings;
    settings.mode = TimeMode::DynamicGameClock;
    settings.game_clock_remaining = std::chrono::milliseconds(10'000);
    settings.time_increment = std::chrono::milliseconds(200);

    TimeManager mgr(settings);
    REQUIRE(mgr.game_clock() == std::chrono::milliseconds(10'000));

    // Turn took 500ms
    mgr.complete_turn(std::chrono::microseconds(500'000));

    // Remaining: 10,000 - 500 + 200 = 9,700ms
    REQUIRE(mgr.game_clock() == std::chrono::milliseconds(9'700));
}

TEST_CASE("Engine: search_timed Integration", "[time_manager]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    AlphaBetaEngine ab_engine(eval);
    LazySmpEngine smp_engine(eval, 2);

    Board board;
    board.make_move(3);
    board.make_move(3);

    TimeSettings settings;
    settings.mode = TimeMode::FixedPerMove;
    settings.fixed_time_per_move = std::chrono::milliseconds(50);
    settings.safety_buffer = std::chrono::milliseconds(5);

    TimeManager mgr(settings);

    SECTION("AlphaBetaEngine respects time budget") {
        const auto start = std::chrono::high_resolution_clock::now();
        SearchResult res = ab_engine.search_timed(board, mgr, 20);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(res.best_move >= 0);
        REQUIRE(res.best_move < COLS);
        REQUIRE(res.depth_reached >= 2);
        // Ensure engine finished within reasonable tolerance of budget
        REQUIRE(elapsed.count() <= 120);
    }

    SECTION("LazySmpEngine respects time budget") {
        const auto start = std::chrono::high_resolution_clock::now();
        SearchResult res = smp_engine.search_timed(board, mgr, 20);
        const auto end = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(res.best_move >= 0);
        REQUIRE(res.best_move < COLS);
        REQUIRE(res.depth_reached >= 2);
        REQUIRE(elapsed.count() <= 120);
    }
}
