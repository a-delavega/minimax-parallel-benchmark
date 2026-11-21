#pragma once

#include "minimax/board.hpp"
#include "minimax/search_result.hpp"
#include <functional>

namespace mmb {

/**
 * @brief Callback signature invoked when an iterative deepening depth iteration completes.
 * @param result SearchResult of the completed depth layer.
 * @return true to continue to next depth, false to abort search gracefully.
 */
using IterationCallback = std::function<bool(const SearchResult&)>;

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
     * @brief Find best move using Iterative Deepening Search (d = 1 .. max_depth).
     * @param board Starting board position.
     * @param max_depth Maximum search depth to iterate up to.
     * @param callback Optional callback invoked after each depth iteration finishes.
     * @return SearchResult containing best move and accumulated execution telemetry.
     */
    [[nodiscard]] virtual SearchResult search_iterative(
        const Board& board,
        int max_depth,
        IterationCallback callback = nullptr
    ) {
        SearchResult final_res;
        for (int d = 1; d <= max_depth; ++d) {
            SearchResult cur = search(board, d);
            final_res = cur;
            final_res.depth_reached = d;
            final_res.iterations_completed = d;
            if (callback && !callback(cur)) {
                break;
            }
        }
        return final_res;
    }

    /**
     * @brief Human-readable name of the engine strategy.
     */
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

} // namespace mmb
