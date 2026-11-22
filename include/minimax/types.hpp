#pragma once

#include <cstdint>
#include <exception>

namespace mmb {

// Board column index (0 to 6)
using Column = int;

// Position evaluation score for Minimax / Heuristics
using Score = int32_t;

// Player identification
enum class Player : uint8_t {
    Player1 = 0,
    Player2 = 1,
    None    = 2
};

// Switch player in O(1)
[[nodiscard]] constexpr Player opponent(Player p) noexcept {
    return (p == Player::Player1) ? Player::Player2 : Player::Player1;
}

// Connect 4 standard dimensions
inline constexpr int ROWS = 6;
inline constexpr int COLS = 7;

// Game-theoretic scoring thresholds
inline constexpr Score MATE_THRESHOLD = 90'000;
inline constexpr Score SCORE_WIN = 100'000;

// Periodic node frequency for atomic stop flag polling (check every 2048 nodes)
inline constexpr uint64_t NODE_CHECK_MASK = 2047ULL;

/**
 * @brief Exception thrown to unwind tree traversal upon atomic cooperative search cancellation.
 */
struct SearchAbortedException final : public std::exception {
    [[nodiscard]] const char* what() const noexcept override {
        return "SearchAbortedException: Game-tree search interrupted by atomic stop flag";
    }
};

} // namespace mmb
