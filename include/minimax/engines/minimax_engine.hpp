#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/search_result.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>

namespace mmb {

/**
 * @brief Pure sequential Minimax search engine without pruning.
 * 
 * Serves as the golden-truth baseline oracle for game-tree exploration.
 * Evaluates the full decision tree up to a given depth:
 * - Quantifies raw recursive branching overhead.
 * - Provides exact reference scores and move selections to mathematically
 *   verify that subsequent optimizations (Alpha-Beta, Lazy SMP) produce identical results.
 */
class MinimaxEngine final : public IEngine {
public:
    explicit MinimaxEngine(std::shared_ptr<const IEvaluator> evaluator)
        : m_evaluator(std::move(evaluator)) {}

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = max_depth;

        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return result;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();

        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;

        const Player pov = board.current_player;

        // Iterate through all columns
        for (Column col = 0; col < COLS; ++col) {
            if (!board.is_valid_move(col)) {
                continue;
            }

            Board next_board = board;
            next_board.make_move(col);
            result.nodes_visited++;

            // Minimizing child step
            const Score score = minimax(next_board, max_depth - 1, 1, false, pov, result.nodes_visited);

            if (score > best_score || best_col == -1) {
                best_score = score;
                best_col = col;
            }
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.best_move = best_col;
        result.score = best_score;

        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "MinimaxEngine (Sequential Full Tree)";
    }

private:
    [[nodiscard]] Score minimax(
        const Board& board, 
        int depth, 
        int ply, 
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

        if (is_maximizing) {
            Score max_eval = std::numeric_limits<Score>::lowest();
            for (Column col = 0; col < COLS; ++col) {
                if (!board.is_valid_move(col)) {
                    continue;
                }

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = minimax(next_board, depth - 1, ply + 1, false, pov, nodes_count);
                max_eval = std::max(max_eval, eval);
            }
            return max_eval;
        } else {
            Score min_eval = std::numeric_limits<Score>::max();
            for (Column col = 0; col < COLS; ++col) {
                if (!board.is_valid_move(col)) {
                    continue;
                }

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = minimax(next_board, depth - 1, ply + 1, true, pov, nodes_count);
                min_eval = std::min(min_eval, eval);
            }
            return min_eval;
        }
    }

    std::shared_ptr<const IEvaluator> m_evaluator;
};

} // namespace mmb
