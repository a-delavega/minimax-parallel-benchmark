#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/search_result.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <random>

namespace mmb {

/**
 * @brief Suboptimal agent that selects uniform random legal moves.
 * 
 * Serves as the maximum-entropy benchmark baseline for opponent profiling and exploitation.
 */
class RandomEngine final : public IEngine {
public:
    explicit RandomEngine(uint64_t seed = 42ULL)
        : m_rng(seed) {}

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = std::max(1, max_depth);

        const auto start = std::chrono::high_resolution_clock::now();

        std::array<Column, COLS> legal_moves{};
        int count = 0;
        for (Column col = 0; col < COLS; ++col) {
            if (board.is_valid_move(col)) {
                legal_moves[static_cast<std::size_t>(count++)] = col;
            }
        }

        if (count > 0) {
            std::uniform_int_distribution<int> dist(0, count - 1);
            result.best_move = legal_moves[static_cast<std::size_t>(dist(m_rng))];
        }

        result.nodes_visited = static_cast<uint64_t>(count);
        const auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "RandomEngine (Uniform Random Legal Moves)";
    }

private:
    std::mt19937_64 m_rng;
};

/**
 * @brief Suboptimal agent that executes greedy 1-ply heuristic evaluation.
 * 
 * Behavior:
 * 1. Executes immediate winning move if present.
 * 2. Blocks immediate opponent winning threat if detected.
 * 3. Otherwise, chooses the move that maximizes static evaluation after 1 ply.
 * Oblivious to multi-ply tactics, traps, or forks.
 */
class GreedyEngine final : public IEngine {
public:
    explicit GreedyEngine(std::shared_ptr<const IEvaluator> evaluator)
        : m_evaluator(std::move(evaluator)) {}

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = 1;

        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return result;
        }

        const auto start = std::chrono::high_resolution_clock::now();
        const Player current = board.current_player;
        const Player opp = opponent(current);

        // 1. Check immediate win
        for (Column c = 0; c < COLS; ++c) {
            if (board.is_valid_move(c) && board.is_winning_move_for(current, c)) {
                result.best_move = c;
                result.score = SCORE_WIN - 1;
                result.nodes_visited = 1;
                const auto end = std::chrono::high_resolution_clock::now();
                result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return result;
            }
        }

        // 2. Check immediate defensive block
        for (Column c = 0; c < COLS; ++c) {
            if (board.is_valid_move(c) && board.is_winning_move_for(opp, c)) {
                result.best_move = c;
                result.score = 0;
                result.nodes_visited = 2;
                const auto end = std::chrono::high_resolution_clock::now();
                result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                return result;
            }
        }

        // 3. 1-ply greedy static evaluation
        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;
        uint64_t nodes = 0;

        std::array<Column, COLS> ordered{};
        const int num_moves = MoveOrderer::get_ordered_moves(board, ordered);

        for (int i = 0; i < num_moves; ++i) {
            const Column c = ordered[static_cast<std::size_t>(i)];
            Board next_board = board;
            next_board.make_move(c);
            nodes++;

            const Score eval = m_evaluator->evaluate(next_board, current, 1);
            if (eval > best_score || best_col == -1) {
                best_score = eval;
                best_col = c;
            }
        }

        const auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        result.best_move = best_col;
        result.score = best_score;
        result.nodes_visited = nodes;
        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "GreedyEngine (1-Ply Heuristic Maximizer)";
    }

private:
    std::shared_ptr<const IEvaluator> m_evaluator;
};

} // namespace mmb
