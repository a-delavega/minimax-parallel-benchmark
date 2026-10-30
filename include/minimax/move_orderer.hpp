#pragma once

#include "minimax/board.hpp"
#include "minimax/types.hpp"
#include <array>

namespace mmb {

/**
 * @brief Strategy for ordering candidate moves to maximize Alpha-Beta cutoffs.
 * 
 * In Connect 4, playing towards the center offers the highest number of 4-in-a-row
 * potential lines (Column 3 participates in up to 16 lines, whereas columns 0 and 6
 * only participate in 7). Prioritizing center columns first dramatically improves
 * branch pruning efficiency.
 */
class MoveOrderer {
public:
    // Center-outward column evaluation sequence: 3, 2, 4, 1, 5, 0, 6
    static constexpr std::array<Column, COLS> ORDERED_COLUMNS = {3, 2, 4, 1, 5, 0, 6};

    /**
     * @brief Populate an array with all legal moves ordered from center outwards.
     * @param board The current game board.
     * @param out_moves Array to write legal moves into.
     * @return Number of legal moves populated in out_moves.
     */
    [[nodiscard]] static constexpr int get_ordered_moves(
        const Board& board, 
        std::array<Column, COLS>& out_moves
    ) noexcept {
        int count = 0;
        for (const Column col : ORDERED_COLUMNS) {
            if (board.is_valid_move(col)) {
                out_moves[count++] = col;
            }
        }
        return count;
    }
};

} // namespace mmb
