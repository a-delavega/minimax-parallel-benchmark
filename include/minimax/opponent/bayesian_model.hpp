#pragma once

#include "minimax/board.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/opponent/opponent_type.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace mmb {

/**
 * @brief Discrete Bayesian belief updater over adversarial behavioral hypotheses.
 * 
 * Maintains a posterior probability distribution P(theta_k | D) across candidate opponent models:
 * Theta = {OptimalMinimax, Greedy, ShallowHeuristic, HumanNovice, Random}.
 * 
 * Upon observing an opponent move m, updates belief via Bayes' Theorem:
 * P(theta_k | m, B) = [P(m | theta_k, B) * P(theta_k)] / sum_j [P(m | theta_j, B) * P(theta_j)]
 * 
 * Provides marginal predictive distributions over future moves:
 * P(m | B) = sum_k P(m | theta_k, B) * P(theta_k)
 */
class BayesianOpponentModel {
public:
    static constexpr std::size_t NUM_HYPOTHESES = 5;

    static constexpr std::array<OpponentType, NUM_HYPOTHESES> HYPOTHESES = {
        OpponentType::OptimalMinimax,
        OpponentType::Greedy,
        OpponentType::ShallowHeuristic,
        OpponentType::HumanNovice,
        OpponentType::Random
    };

    BayesianOpponentModel() {
        reset();
    }

    void reset() noexcept {
        constexpr double uniform = 1.0 / static_cast<double>(NUM_HYPOTHESES);
        for (std::size_t i = 0; i < NUM_HYPOTHESES; ++i) {
            m_priors[i] = uniform;
        }
        m_moves_observed = 0;
    }

    [[nodiscard]] double belief(OpponentType type) const noexcept {
        const int idx = type_to_index(type);
        return (idx >= 0) ? m_priors[static_cast<std::size_t>(idx)] : 0.0;
    }

    [[nodiscard]] OpponentType map_hypothesis() const noexcept {
        std::size_t best_idx = 0;
        double best_p = m_priors[0];
        for (std::size_t i = 1; i < NUM_HYPOTHESES; ++i) {
            if (m_priors[i] > best_p) {
                best_p = m_priors[i];
                best_idx = i;
            }
        }
        return HYPOTHESES[best_idx];
    }

    [[nodiscard]] uint32_t moves_observed() const noexcept {
        return m_moves_observed;
    }

    /**
     * @brief Computes likelihood P(chosen_move | theta_k, board) for a given hypothesis.
     */
    [[nodiscard]] double compute_likelihood(
        OpponentType hypothesis,
        const Board& before,
        Column chosen_move,
        Player opponent,
        const IEvaluator& evaluator
    ) const noexcept {
        if (!before.is_valid_move(chosen_move)) {
            return 0.0;
        }

        std::array<bool, COLS> legal{};
        int legal_count = 0;
        for (Column c = 0; c < COLS; ++c) {
            legal[static_cast<std::size_t>(c)] = before.is_valid_move(c);
            if (legal[static_cast<std::size_t>(c)]) {
                legal_count++;
            }
        }

        if (legal_count == 0) {
            return 0.0;
        }

        const Player our_player = mmb::opponent(opponent);

        // Check if there was an immediate winning move for opponent
        bool had_win = false;
        bool played_win = false;
        for (Column c = 0; c < COLS; ++c) {
            if (legal[static_cast<std::size_t>(c)] && before.is_winning_move_for(opponent, c)) {
                had_win = true;
                if (c == chosen_move) played_win = true;
            }
        }

        // Check if there was an urgent defensive block needed
        bool had_block = false;
        bool played_block = false;
        for (Column c = 0; c < COLS; ++c) {
            if (legal[static_cast<std::size_t>(c)] && before.is_winning_move_for(our_player, c)) {
                had_block = true;
                if (c == chosen_move) played_block = true;
            }
        }

        switch (hypothesis) {
            case OpponentType::Random: {
                // Uniform likelihood
                return 1.0 / static_cast<double>(legal_count);
            }
            case OpponentType::Greedy: {
                // Greedy player plays win with ~98%, block with ~95%
                if (had_win) return played_win ? 0.98 : 0.02 / std::max(1, legal_count - 1);
                if (had_block) return played_block ? 0.95 : 0.05 / std::max(1, legal_count - 1);
                // Otherwise evaluates 1-ply static evaluation
                std::array<Score, COLS> scores{};
                for (Column c = 0; c < COLS; ++c) {
                    if (legal[static_cast<std::size_t>(c)]) {
                        Board next = before;
                        next.make_move(c);
                        scores[static_cast<std::size_t>(c)] = evaluator.evaluate(next, opponent, 1);
                    }
                }
                const auto probs = OpponentMath::softmax(scores, legal, 60.0);
                return std::max(1e-4, probs[static_cast<std::size_t>(chosen_move)]);
            }
            case OpponentType::HumanNovice: {
                // Misses wins ~40% of the time, misses blocks ~45% of the time
                if (had_win) return played_win ? 0.60 : 0.40 / std::max(1, legal_count - 1);
                if (had_block) return played_block ? 0.55 : 0.45 / std::max(1, legal_count - 1);
                std::array<Score, COLS> scores{};
                for (Column c = 0; c < COLS; ++c) {
                    if (legal[static_cast<std::size_t>(c)]) {
                        Board next = before;
                        next.make_move(c);
                        scores[static_cast<std::size_t>(c)] = evaluator.evaluate(next, opponent, 1);
                    }
                }
                const auto probs = OpponentMath::softmax(scores, legal, 180.0);
                return std::max(1e-4, probs[static_cast<std::size_t>(chosen_move)]);
            }
            case OpponentType::ShallowHeuristic: {
                if (had_win) return played_win ? 0.90 : 0.10 / std::max(1, legal_count - 1);
                if (had_block) return played_block ? 0.85 : 0.15 / std::max(1, legal_count - 1);
                std::array<Score, COLS> scores{};
                for (Column c = 0; c < COLS; ++c) {
                    if (legal[static_cast<std::size_t>(c)]) {
                        Board next = before;
                        next.make_move(c);
                        scores[static_cast<std::size_t>(c)] = evaluator.evaluate(next, opponent, 1);
                    }
                }
                const auto probs = OpponentMath::softmax(scores, legal, 90.0);
                return std::max(1e-4, probs[static_cast<std::size_t>(chosen_move)]);
            }
            case OpponentType::OptimalMinimax: {
                // Optimal player never blunders immediate win or block
                if (had_win) return played_win ? 0.999 : 0.001 / std::max(1, legal_count - 1);
                if (had_block) return played_block ? 0.995 : 0.005 / std::max(1, legal_count - 1);
                // Sharply favors central and tactical columns
                std::array<Score, COLS> scores{};
                for (Column c = 0; c < COLS; ++c) {
                    if (legal[static_cast<std::size_t>(c)]) {
                        Board next = before;
                        next.make_move(c);
                        Score eval = evaluator.evaluate(next, opponent, 1);
                        if (c >= 2 && c <= 4) eval += 50; // Central control
                        scores[static_cast<std::size_t>(c)] = eval;
                    }
                }
                const auto probs = OpponentMath::softmax(scores, legal, 40.0);
                return std::max(1e-4, probs[static_cast<std::size_t>(chosen_move)]);
            }
            default:
                return 1.0 / static_cast<double>(legal_count);
        }
    }

