#pragma once

#include "minimax/board.hpp"
#include <iostream>

namespace mmb {

/**
 * @brief Renders the Connect 4 board in a clean terminal ASCII format.
 */
inline void render_board(const Board& b, std::ostream& os = std::cout) {
    os << "\n   0   1   2   3   4   5   6   (Columna)\n";
    os << " +---+---+---+---+---+---+---+\n";
    for (int r = ROWS - 1; r >= 0; --r) {
        os << " |";
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
    os << " +---+---+---+---+---+---+---+\n";
    os << " Turno: " << (b.current_player == Player::Player1 ? "Jugador 1 [X]" : "Jugador 2 [O]")
       << " | Jugadas realizadas: " << static_cast<int>(b.moves_played) << "\n\n";
}

} // namespace mmb
