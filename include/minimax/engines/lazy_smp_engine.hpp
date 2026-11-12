#pragma once

#include "minimax/board.hpp"
#include "minimax/concurrency/result_reducer.hpp"
#include "minimax/concurrency/search_diversifier.hpp"
#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_data.hpp"
#include "minimax/concurrency/worker_pool.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/search_result.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace mmb {

/**
 * @brief Multi-threaded Lazy SMP (Symmetric Multiprocessing) game-tree search engine.
 * 
 * Features:
 * 1. Zero-Mutex Hot Path: Threads never block on mutexes during recursive Alpha-Beta traversal.
 * 2. L1 Cache-Line Isolation: Per-worker state is 64-byte aligned (alignas(64)) to prevent False Sharing.
 * 3. Thread Asymmetry & Diversification: Uses SearchDiversifier to vary search depth, aspiration
 *    deltas, and shallow candidate move orders across helpers, preventing work duplication.
 * 4. Lock-Free Indivisible CAS: Uses SharedSearchState single 64-bit word atomic updates.
 * 5. Deterministic Aggregation: ResultReducer applies game-theoretic dominance rules to select
 *    the globally optimal best move while summing true parallel throughput.
 */
class LazySmpEngine final : public IEngine {
public:
    explicit LazySmpEngine(
        std::shared_ptr<const IEvaluator> evaluator,
        std::size_t num_threads = WorkerPool::default_thread_count()
    ) : m_evaluator(std::move(evaluator)),
        m_pool(num_threads) {}

    [[nodiscard]] std::size_t thread_count() const noexcept {
        return m_pool.thread_count();
    }

    void set_thread_count(std::size_t count) {
        m_pool.set_thread_count(count);
    }

    [[nodiscard]] const WorkerPool& worker_pool() const noexcept {
        return m_pool;
    }

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult quick_result;
        quick_result.depth_reached = max_depth;

        // Terminal or shallow check
        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            quick_result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return quick_result;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();

        // Dispatch parallel Lazy SMP search across worker threads
        m_pool.execute([this, &board, max_depth](int thread_id, WorkerData& data, SharedSearchState& shared) {
            run_worker(thread_id, board, max_depth, data, shared);
        }, m_shared_state);

        const auto end_time = std::chrono::high_resolution_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        // Aggregate results and telemetry across all threads
        SearchResult result = ResultReducer::reduce(m_pool, m_shared_state, elapsed);
        if (result.depth_reached <= 0) {
            result.depth_reached = max_depth;
        }
        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "LazySmpEngine (Parallel Alpha-Beta with Thread Asymmetry)";
    }

private:
    void run_worker(
        int thread_id,
        const Board& root_board,
        int base_depth,
        WorkerData& data,
        SharedSearchState& shared
    ) const noexcept {
        const ThreadSearchConfig cfg = SearchDiversifier::configure(thread_id, base_depth);
        const Player pov = root_board.current_player;

        Score alpha = std::numeric_limits<Score>::lowest() + 1;
        const Score beta = std::numeric_limits<Score>::max() - 1;

        std::array<Column, COLS> ordered_moves{};
        const int num_moves = SearchDiversifier::get_diversified_moves(root_board, thread_id, 0, ordered_moves);

        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;

        for (int i = 0; i < num_moves; ++i) {
            if (shared.should_stop()) {
                break;
            }

            const Column col = ordered_moves[static_cast<std::size_t>(i)];
            Board next_board = root_board;
            next_board.make_move(col);
            data.nodes_visited++;

            const Score score = alphabeta(
                next_board,
                cfg.target_depth - 1,
                1,
                alpha,
                beta,
                false,
                pov,
                thread_id,
                data,
                shared
            );

            if (shared.should_stop()) {
                break;
            }

            if (score > best_score || best_col == -1) {
                best_score = score;
                best_col = col;
                data.best_move = col;
                data.best_score = score;
                data.depth_reached = cfg.target_depth;
                shared.update_best(col, score, cfg.target_depth);
            }

            alpha = std::max(alpha, best_score);
            if (alpha >= beta) {
                data.beta_cutoffs++;
                break;
            }
        }

        if (!shared.should_stop() && best_col != -1) {
            data.search_completed = true;
        }
    }

    [[nodiscard]] Score alphabeta(
        const Board& board,
        int depth,
        int ply,
        Score alpha,
        Score beta,
        bool is_maximizing,
        Player pov,
        int thread_id,
        WorkerData& data,
        SharedSearchState& shared
    ) const noexcept {
        // Fast periodic poll of the lock-free stop signal
        if ((data.nodes_visited & 1023ULL) == 0 && shared.should_stop()) {
            return 0;
        }

        const Player current = board.current_player;
        const Player previous = opponent(current);

        if (board.has_won(previous)) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        if (board.is_draw() || depth == 0) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        std::array<Column, COLS> ordered_moves{};
        const int num_moves = SearchDiversifier::get_diversified_moves(board, thread_id, ply, ordered_moves);

        if (is_maximizing) {
            Score max_eval = std::numeric_limits<Score>::lowest();
            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[static_cast<std::size_t>(i)];
                Board next_board = board;
                next_board.make_move(col);
                data.nodes_visited++;

                const Score eval = alphabeta(
                    next_board, depth - 1, ply + 1, alpha, beta, false, pov, thread_id, data, shared
                );
                if (shared.should_stop()) {
                    return 0;
                }

                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);

                if (beta <= alpha) {
                    data.beta_cutoffs++;
                    break;
                }
            }
            return max_eval;
        } else {
            Score min_eval = std::numeric_limits<Score>::max();
            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[static_cast<std::size_t>(i)];
                Board next_board = board;
                next_board.make_move(col);
                data.nodes_visited++;

                const Score eval = alphabeta(
                    next_board, depth - 1, ply + 1, alpha, beta, true, pov, thread_id, data, shared
                );
                if (shared.should_stop()) {
                    return 0;
                }

                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);

                if (beta <= alpha) {
                    data.beta_cutoffs++;
                    break;
                }
            }
            return min_eval;
        }
    }

    std::shared_ptr<const IEvaluator> m_evaluator;
    WorkerPool m_pool;
    SharedSearchState m_shared_state;
};

} // namespace mmb
