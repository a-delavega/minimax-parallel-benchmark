#pragma once

#include "minimax/concurrency/cache_aligned.hpp"
#include "minimax/types.hpp"
#include <atomic>
#include <limits>

namespace mmb {

/**
 * @brief Lock-free shared coordination state for Lazy SMP search workers.
 * 
 * Provides atomic signaling and root bounds reduction without requiring
 * mutexes or lock synchronization on the search recursive hot-path.
 */
struct alignas(CACHE_LINE_SIZE) SharedSearchState {
    std::atomic<bool> stop_flag{false};
    std::atomic<Score> global_best_score{std::numeric_limits<Score>::lowest()};
    std::atomic<Column> global_best_move{-1};
    std::atomic<uint32_t> completed_workers{0};

    SharedSearchState() = default;

    // Non-copyable, non-movable due to atomics
    SharedSearchState(const SharedSearchState&) = delete;
    SharedSearchState& operator=(const SharedSearchState&) = delete;

    void reset() noexcept {
        stop_flag.store(false, std::memory_order_relaxed);
        global_best_score.store(std::numeric_limits<Score>::lowest(), std::memory_order_relaxed);
        global_best_move.store(-1, std::memory_order_relaxed);
        completed_workers.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] bool should_stop() const noexcept {
        return stop_flag.load(std::memory_order_relaxed);
    }

    void request_stop() noexcept {
        stop_flag.store(true, std::memory_order_release);
    }

    /**
     * @brief Lock-free opportunistic update of the global best move and score.
     * @param move Candidate column.
     * @param score Candidate score.
     * @return True if this update successfully raised the global best score.
     */
    bool update_best(Column move, Score score) noexcept {
        Score current = global_best_score.load(std::memory_order_relaxed);
        while (score > current) {
            if (global_best_score.compare_exchange_weak(
                    current, score, 
                    std::memory_order_release, 
                    std::memory_order_relaxed)) {
                global_best_move.store(move, std::memory_order_release);
                return true;
            }
        }
        return false;
    }
};

static_assert(sizeof(SharedSearchState) % CACHE_LINE_SIZE == 0,
              "SharedSearchState size must be a multiple of CACHE_LINE_SIZE.");

} // namespace mmb
