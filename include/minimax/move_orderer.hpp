#pragma once

#include "minimax/board.hpp"
#include "minimax/types.hpp"
#include <array>

namespace mmb {

/**
 * @brief Strategy for ordering candidate moves to maximize Alpha-Beta cutoffs.
 * 
 * Move prioritization order:
 * 1. Immediate winning moves: If placing a disc produces 4-in-a-row right now,
 *    it is explored first, producing an instantaneous beta-cutoff.
 * 2. Urgent defensive blocks: If the opponent threatens an immediate win on their
 *    turn, the blocking column is prioritized to avert defeat.
 * 3. Center-outward column sequence [3, 2, 4, 1, 5, 0, 6]: Column 3 participates
 *    in up to 16 lines (vs only 7 on outer edges), offering maximum long-term potential.
 */
class MoveOrderer {
public:
    // Center-outward column evaluation sequence: 3, 2, 4, 1, 5, 0, 6
    static constexpr std::array<Column, COLS> ORDERED_COLUMNS = {3, 2, 4, 1, 5, 0, 6};

    /**
     * @brief Populate an array with legal moves ordered for maximum pruning efficiency.
     * @param board The current game board.
     * @param out_moves Array to write legal moves into.
     * @return Number of legal moves populated in out_moves.
     */
    [[nodiscard]] static constexpr int get_ordered_moves(
        const Board& board, 
        std::array<Column, COLS>& out_moves
    ) noexcept {
        int count = 0;
        const Player current = board.current_player;
        const Player opp = opponent(current);

        Column win_move = -1;
        Column block_move = -1;

        // Fast scan for tactical threats
        for (const Column col : ORDERED_COLUMNS) {
            if (board.is_valid_move(col)) {
                if (win_move == -1 && board.is_winning_move_for(current, col)) {
                    win_move = col;
                }
                if (block_move == -1 && board.is_winning_move_for(opp, col)) {
                    block_move = col;
                }
            }
        }

        // 1. Immediate winning move takes highest priority
        if (win_move != -1) {
            out_moves[count++] = win_move;
        }

        // 2. Urgent defensive block against immediate opponent win
        if (block_move != -1 && block_move != win_move) {
            out_moves[count++] = block_move;
        }

        // 3. Fill remaining legal moves in center-outward order
        for (const Column col : ORDERED_COLUMNS) {
            if (board.is_valid_move(col) && col != win_move && col != block_move) {
                out_moves[count++] = col;
            }
        }

        return count;
    }
};

} // namespace mmb
