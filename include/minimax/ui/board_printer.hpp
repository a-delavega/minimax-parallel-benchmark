#pragma once

#include "minimax/board.hpp"
#include <iostream>

namespace mmb {

/**
 * @brief Renders the Connect 4 board in a clean terminal ASCII format.
 * Columns are displayed 1-based (1 to 7) on both top and bottom for intuitive visual alignment.
 */
inline void render_board(const Board& b, std::ostream& os = std::cout) {
    os << "\n    1   2   3   4   5   6   7   (Column)\n";
    os << "  +---+---+---+---+---+---+---+\n";
    for (int r = ROWS - 1; r >= 0; --r) {
        os << "  |";
        for (int c = 0; c < COLS; ++c) {
            const uint64_t mask = 1ULL << (c * 7 + r);
            if (b.bitboard[0] & mask) {
                os << " X |";
            } else if (b.bitboard[1] & mask) {
                os << " O |";
            } else {
                os << " . |";
            }
        }
        os << "\n";
    }
    os << "  +---+---+---+---+---+---+---+\n";
    os << "    1   2   3   4   5   6   7   (Column)\n";
    os << "  Turn: " << (b.current_player == Player::Player1 ? "Player 1 [X]" : "Player 2 [O]")
       << " | Moves played: " << static_cast<int>(b.moves_played) << "\n\n";
}

} // namespace mmb
