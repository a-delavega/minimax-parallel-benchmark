#pragma once

#include "minimax/evaluator.hpp"

namespace mmb {

/**
 * @brief Trivial terminal-only position evaluator.
 * 
 * Scores positions based solely on terminal conditions:
 * - Win: +WIN_SCORE - depth (incentivizes fastest wins)
 * - Loss: -WIN_SCORE + depth (incentivizes longest survival)
 * - Draw: 0
 * - Non-terminal: 0 (pure depth-oriented brute force)
 * 
 * Extremely fast O(1) evaluation designed to maximize raw search speed and tree depth.
 */
class SimpleEvaluator final : public IEvaluator {
public:
    static constexpr Score WIN_SCORE = 100'000;

    [[nodiscard]] Score evaluate(const Board& board, Player pov, int depth = 0) const noexcept override {
        const Player opp = opponent(pov);

        if (board.has_won(pov)) {
            return WIN_SCORE - depth;
        }
        if (board.has_won(opp)) {
            return -WIN_SCORE + depth;
        }
        return 0; // Neutral / Draw / Undecided
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "SimpleEvaluator (Raw Depth)";
    }
};

} // namespace mmb
