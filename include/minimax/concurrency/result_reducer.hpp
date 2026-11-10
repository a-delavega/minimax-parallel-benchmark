#pragma once

#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_data.hpp"
#include "minimax/concurrency/worker_pool.hpp"
#include "minimax/search_result.hpp"
#include "minimax/types.hpp"
#include <chrono>
#include <cstdint>
#include <limits>

namespace mmb {

/**
 * @brief Deterministic lock-free aggregator for multi-worker Lazy SMP search results.
 * 
 * Combines telemetry across all parallel threads and applies strict game-theoretic
 * dominance rules to pick the authoritative best move:
 * 1. Winning forced mates (Score >= MATE_THRESHOLD) dominate heuristic estimates,
 *    preferring the shortest distance-to-mate.
 * 2. Greater verified search depth dominates shallower completed searches.
 * 3. Equal-depth ties break by highest score, then lowest worker ID (Master preference).
 */
class ResultReducer {
public:
    static constexpr Score MATE_THRESHOLD = 90'000;

    /**
     * @brief Aggregate worker results and shared atomic state into a unified SearchResult.
     * @param pool Worker pool containing per-thread aligned telemetry.
     * @param shared Shared atomic search state.
     * @param elapsed Wall-clock duration of the parallel search.
     * @return Consolidated SearchResult.
     */
    [[nodiscard]] static SearchResult reduce(
        const WorkerPool& pool,
        const SharedSearchState& shared,
        std::chrono::microseconds elapsed = std::chrono::microseconds{0}
    ) noexcept {
        SearchResult final_res;
        final_res.duration = elapsed;
        final_res.nodes_visited = pool.total_nodes_visited();

        const std::size_t n = pool.thread_count();
        int best_worker_id = -1;
        Score best_score = std::numeric_limits<Score>::lowest();
        int best_depth = -1;
        Column best_move = -1;

        for (std::size_t i = 0; i < n; ++i) {
            const WorkerData& w = pool.worker_data(i);
            if (!w.search_completed || w.best_move == -1) {
                continue;
            }

            if (best_worker_id == -1 ||
                is_better(w.best_score, w.depth_reached, w.thread_id,
                          best_score, best_depth, best_worker_id)) {
                best_worker_id = w.thread_id;
                best_score = w.best_score;
                best_depth = w.depth_reached;
                best_move = w.best_move;
            }
        }

        // Check if SharedSearchState discovered an even stronger/deeper result (e.g. forced mate or aborted run)
        const UnpackedBest shared_best = shared.read_best();
        if (best_worker_id == -1) {
            if (shared_best.valid && shared_best.move != -1) {
                final_res.best_move = shared_best.move;
                final_res.score = shared_best.score;
                final_res.depth_reached = shared_best.depth;
            } else if (n > 0 && pool.worker_data(0).best_move != -1) {
                const WorkerData& master = pool.worker_data(0);
                final_res.best_move = master.best_move;
                final_res.score = master.best_score;
                final_res.depth_reached = master.depth_reached;
            }
            return final_res;
        }

        // If shared atomic scoreboard recorded a deeper or winning-mate move, adopt it
        if (shared_best.valid && shared_best.move != -1) {
            if (is_better(shared_best.score, shared_best.depth, std::numeric_limits<int>::max(),
                          best_score, best_depth, best_worker_id)) {
                best_move = shared_best.move;
                best_score = shared_best.score;
                best_depth = shared_best.depth;
            }
        }

        final_res.best_move = best_move;
        final_res.score = best_score;
        final_res.depth_reached = best_depth;
        return final_res;
    }

    /**
     * @brief Compare candidate result A against current best B.
     */
    [[nodiscard]] static constexpr bool is_better(
        Score score_a, int depth_a, int id_a,
        Score score_b, int depth_b, int id_b
    ) noexcept {
        const bool a_is_win = (score_a >= MATE_THRESHOLD);
        const bool b_is_win = (score_b >= MATE_THRESHOLD);

        // 1. Winning forced mates dominate non-mates; shorter mate wins
        if (a_is_win != b_is_win) {
            return a_is_win;
        }
        if (a_is_win && b_is_win) {
            if (score_a != score_b) {
                return score_a > score_b;
            }
            return id_a < id_b;
        }

        // 2. Deeper verified search beats shallower search
        if (depth_a != depth_b) {
            return depth_a > depth_b;
        }

        // 3. Higher score wins at identical depth
        if (score_a != score_b) {
            return score_a > score_b;
        }

        // 4. Deterministic Master tie-breaker
        return id_a < id_b;
    }
};

} // namespace mmb
