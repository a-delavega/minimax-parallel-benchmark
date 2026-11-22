#pragma once

#include "minimax/board.hpp"
#include "minimax/search_result.hpp"
#include "minimax/time/time_manager.hpp"
#include <chrono>
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
    /**
     * @brief Sets or clears an external atomic stop flag for cooperative search cancellation.
     */
    virtual void set_stop_flag(std::atomic<bool>* flag) noexcept {
        (void)flag;
    }

    [[nodiscard]] virtual SearchResult search_iterative(
        const Board& board,
        int max_depth,
        IterationCallback callback = nullptr
    ) {
        SearchResult final_res;
        for (int d = 1; d <= max_depth; ++d) {
            try {
                SearchResult cur = search(board, d);
                final_res = cur;
                final_res.depth_reached = d;
                final_res.iterations_completed = d;
                if (callback && !callback(cur)) {
                    break;
                }
            } catch (const SearchAbortedException&) {
                break;
            }
        }
        return final_res;
    }

    /**
     * @brief Find best move using dynamic time-budgeted iterative deepening search.
     * @param board Starting board position.
     * @param time_mgr Configured TimeManager instance.
     * @param max_depth Absolute maximum depth cap (defaults to 42 plies).
     * @return SearchResult containing best move found within the allotted time.
     */
    [[nodiscard]] virtual SearchResult search_timed(
        const Board& board,
        TimeManager& time_mgr,
        int max_depth = 42
    ) {
        set_stop_flag(&time_mgr.stop_flag());
        struct StopGuard {
            IEngine* engine;
            ~StopGuard() { engine->set_stop_flag(nullptr); }
        } guard{this};

        time_mgr.start_turn(board);
        auto last_iter_start = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds last_iter_duration{0};

        SearchResult res = search_iterative(
            board,
            max_depth,
            [&](const SearchResult& iter_res) {
                const auto now = std::chrono::high_resolution_clock::now();
                last_iter_duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_iter_start);
                last_iter_start = now;
                return time_mgr.should_start_next_depth(iter_res.depth_reached, last_iter_duration);
            }
        );

        time_mgr.complete_turn(time_mgr.elapsed());
        return res;
    }

    /**
     * @brief Human-readable name of the engine strategy.
     */
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

} // namespace mmb
