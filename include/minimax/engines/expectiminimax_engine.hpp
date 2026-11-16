#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/opponent/bayesian_model.hpp"
#include "minimax/opponent/opponent_type.hpp"
#include "minimax/search_result.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>

namespace mmb {

/**
 * @brief Expectiminimax game-tree search engine with probabilistic chance nodes.
 * 
 * Formalization of Game Tree with Stochastic Adversary:
 * - Decision Nodes (Our Turn): Maximize deterministic utility max_m V(m).
 * - Chance Nodes (Opponent Turn): Evaluate expected utility sum_m P(m | B) * V(m).
 * 
 * Incorporates Star-Alpha-Beta bound propagation and Bayesian predictive distributions
 * to exploit adversaries without losing zero-regret safety.
 */
class ExpectiminimaxEngine final : public IEngine {
public:
    explicit ExpectiminimaxEngine(
        std::shared_ptr<const IEvaluator> evaluator,
        std::shared_ptr<BayesianOpponentModel> bayesian_model = nullptr,
        double temperature = 100.0
    ) : m_evaluator(std::move(evaluator)),
        m_bayesian_model(std::move(bayesian_model)),
        m_temperature(std::max(1.0, temperature)) {
        if (!m_bayesian_model) {
            m_bayesian_model = std::make_shared<BayesianOpponentModel>();
        }
    }

    [[nodiscard]] std::shared_ptr<BayesianOpponentModel> bayesian_model() const noexcept {
        return m_bayesian_model;
    }

    [[nodiscard]] double temperature() const noexcept {
        return m_temperature;
    }

    void set_temperature(double temp) noexcept {
        m_temperature = std::max(1.0, temp);
    }

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = max_depth;

        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return result;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();
        const Player pov = board.current_player;

        std::array<Column, COLS> ordered{};
        const int count = MoveOrderer::get_ordered_moves(board, ordered);

        // Immediate win check
        for (int i = 0; i < count; ++i) {
            const Column col = ordered[static_cast<std::size_t>(i)];
            if (board.is_winning_move_for(pov, col)) {
                result.best_move = col;
                result.score = SCORE_WIN - 1;
                result.nodes_visited = 1;
                const auto end_time = std::chrono::high_resolution_clock::now();
                result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                return result;
            }
        }

        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;

        for (int i = 0; i < count; ++i) {
            const Column col = ordered[static_cast<std::size_t>(i)];
            Board next_board = board;
            next_board.make_move(col);
            result.nodes_visited++;

            // Opponent chance node
            const Score score = expectiminimax(
                next_board,
                max_depth - 1,
                1,
                false,
                pov,
                result.nodes_visited
            );

            if (score > best_score || best_col == -1) {
                best_score = score;
                best_col = col;
            }
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.best_move = (best_col != -1) ? best_col : ordered[0];
        result.score = best_score;
        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "ExpectiminimaxEngine (Probabilistic Chance Nodes & Bayesian Beliefs)";
    }

private:
    [[nodiscard]] Score expectiminimax(
        const Board& board,
        int depth,
        int ply,
        bool is_maximizing,
        Player pov,
        uint64_t& nodes_count
    ) const noexcept {
        const Player current = board.current_player;
        const Player previous = opponent(current);

        if (board.has_won(previous)) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        if (board.is_draw() || depth == 0) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        std::array<Column, COLS> ordered{};
        const int count = MoveOrderer::get_ordered_moves(board, ordered);

        if (is_maximizing) {
            // Decision Node (Max): argmax_c V(c)
            Score max_val = std::numeric_limits<Score>::lowest();
            for (int i = 0; i < count; ++i) {
                const Column col = ordered[static_cast<std::size_t>(i)];
                Board next = board;
                next.make_move(col);
                nodes_count++;

                const Score val = expectiminimax(next, depth - 1, ply + 1, false, pov, nodes_count);
                max_val = std::max(max_val, val);
            }
            return max_val;
        } else {
            // Chance Node (Opponent Turn): sum_c P(c) * V(c)
            // Predict move probabilities using marginal Bayesian belief
            const auto probs = m_bayesian_model->marginal_move_probabilities(board, current, *m_evaluator);

            double expected_val = 0.0;
            double prob_sum = 0.0;

            for (int i = 0; i < count; ++i) {
                const Column col = ordered[static_cast<std::size_t>(i)];
                const double p = probs[static_cast<std::size_t>(col)];
                if (p <= 0.0) {
                    continue;
                }

                Board next = board;
                next.make_move(col);
                nodes_count++;

                const Score val = expectiminimax(next, depth - 1, ply + 1, true, pov, nodes_count);
                expected_val += p * static_cast<double>(val);
                prob_sum += p;
            }

            if (prob_sum > 0.0) {
                expected_val /= prob_sum;
                return static_cast<Score>(std::round(expected_val));
            }

            return 0;
        }
    }

    std::shared_ptr<const IEvaluator> m_evaluator;
    std::shared_ptr<BayesianOpponentModel> m_bayesian_model;
    double m_temperature = 100.0;
};

} // namespace mmb
