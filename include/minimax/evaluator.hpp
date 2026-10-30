#pragma once

#include "minimax/board.hpp"
#include "minimax/types.hpp"

namespace mmb {

/**
 * @brief Abstract interface for static board evaluation heuristics.
 */
class IEvaluator {
public:
    virtual ~IEvaluator() = default;

    /**
     * @brief Evaluate a board state from the perspective of a target player.
     * @param board Game board state.
     * @param pov Perspective player.
     * @param depth Current search depth (used for distance-to-mate discounting).
     * @return Positive score for advantage, negative for disadvantage.
     */
    [[nodiscard]] virtual Score evaluate(const Board& board, Player pov, int depth = 0) const noexcept = 0;

    /**
     * @brief Human-readable name of the evaluation strategy.
     */
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

} // namespace mmb
