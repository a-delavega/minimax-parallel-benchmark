#pragma once

#include "minimax/types.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace mmb {

/**
 * @brief Categorization of adversarial strategic behaviors in zero-sum games.
 */
enum class OpponentType : uint8_t {
    OptimalMinimax,   ///< Game-theoretically rational adversary (assumes zero blunders)
    Greedy,           ///< 1-ply greedy heuristic maximizer (susceptible to multi-ply traps)
    ShallowHeuristic, ///< 2-3 ply depth-limited searcher (intermediate tactical awareness)
    Random,           ///< Uniform random legal move selector (maximum entropy)
    HumanNovice,      ///< High error rate (~35%), oblivious to forks/double-threats
    HumanIntermediate ///< Moderate error rate (~15%), detects 1-ply wins but misses 3-ply traps
};

/**
 * @brief Quantitative parametric model of an opponent's decision tendencies.
 */
struct OpponentProfile {
    OpponentType type = OpponentType::OptimalMinimax;
    double blunder_rate = 0.0;        ///< Empirical probability [0.0, 1.0] of committing a tactical error
    int estimated_depth = 8;          ///< Inferred search horizon in plies
    double center_bias = 0.5;         ///< Propensity [0.0, 1.0] to favor central columns (cols 2, 3, 4)
    double tactical_awareness = 1.0;  ///< Sensitivity [0.0, 1.0] to detecting opponent forks/multi-threats

    [[nodiscard]] static constexpr OpponentProfile preset_optimal() noexcept {
        return OpponentProfile{
            .type = OpponentType::OptimalMinimax,
            .blunder_rate = 0.0,
            .estimated_depth = 12,
            .center_bias = 0.7,
            .tactical_awareness = 1.0
        };
    }

    [[nodiscard]] static constexpr OpponentProfile preset_greedy() noexcept {
        return OpponentProfile{
            .type = OpponentType::Greedy,
            .blunder_rate = 0.25,
            .estimated_depth = 1,
            .center_bias = 0.5,
            .tactical_awareness = 0.4
        };
    }

    [[nodiscard]] static constexpr OpponentProfile preset_random() noexcept {
        return OpponentProfile{
            .type = OpponentType::Random,
            .blunder_rate = 0.85,
            .estimated_depth = 0,
            .center_bias = 0.0,
            .tactical_awareness = 0.0
        };
    }

    [[nodiscard]] static constexpr OpponentProfile preset_novice() noexcept {
        return OpponentProfile{
            .type = OpponentType::HumanNovice,
            .blunder_rate = 0.40,
            .estimated_depth = 2,
            .center_bias = 0.4,
            .tactical_awareness = 0.2
        };
    }

    [[nodiscard]] static constexpr OpponentProfile preset_intermediate() noexcept {
        return OpponentProfile{
            .type = OpponentType::HumanIntermediate,
            .blunder_rate = 0.15,
            .estimated_depth = 4,
            .center_bias = 0.6,
            .tactical_awareness = 0.7
        };
    }

    [[nodiscard]] std::string to_string() const {
        switch (type) {
            case OpponentType::OptimalMinimax:   return "OptimalMinimax";
            case OpponentType::Greedy:           return "Greedy (1-Ply)";
            case OpponentType::ShallowHeuristic: return "ShallowHeuristic";
            case OpponentType::Random:           return "Random (Uniform)";
            case OpponentType::HumanNovice:      return "HumanNovice";
            case OpponentType::HumanIntermediate:return "HumanIntermediate";
        }
        return "Unknown";
    }
};

/**
 * @brief Mathematical utilities for stochastic game-theoretic utility and decision modeling.
 */
class OpponentMath {
public:
    /**
     * @brief Computes Boltzmann (Softmax) probability distribution over candidate move scores.
     * 
     * P(m_i) = exp(s_i / T) / sum_j(exp(s_j / T))
     * 
     * @param scores Array of scores for each column (0..COLS-1).
     * @param legal Boolean mask of valid columns.
     * @param temperature Softmax temperature (higher T -> uniform random; lower T -> argmax).
     * @return Normalized probability distribution summing to 1.0 across legal moves.
     */
    [[nodiscard]] static std::array<double, COLS> softmax(
        const std::array<Score, COLS>& scores,
        const std::array<bool, COLS>& legal,
        double temperature = 100.0
    ) noexcept {
        std::array<double, COLS> probs{};
        const double T = std::max(1.0, temperature);

        // Find max legal score to prevent exponential overflow in exp()
        Score max_legal = std::numeric_limits<Score>::lowest();
        int legal_count = 0;
        for (std::size_t i = 0; i < COLS; ++i) {
            if (legal[i]) {
                max_legal = std::max(max_legal, scores[i]);
                legal_count++;
            }
        }

        if (legal_count == 0) {
            return probs;
        }

        double sum_exp = 0.0;
        for (std::size_t i = 0; i < COLS; ++i) {
            if (legal[i]) {
                const double diff = static_cast<double>(scores[i] - max_legal);
                const double val = std::exp(diff / T);
                probs[i] = val;
                sum_exp += val;
            } else {
                probs[i] = 0.0;
            }
        }

        if (sum_exp > 0.0) {
            for (std::size_t i = 0; i < COLS; ++i) {
                probs[i] /= sum_exp;
            }
        }
        return probs;
    }

    /**
     * @brief Computes epsilon-greedy probability distribution.
     * 
     * With probability (1 - epsilon), chooses the optimal move (argmax).
     * With probability epsilon, distributes uniformly among all other legal moves.
     */
    [[nodiscard]] static std::array<double, COLS> epsilon_greedy(
        const std::array<Score, COLS>& scores,
        const std::array<bool, COLS>& legal,
        double epsilon = 0.1
    ) noexcept {
        std::array<double, COLS> probs{};
        const double eps = std::clamp(epsilon, 0.0, 1.0);

        int best_col = -1;
        Score best_score = std::numeric_limits<Score>::lowest();
        int legal_count = 0;

        for (std::size_t i = 0; i < COLS; ++i) {
            if (legal[i]) {
                legal_count++;
                if (scores[i] > best_score || best_col == -1) {
                    best_score = scores[i];
                    best_col = static_cast<int>(i);
                }
            }
        }

        if (legal_count == 0 || best_col == -1) {
            return probs;
        }

        if (legal_count == 1 || eps <= 0.0) {
            probs[static_cast<std::size_t>(best_col)] = 1.0;
            return probs;
        }

        const double non_best_share = eps / static_cast<double>(legal_count - 1);
        for (std::size_t i = 0; i < COLS; ++i) {
            if (!legal[i]) {
                probs[i] = 0.0;
            } else if (static_cast<int>(i) == best_col) {
                probs[i] = 1.0 - eps;
            } else {
                probs[i] = non_best_share;
            }
        }
        return probs;
    }

    /**
     * @brief Calculates expected payoff E[V] given candidate scores and transition probabilities.
     */
    [[nodiscard]] static double expected_value(
        const std::array<Score, COLS>& scores,
        const std::array<double, COLS>& probabilities
    ) noexcept {
        double ev = 0.0;
        for (std::size_t i = 0; i < COLS; ++i) {
            ev += static_cast<double>(scores[i]) * probabilities[i];
        }
        return ev;
    }
};

} // namespace mmb
