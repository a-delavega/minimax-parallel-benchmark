#pragma once

#include <cstdint>

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

} // namespace mmb
