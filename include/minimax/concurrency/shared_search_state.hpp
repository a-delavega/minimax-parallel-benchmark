#pragma once

#include "minimax/concurrency/cache_aligned.hpp"
#include "minimax/types.hpp"
#include <atomic>
#include <cstdint>
#include <limits>

namespace mmb {

/**
 * @brief Unpacked view of the globally best move, score, and depth.
 */
struct UnpackedBest {
    Column move = -1;
    Score score = std::numeric_limits<Score>::lowest();
    int depth = 0;
    bool valid = false;
};

/**
 * @brief Lock-free shared coordination state for Lazy SMP search workers.
 * 
 * Uses a single 64-bit atomic word (packed_best) to guarantee indivisible CAS updates
 * of (move, score, depth), completely eliminating torn reads or stale-write races
 * without mutexes or locks on the recursive hot-path.
 * 
 * 64-bit Packed Layout:
 *   Bits [0..31]  : Score (int32_t)
 *   Bits [32..47] : Depth (uint16_t)
 *   Bits [48..55] : Column move (int8_t)
 *   Bit  [56]     : Valid flag (1 bit)
 */
struct alignas(CACHE_LINE_SIZE) SharedSearchState {
    std::atomic<bool> stop_flag{false};
    std::atomic<uint64_t> packed_best{0ULL};
    std::atomic<uint32_t> completed_workers{0};

    SharedSearchState() = default;

    // Non-copyable, non-movable due to atomics
    SharedSearchState(const SharedSearchState&) = delete;
    SharedSearchState& operator=(const SharedSearchState&) = delete;

    void reset() noexcept {
        stop_flag.store(false, std::memory_order_relaxed);
        packed_best.store(0ULL, std::memory_order_relaxed);
        completed_workers.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] bool should_stop() const noexcept {
        return stop_flag.load(std::memory_order_relaxed);
    }

    void request_stop() noexcept {
        stop_flag.store(true, std::memory_order_release);
    }

    [[nodiscard]] static constexpr uint64_t pack(Column move, Score score, int depth) noexcept {
        const uint64_t s = static_cast<uint32_t>(score);
        const uint64_t d = static_cast<uint16_t>(depth > 0 ? depth : 0);
        const uint64_t m = static_cast<uint8_t>(static_cast<int8_t>(move));
        const uint64_t v = 1ULL;
        return s | (d << 32) | (m << 48) | (v << 56);
    }

    [[nodiscard]] static constexpr UnpackedBest unpack(uint64_t raw) noexcept {
        UnpackedBest res;
        res.valid = ((raw >> 56) & 1ULL) != 0;
        if (!res.valid) {
            return res;
        }
        res.score = static_cast<Score>(static_cast<uint32_t>(raw & 0xFFFFFFFFULL));
        res.depth = static_cast<int>(static_cast<uint16_t>((raw >> 32) & 0xFFFFULL));
        res.move  = static_cast<Column>(static_cast<int8_t>((raw >> 48) & 0xFFULL));
        return res;
    }

    /**
     * @brief Atomically read the current best (move, score, depth) tuple.
     */
    [[nodiscard]] UnpackedBest read_best() const noexcept {
        return unpack(packed_best.load(std::memory_order_acquire));
    }

    [[nodiscard]] Column best_move() const noexcept {
        return read_best().move;
    }

    [[nodiscard]] Score best_score() const noexcept {
        return read_best().score;
    }

    [[nodiscard]] int best_depth() const noexcept {
        return read_best().depth;
    }

    /**
     * @brief Lock-free indivisible CAS update of the global best (move, score, depth).
     * 
     * Preference rule:
     * 1. Any valid result beats an invalid (uninitialized) state.
     * 2. Higher search depth strictly dominates shallower depth.
     * 3. At equal depth, strictly higher score wins.
     * 
     * @param move Candidate column.
     * @param score Candidate score.
     * @param depth Depth at which this candidate was verified.
     * @return True if this call updated the global state.
     */
    bool update_best(Column move, Score score, int depth = 0) noexcept {
        const int clamped_depth = (depth > 0) ? depth : 0;
        const uint64_t desired = pack(move, score, clamped_depth);
        uint64_t current_raw = packed_best.load(std::memory_order_relaxed);

        while (true) {
            const UnpackedBest current = unpack(current_raw);
            const bool is_better = !current.valid ||
                                   (clamped_depth > current.depth) ||
                                   (clamped_depth == current.depth && score > current.score);
            if (!is_better) {
                return false;
            }

            if (packed_best.compare_exchange_weak(
                    current_raw, desired,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                return true;
            }
        }
    }
};

static_assert(sizeof(SharedSearchState) % CACHE_LINE_SIZE == 0,
              "SharedSearchState size must be a multiple of CACHE_LINE_SIZE.");
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "64-bit atomic word must be lock-free on the target architecture.");

} // namespace mmb