    /**
     * @brief Bayesian Belief Update step: P(theta_k | m) proportional to P(m | theta_k) * P(theta_k).
     */
    void update(
        const Board& before,
        Column chosen_move,
        Player opponent,
        const IEvaluator& evaluator
    ) noexcept {
        if (!before.is_valid_move(chosen_move)) {
            return;
        }

        m_moves_observed++;
        std::array<double, NUM_HYPOTHESES> numerators{};
        double denominator = 0.0;

        for (std::size_t i = 0; i < NUM_HYPOTHESES; ++i) {
            const double likelihood = compute_likelihood(HYPOTHESES[i], before, chosen_move, opponent, evaluator);
            const double prior = m_priors[i];
            const double val = likelihood * prior;
            numerators[i] = val;
            denominator += val;
        }

        if (denominator > 0.0) {
            for (std::size_t i = 0; i < NUM_HYPOTHESES; ++i) {
                // Blend with Laplace smoothing floor to prevent zero-likelihood starvation
                m_priors[i] = std::clamp(numerators[i] / denominator, 0.001, 0.999);
            }
            // Re-normalize
            const double sum = std::accumulate(m_priors.begin(), m_priors.end(), 0.0);
            for (std::size_t i = 0; i < NUM_HYPOTHESES; ++i) {
                m_priors[i] /= sum;
            }
        }
    }

    /**
     * @brief Marginal predictive distribution over candidate moves: P(m | B) = sum_k P(m | theta_k) * P(theta_k).
     */
    [[nodiscard]] std::array<double, COLS> marginal_move_probabilities(
        const Board& board,
        Player opponent,
        const IEvaluator& evaluator
    ) const noexcept {
        std::array<double, COLS> marginals{};
        for (Column c = 0; c < COLS; ++c) {
            if (!board.is_valid_move(c)) {
                continue;
            }
            double p_c = 0.0;
            for (std::size_t k = 0; k < NUM_HYPOTHESES; ++k) {
                const double likelihood = compute_likelihood(HYPOTHESES[k], board, c, opponent, evaluator);
                p_c += likelihood * m_priors[k];
            }
            marginals[static_cast<std::size_t>(c)] = p_c;
        }

        const double sum = std::accumulate(marginals.begin(), marginals.end(), 0.0);
        if (sum > 0.0) {
            for (std::size_t c = 0; c < COLS; ++c) {
                marginals[c] /= sum;
            }
        }
        return marginals;
    }

private:
    [[nodiscard]] static int type_to_index(OpponentType type) noexcept {
        for (std::size_t i = 0; i < NUM_HYPOTHESES; ++i) {
            if (HYPOTHESES[i] == type) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::array<double, NUM_HYPOTHESES> m_priors{};
    uint32_t m_moves_observed = 0;
};

} // namespace mmb
