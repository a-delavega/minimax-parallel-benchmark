#pragma once

#include "minimax/types.hpp"
#include <chrono>

namespace mmb {

/**
 * @brief Telemetry and result statistics from a search execution.
 */
struct SearchResult {
    Column best_move = -1;             // Recommended column [0, 6], or -1 if no legal move
    Score score = 0;                   // Position score from perspective of the searching player
    uint64_t nodes_visited = 0;        // Total nodes evaluated during search
    std::chrono::microseconds duration{0}; // Elapsed wall-clock time
    int depth_reached = 0;             // Depth level reached by search

    [[nodiscard]] double nodes_per_second() const noexcept {
        const double seconds = duration.count() / 1'000'000.0;
        return (seconds > 0.0) ? (static_cast<double>(nodes_visited) / seconds) : 0.0;
    }
};

} // namespace mmb
