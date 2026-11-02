#include "minimax/board.hpp"
#include <iostream>

void print_board(const mmb::Board& b) {
    std::cout << "\n    1   2   3   4   5   6   7   (Column)\n";
    std::cout << "  +---+---+---+---+---+---+---+\n";
    for (int r = mmb::ROWS - 1; r >= 0; --r) {
        std::cout << "  |";
        for (int c = 0; c < mmb::COLS; ++c) {
            const uint64_t mask = 1ULL << (c * 7 + r);
            if (b.bitboard[0] & mask) {
                std::cout << " X |";
            } else if (b.bitboard[1] & mask) {
                std::cout << " O |";
            } else {
                std::cout << " . |";
            }
        }
        std::cout << "\n";
    }
    std::cout << "  +---+---+---+---+---+---+---+\n";
    std::cout << "    1   2   3   4   5   6   7   (Column)\n";
    std::cout << "  Turn: " << (b.current_player == mmb::Player::Player1 ? "Player 1 (X)" : "Player 2 (O)") 
              << " | Moves played: " << static_cast<int>(b.moves_played) << "\n\n";
}

int main() {
    mmb::Board board;
    std::cout << "Initial empty board:";
    print_board(board);

    std::cout << "Executing sample test moves: P1 col 4, P2 col 4, P1 col 3, P2 col 5 (1-based)...\n";
    board.make_move(3); // Column 4 (1-based)
    board.make_move(3); // Column 4 (1-based)
    board.make_move(2); // Column 3 (1-based)
    board.make_move(4); // Column 5 (1-based)
    print_board(board);

    return 0;
}
