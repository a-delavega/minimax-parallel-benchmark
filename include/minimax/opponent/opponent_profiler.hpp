#pragma once

#include "minimax/board.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/opponent/opponent_type.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>

namespace mmb {

/**
 * @brief Dynamic runtime telemetry and Bayesian behavioral profiler for adversaries.
 * 
 * Monitors opponent decisions in real time to deduce their skill level, tactical blind spots,
 * blunder probability, and positional biases. Feeds empirical probabilities into ExploitEngine.
 */
class OpponentProfiler {
public:
    OpponentProfiler() = default;

    explicit OpponentProfiler(OpponentProfile initial_prior)
        : m_profile(initial_prior) {}

    /**
     * @brief Records an observed opponent move and updates internal behavioral metrics.
     * @param before State of the board prior to the move.
     * @param chosen_move Column selected by the opponent.
     * @param opponent Player whose move is being recorded.
     */
    void record_move(const Board& before, Column chosen_move, Player opponent) noexcept {
        if (!before.is_valid_move(chosen_move)) {
            return;
        }

        m_moves_observed++;
        const Player our_player = mmb::opponent(opponent);

        // Check if opponent had an immediate winning move available
        bool had_immediate_win = false;
        bool played_immediate_win = false;

        for (Column c = 0; c < COLS; ++c) {
            if (before.is_valid_move(c) && before.is_winning_move_for(opponent, c)) {
                had_immediate_win = true;
                if (c == chosen_move) {
                    played_immediate_win = true;
                }
            }
        }

        if (had_immediate_win && !played_immediate_win) {
            m_missed_wins++;
        }

        // Check if opponent faced an urgent threat (must block our immediate win)
        bool faced_urgent_threat = false;
        bool blocked_urgent_threat = false;

        for (Column c = 0; c < COLS; ++c) {
            if (before.is_valid_move(c) && before.is_winning_move_for(our_player, c)) {
                faced_urgent_threat = true;
                if (c == chosen_move) {
                    blocked_urgent_threat = true;
                }
            }
        }

        if (faced_urgent_threat && !blocked_urgent_threat) {
            m_unblocked_losses++;
        }

        // Record center positional bias (columns 2, 3, 4)
        if (chosen_move >= 2 && chosen_move <= 4) {
            m_center_moves++;
        }

        update_profile();
    }

    /**
     * @brief Generates predicted move probability distribution P(col) for the opponent.
     * 
     * @param board Current board state where opponent is to move.
     * @param opponent Opponent player identifier.
     * @param evaluator Static position evaluator for candidate move scoring.
     * @return Array of probabilities summing to 1.0 across all legal columns.
     */
    [[nodiscard]] std::array<double, COLS> predict_move_probabilities(
        const Board& board,
        Player opponent,
        const IEvaluator& evaluator
    ) const noexcept {
        std::array<double, COLS> probs{};
        const Player our_player = mmb::opponent(opponent);

        std::array<bool, COLS> legal{};
        int legal_count = 0;
        for (Column c = 0; c < COLS; ++c) {
            legal[static_cast<std::size_t>(c)] = board.is_valid_move(c);
            if (legal[static_cast<std::size_t>(c)]) {
                legal_count++;
            }
        }

        if (legal_count == 0) {
            return probs;
        }

        // 1. Check for immediate winning move
        std::array<bool, COLS> win_moves{};
        int win_count = 0;
        for (Column c = 0; c < COLS; ++c) {
            if (legal[static_cast<std::size_t>(c)] && board.is_winning_move_for(opponent, c)) {
                win_moves[static_cast<std::size_t>(c)] = true;
                win_count++;
            }
        }

        if (win_count > 0) {
            // An optimal or aware opponent will play the winning move with high probability
            const double win_prob = std::clamp(m_profile.tactical_awareness, 0.5, 1.0);
            const double remainder = (1.0 - win_prob) / std::max(1, legal_count - win_count);

            for (Column c = 0; c < COLS; ++c) {
                if (!legal[static_cast<std::size_t>(c)]) {
                    probs[static_cast<std::size_t>(c)] = 0.0;
                } else if (win_moves[static_cast<std::size_t>(c)]) {
                    probs[static_cast<std::size_t>(c)] = win_prob / static_cast<double>(win_count);
                } else {
                    probs[static_cast<std::size_t>(c)] = remainder;
                }
            }
            return probs;
        }

        // 2. Check for urgent defensive block
        std::array<bool, COLS> block_moves{};
        int block_count = 0;
        for (Column c = 0; c < COLS; ++c) {
            if (legal[static_cast<std::size_t>(c)] && board.is_winning_move_for(our_player, c)) {
                block_moves[static_cast<std::size_t>(c)] = true;
                block_count++;
            }
        }

        if (block_count > 0) {
            const double block_prob = std::clamp(m_profile.tactical_awareness * 0.95, 0.4, 0.98);
            const double remainder = (1.0 - block_prob) / std::max(1, legal_count - block_count);

            for (Column c = 0; c < COLS; ++c) {
                if (!legal[static_cast<std::size_t>(c)]) {
                    probs[static_cast<std::size_t>(c)] = 0.0;
                } else if (block_moves[static_cast<std::size_t>(c)]) {
                    probs[static_cast<std::size_t>(c)] = block_prob / static_cast<double>(block_count);
                } else {
                    probs[static_cast<std::size_t>(c)] = remainder;
                }
            }
            return probs;
        }

        // 3. General position: evaluate 1-ply candidates with Boltzmann distribution
        std::array<Score, COLS> scores{};
        for (Column c = 0; c < COLS; ++c) {
            if (legal[static_cast<std::size_t>(c)]) {
                Board next_board = board;
                next_board.make_move(c);
                Score eval = evaluator.evaluate(next_board, opponent, 1);
                // Apply center bias
                if (c >= 2 && c <= 4) {
                    eval += static_cast<Score>(m_profile.center_bias * 40.0);
                }
                scores[static_cast<std::size_t>(c)] = eval;
            } else {
                scores[static_cast<std::size_t>(c)] = std::numeric_limits<Score>::lowest();
            }
        }

        // Temperature varies inversely with estimated opponent skill
        const double T = 50.0 + (m_profile.blunder_rate * 250.0);
        return OpponentMath::softmax(scores, legal, T);
    }

