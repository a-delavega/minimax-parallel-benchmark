#pragma once

#include "minimax/board.hpp"
#include "minimax/search_result.hpp"

namespace mmb {

/**
 * @brief Abstract interface for game-tree search algorithms.
 */
class IEngine {
public:
    virtual ~IEngine() = default;

    /**
     * @brief Find the best move for the board's current player.
     * @param board Starting board position.
     * @param max_depth Maximum search plies to explore.
     * @return SearchResult containing best move and execution telemetry.
     */
    [[nodiscard]] virtual SearchResult search(const Board& board, int max_depth) = 0;

    /**
     * @brief Human-readable name of the engine strategy.
     */
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

} // namespace mmb
