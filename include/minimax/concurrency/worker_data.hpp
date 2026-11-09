#pragma once

#include "minimax/concurrency/cache_aligned.hpp"
#include "minimax/types.hpp"
#include <cstdint>

namespace mmb {

/**
 * @brief Thread-local state and telemetry for a single Lazy SMP search worker.
 * 
 * Enforces alignas(CACHE_LINE_SIZE) to guarantee that adjacent elements in a worker array
 * reside in distinct 64-byte L1 cache lines, eliminating False Sharing across CPU cores.
 */
struct alignas(CACHE_LINE_SIZE) WorkerData {
    int thread_id = 0;
    int depth_offset = 0;
    int depth_reached = 0;
    Column best_move = -1;
    Score best_score = 0;
    uint64_t nodes_visited = 0;
    uint64_t beta_cutoffs = 0;

    void reset(int id, int offset = 0) noexcept {
        thread_id = id;
        depth_offset = offset;
        depth_reached = 0;
        best_move = -1;
        best_score = 0;
        nodes_visited = 0;
        beta_cutoffs = 0;
    }
};

static_assert(sizeof(WorkerData) % CACHE_LINE_SIZE == 0, 
              "WorkerData size must be a multiple of CACHE_LINE_SIZE to prevent False Sharing.");
static_assert(alignof(WorkerData) >= CACHE_LINE_SIZE, 
              "WorkerData alignment must match or exceed CACHE_LINE_SIZE.");

} // namespace mmb
