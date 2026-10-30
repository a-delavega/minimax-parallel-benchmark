#pragma once

#include "minimax/types.hpp"
#include <array>
#include <cstdint>

namespace mmb {

/**
 * @brief High-performance Connect 4 representation using 64-bit Bitboards.
 * 
 * Layout of 7 bits per column (6 playable rows + 1 sentinel bit on top):
 * bit_index = col * 7 + row
 * 
 *   Col:    0     1     2     3     4     5     6
 * Row 6  |  6  | 13  | 20  | 27  | 34  | 41  | 48  |  <- SENTINEL BITS (Always 0)
 * Row 5  |  5  | 12  | 19  | 26  | 33  | 40  | 47  |  <- Top playable row
 * Row 4  |  4  | 11  | 18  | 25  | 32  | 39  | 46  |
 * Row 3  |  3  | 10  | 17  | 24  | 31  | 38  | 45  |
 * Row 2  |  2  |  9  | 16  | 23  | 30  | 37  | 44  |
 * Row 1  |  1  |  8  | 15  | 22  | 29  | 36  | 43  |
 * Row 0  |  0  |  7  | 14  | 21  | 28  | 35  | 42  |  <- Bottom row
 * 
 * Total bits used: 49 bits within a single uint64_t.
 * Thread-safety: No dynamic memory, no global state. Trivially copyable by value across threads.
 */
struct Board {
    // Bitboard for each player: bitboard[0] = Player1, bitboard[1] = Player2
    uint64_t bitboard[2] = {0ULL, 0ULL};

    // Bit index where the next piece will be placed in each column
    // Initial values: {0, 7, 14, 21, 28, 35, 42}
    std::array<uint8_t, 7> height = {0, 7, 14, 21, 28, 35, 42};

    // Total number of moves played in the game
    uint8_t moves_played = 0;

    // Player with the current turn
    Player current_player = Player::Player1;

    // ============================================================
    // Bitmask Constants
    // ============================================================
    // Row 0 of all columns (bits 0, 7, 14, 21, 28, 35, 42)
    static constexpr uint64_t BOTTOM_MASK = 
        (1ULL << 0)  | (1ULL << 7)  | (1ULL << 14) | (1ULL << 21) | 
        (1ULL << 28) | (1ULL << 35) | (1ULL << 42);

    // Row 5 (top valid playable row) of all columns (bits 5, 12, 19, 26, 33, 40, 47)
    static constexpr uint64_t TOP_MASK = 
        (1ULL << 5)  | (1ULL << 12) | (1ULL << 19) | (1ULL << 26) | 
        (1ULL << 33) | (1ULL << 40) | (1ULL << 47);

    // All valid playable cells (rows 0 to 5 for all 7 columns)
    // 6 continuous bits per column
    static constexpr uint64_t BOARD_MASK = 
        (0b111111ULL << 0)  | (0b111111ULL << 7)  | (0b111111ULL << 14) | 
        (0b111111ULL << 21) | (0b111111ULL << 28) | (0b111111ULL << 35) | 
        (0b111111ULL << 42);

    // Combined bitmask representing all pieces on the board
    [[nodiscard]] constexpr uint64_t occupied() const noexcept {
        return bitboard[0] | bitboard[1];
    }

    /**
     * @brief Check whether a column can accept a piece in O(1).
     * @param col Column index [0, 6].
     */
    [[nodiscard]] constexpr bool is_valid_move(Column col) const noexcept {
        if (col < 0 || col >= COLS) return false;
        // If the target bit reaches row 6 (sentinel), column is full
        return (TOP_MASK & (1ULL << height[col])) == 0;
    }

    /**
     * @brief Play a move for current player in O(1).
     * @param col Column to drop the piece.
     */
    constexpr void make_move(Column col) noexcept {
        const uint64_t move_mask = 1ULL << height[col]++;
        bitboard[static_cast<uint8_t>(current_player)] |= move_mask;
        moves_played++;
        current_player = opponent(current_player);
    }

    /**
     * @brief Checks if a bitboard has 4 connected pieces in O(1).
     * 
     * Uses bit shifts with sentinel-bit protection:
     * - Vertical: shift 1
     * - Horizontal: shift 7
     * - Diagonal /: shift 6
     * - Diagonal \: shift 8
     */
    [[nodiscard]] static constexpr bool has_won(uint64_t b) noexcept {
        // Vertical (|)
        uint64_t m = b & (b >> 1);
        if (m & (m >> 2)) return true;

        // Horizontal (-)
        m = b & (b >> 7);
        if (m & (m >> 14)) return true;

        // Diagonal ascending (/)
        m = b & (b >> 6);
        if (m & (m >> 12)) return true;

        // Diagonal descending (\)
        m = b & (b >> 8);
        if (m & (m >> 16)) return true;

        return false;
    }

    /**
     * @brief Checks if the given player has won.
     */
    [[nodiscard]] constexpr bool has_won(Player player) const noexcept {
        return has_won(bitboard[static_cast<uint8_t>(player)]);
    }

    /**
     * @brief Checks if the game ended in a draw (board full without winner).
     */
    [[nodiscard]] constexpr bool is_draw() const noexcept {
        return moves_played >= (ROWS * COLS);
    }
};

} // namespace mmb
