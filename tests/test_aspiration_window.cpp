#include <catch2/catch_test_macros.hpp>
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/search/aspiration_window.hpp"
#include <memory>

using namespace mmb;

TEST_CASE("AspirationSearcher: Unit Logic & Recovery", "[aspiration_window]") {
    AspirationConfig cfg;
    cfg.initial_delta = 50;
    cfg.min_depth = 2;
    cfg.max_widening_attempts = 3;
    cfg.widening_factor = 2.0;

    AspirationStats stats;

    SECTION("Window Hit: Score lies strictly within window") {
        int call_count = 0;
        const Score true_score = 110;
        const Score prev_score = 100; // Window: [50, 150]

        SearchResult res = AspirationSearcher::search(
            3,
            prev_score,
            [&](int depth, Score alpha, Score beta) {
                call_count++;
                REQUIRE(depth == 3);
                REQUIRE(alpha == 50);
                REQUIRE(beta == 150);
                SearchResult r;
                r.score = true_score;
                r.best_move = 3;
                return r;
            },
            cfg,
            &stats
        );

        REQUIRE(call_count == 1);
        REQUIRE(res.score == true_score);
        REQUIRE(res.best_move == 3);
        REQUIRE(stats.window_hits == 1);
        REQUIRE(stats.fail_lows == 0);
        REQUIRE(stats.fail_highs == 0);
        REQUIRE(stats.re_searches == 0);
        REQUIRE(stats.hit_rate() == 100.0);
    }

    SECTION("Fail-Low Recovery: Score drops below alpha then converges on widening") {
        int call_count = 0;
        const Score prev_score = 100; // Initial window: [50, 150]
        const Score true_score = 30;  // Outside initial window (fail-low)

        SearchResult res = AspirationSearcher::search(
            3,
            prev_score,
            [&](int, Score alpha, Score) {
                call_count++;
                SearchResult r;
                if (call_count == 1) {
                    // Fail-low: returns alpha bound or worse
                    r.score = alpha;
                } else {
                    // Widened window covers true_score (30)
                    r.score = true_score;
                }
                r.best_move = 2;
                return r;
            },
            cfg,
            &stats
        );

        REQUIRE(call_count == 2);
        REQUIRE(res.score == true_score);
        REQUIRE(stats.fail_lows == 1);
        REQUIRE(stats.re_searches == 1);
        REQUIRE(stats.window_hits == 1);
    }

    SECTION("Fail-High Recovery: Score exceeds beta then converges on widening") {
        int call_count = 0;
        const Score prev_score = 100; // Initial window: [50, 150]
        const Score true_score = 180; // Outside initial window (fail-high)

        SearchResult res = AspirationSearcher::search(
            3,
            prev_score,
            [&](int, Score, Score beta) {
                call_count++;
                SearchResult r;
                if (call_count == 1) {
                    // Fail-high: returns beta bound or better
                    r.score = beta;
                } else {
                    // Widened window covers true_score (180)
                    r.score = true_score;
                }
                r.best_move = 4;
                return r;
            },
            cfg,
            &stats
        );

        REQUIRE(call_count == 2);
        REQUIRE(res.score == true_score);
        REQUIRE(stats.fail_highs == 1);
        REQUIRE(stats.re_searches == 1);
        REQUIRE(stats.window_hits == 1);
    }

    SECTION("Terminal Mate Scores Bypass Aspiration Window") {
        int call_count = 0;
        const Score mate_score = SCORE_WIN - 5; // Tactical mate

        SearchResult res = AspirationSearcher::search(
            4,
            mate_score,
            [&](int, Score alpha, Score beta) {
                call_count++;
                // Full window must be used
                REQUIRE(alpha <= std::numeric_limits<Score>::lowest() + 1);
                REQUIRE(beta >= std::numeric_limits<Score>::max() - 1);
                SearchResult r;
                r.score = mate_score;
                r.best_move = 3;
                return r;
            },
            cfg,
            &stats
        );

        REQUIRE(call_count == 1);
        REQUIRE(res.score == mate_score);
    }
}

TEST_CASE("AlphaBetaEngine: Aspiration Window Integration & Equivalence", "[aspiration_window]") {
    auto eval = std::make_shared<ComplexEvaluator>();
    AlphaBetaEngine engine_aspiration(eval);
    AlphaBetaEngine engine_full(eval);

    engine_aspiration.set_aspiration_window(true, 50);
    engine_full.set_aspiration_window(false);

    Board board;
    board.make_move(3);
    board.make_move(3);
    board.make_move(2);
    board.make_move(4);

    const int test_depth = 5;
    SearchResult res_asp = engine_aspiration.search_iterative(board, test_depth);
    SearchResult res_full = engine_full.search_iterative(board, test_depth);

    SECTION("Produces identical best move and score as full window search") {
        REQUIRE(res_asp.best_move == res_full.best_move);
        REQUIRE(res_asp.score == res_full.score);
        REQUIRE(res_asp.depth_reached == test_depth);
    }

    SECTION("Telemetry tracks window hits and efficiency") {
        engine_aspiration.clear_aspiration_stats();
        (void)engine_aspiration.search_iterative(board, test_depth);
        const auto& stats = engine_aspiration.aspiration_stats();
        REQUIRE(stats.total_probes() > 0);
        REQUIRE(stats.window_hits > 0);
        REQUIRE(stats.hit_rate() > 0.0);
    }
}

TEST_CASE("LazySmpEngine: Multi-Threaded Aspiration Search", "[aspiration_window]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    LazySmpEngine engine(eval, 2);

    engine.set_aspiration_window(true, 50);
    REQUIRE(engine.aspiration_window_enabled());

    Board board;
    board.make_move(3);
    board.make_move(3);

    SearchResult res = engine.search_iterative(board, 4);

    REQUIRE(res.best_move >= 0);
    REQUIRE(res.best_move < COLS);
    REQUIRE(res.depth_reached == 4);
    REQUIRE(engine.aspiration_stats().window_hits > 0);
}