    [[nodiscard]] const OpponentProfile& profile() const noexcept {
        return m_profile;
    }

    [[nodiscard]] uint32_t moves_observed() const noexcept { return m_moves_observed; }
    [[nodiscard]] uint32_t missed_wins() const noexcept { return m_missed_wins; }
    [[nodiscard]] uint32_t unblocked_losses() const noexcept { return m_unblocked_losses; }

    [[nodiscard]] double blunder_rate() const noexcept {
        if (m_moves_observed == 0) return m_profile.blunder_rate;
        const double blunders = static_cast<double>(m_missed_wins + m_unblocked_losses);
        return std::clamp(blunders / static_cast<double>(m_moves_observed), 0.0, 1.0);
    }

    [[nodiscard]] double center_bias() const noexcept {
        if (m_moves_observed == 0) return m_profile.center_bias;
        return static_cast<double>(m_center_moves) / static_cast<double>(m_moves_observed);
    }

    void reset() noexcept {
        m_moves_observed = 0;
        m_missed_wins = 0;
        m_unblocked_losses = 0;
        m_center_moves = 0;
        m_profile = OpponentProfile::preset_optimal();
    }

private:
    void update_profile() noexcept {
        if (m_moves_observed == 0) {
            return;
        }

        const double rate = blunder_rate();
        m_profile.blunder_rate = rate;
        m_profile.center_bias = center_bias();

        // Dynamically adjust inferred opponent type and tactical awareness
        if (m_missed_wins > 0 || m_unblocked_losses > 0) {
            m_profile.tactical_awareness = std::max(0.1, 1.0 - (rate * 1.5));
            if (rate > 0.40) {
                m_profile.type = OpponentType::Random;
                m_profile.estimated_depth = 0;
            } else if (rate > 0.15) {
                m_profile.type = OpponentType::HumanNovice;
                m_profile.estimated_depth = 2;
            } else {
                m_profile.type = OpponentType::HumanIntermediate;
                m_profile.estimated_depth = 4;
            }
        } else {
            // Zero blunders so far: high tactical awareness
            m_profile.tactical_awareness = 0.95;
            if (m_moves_observed >= 5) {
                m_profile.type = OpponentType::OptimalMinimax;
                m_profile.estimated_depth = 8;
            }
        }
    }

    OpponentProfile m_profile = OpponentProfile::preset_optimal();
    uint32_t m_moves_observed = 0;
    uint32_t m_missed_wins = 0;
    uint32_t m_unblocked_losses = 0;
    uint32_t m_center_moves = 0;
};

} // namespace mmb
