#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/search_result.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>

namespace mmb {

/**
 * @brief Sequential Alpha-Beta pruning engine with static center-outward move ordering.
 * 
 * Explores game-tree branches while maintaining [alpha, beta] bounds:
 * - alpha: Lower bound score guaranteed for the maximizing player.
 * - beta: Upper bound score guaranteed for the minimizing player.
 * 
 * Integrates MoveOrderer to evaluate high-potential central columns first,
 * triggering earlier beta-cutoffs and dramatically reducing explored nodes
 * from O(b^d) down towards O(b^(d/2)).
 */
class AlphaBetaEngine final : public IEngine {
public:
    explicit AlphaBetaEngine(std::shared_ptr<const IEvaluator> evaluator)
        : m_evaluator(std::move(evaluator)) {}

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = max_depth;

        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return result;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();

        Score alpha = std::numeric_limits<Score>::lowest() + 1;
        const Score beta = std::numeric_limits<Score>::max() - 1;

        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;

        const Player pov = board.current_player;

        // Order root candidate moves to test center columns first
        std::array<Column, COLS> ordered_moves{};
        const int num_moves = MoveOrderer::get_ordered_moves(board, ordered_moves);

        for (int i = 0; i < num_moves; ++i) {
            const Column col = ordered_moves[i];

            Board next_board = board;
            next_board.make_move(col);
            result.nodes_visited++;

            // Minimizing child step
            const Score score = alphabeta(
                next_board, 
                max_depth - 1, 
                1, 
                alpha, 
                beta, 
                false, 
                pov, 
                result.nodes_visited
            );

            if (score > best_score || best_col == -1) {
                best_score = score;
                best_col = col;
            }

            alpha = std::max(alpha, best_score);
            if (alpha >= beta) {
                break; // Root cutoff (unlikely with beta=+inf, but formally sound)
            }
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.best_move = best_col;
        result.score = best_score;

        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "AlphaBetaEngine (Sequential Alpha-Beta with Move Ordering)";
    }

private:
    [[nodiscard]] Score alphabeta(
        const Board& board, 
        int depth, 
        int ply, 
        Score alpha, 
        Score beta, 
        bool is_maximizing, 
        Player pov, 
        uint64_t& nodes_count
    ) const noexcept {
        const Player current = board.current_player;
        const Player previous = opponent(current);

        // Terminal condition: previous player just won
        if (board.has_won(previous)) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        // Terminal condition: draw or maximum depth reached
        if (board.is_draw() || depth == 0) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        std::array<Column, COLS> ordered_moves{};
        const int num_moves = MoveOrderer::get_ordered_moves(board, ordered_moves);

        if (is_maximizing) {
            Score max_eval = std::numeric_limits<Score>::lowest();

            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[i];

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = alphabeta(
                    next_board, 
                    depth - 1, 
                    ply + 1, 
                    alpha, 
                    beta, 
                    false, 
                    pov, 
                    nodes_count
                );

                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);

                if (beta <= alpha) {
                    break; // Beta cutoff: Minimizer will avoid this branch
                }
            }
            return max_eval;
        } else {
            Score min_eval = std::numeric_limits<Score>::max();

            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[i];

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = alphabeta(
                    next_board, 
                    depth - 1, 
                    ply + 1, 
                    alpha, 
                    beta, 
                    true, 
                    pov, 
                    nodes_count
                );

                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);

                if (beta <= alpha) {
                    break; // Alpha cutoff: Maximizer will avoid this branch
                }
            }
            return min_eval;
        }
    }

    std::shared_ptr<const IEvaluator> m_evaluator;
};

} // namespace mmb
