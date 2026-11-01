#pragma once

#include "minimax/board.hpp"
#include "minimax/evaluator.hpp"
#include <array>
#include <bit>
#include <cstdint>

namespace mmb {

/**
 * @brief High-performance heuristic evaluator using bitboard pattern recognition.
 * 
 * Computes deep positional evaluations for non-terminal game states using:
 * 1. Column centrality weighting (Columns 3, 2/4, 1/5, 0/6) via hardware POPCNT.
 * 2. 3-in-a-row threat detection across all 4 directions (horizontal, vertical, diagonals).
 * 3. Immediate tactical threat bonuses (playable winning squares).
 * 4. 2-in-a-row potential alignment bonuses.
 * 5. Depth-discounted terminal scoring (+100,000 / -100,000).
 * 
 * All evaluations run in O(1) with zero dynamic memory allocation.
 */
class ComplexEvaluator final : public IEvaluator {
public:
    static constexpr Score WIN_SCORE = 100'000;

    // Weight per piece in each column (center columns control significantly more lines)
    static constexpr std::array<Score, COLS> COLUMN_WEIGHTS = {10, 25, 50, 90, 50, 25, 10};

    // Tactical scoring constants
    static constexpr Score THREAT_3_SCORE          = 350;  // 3-in-a-row with an empty cell
    static constexpr Score IMMEDIATE_THREAT_BONUS  = 1500; // 3-in-a-row playable right now
    static constexpr Score ALIGNMENT_2_SCORE       = 40;   // 2-in-a-row potential line

    [[nodiscard]] Score evaluate(const Board& board, Player pov, int depth = 0) const noexcept override {
        const Player opp = opponent(pov);

        // 1. Immediate terminal checks
        if (board.has_won(pov)) {
            return WIN_SCORE - depth;
        }
        if (board.has_won(opp)) {
            return -WIN_SCORE + depth;
        }
        if (board.is_draw()) {
            return 0;
        }

        const uint64_t b_pov = board.bitboard[static_cast<uint8_t>(pov)];
        const uint64_t b_opp = board.bitboard[static_cast<uint8_t>(opp)];
        const uint64_t empty = (~(b_pov | b_opp)) & Board::BOARD_MASK;

        // Bitmask of cells that can be played immediately (where the piece would drop)
        uint64_t playable_mask = 0ULL;
        for (Column c = 0; c < COLS; ++c) {
            if (board.is_valid_move(c)) {
                playable_mask |= (1ULL << board.height[c]);
            }
        }

        Score score = 0;

        // 2. Column centrality evaluation (Hardware popcount)
        for (Column c = 0; c < COLS; ++c) {
            const uint64_t col_mask = 0b111111ULL << (c * 7);
            const int pov_pieces = std::popcount(b_pov & col_mask);
            const int opp_pieces = std::popcount(b_opp & col_mask);
            score += COLUMN_WEIGHTS[c] * (pov_pieces - opp_pieces);
        }

        // 3. Directional 3-in-a-row and 2-in-a-row pattern evaluation
        score += evaluate_threats_and_alignments(b_pov, b_opp, empty, playable_mask);

        return score;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return "ComplexEvaluator (Heuristic Bitboard Patterns)";
    }

private:
    /**
     * @brief Evaluates 3-in-a-row threats and 2-in-a-row potentials for both players.
     */
    [[nodiscard]] static Score evaluate_threats_and_alignments(
        uint64_t b_pov, 
        uint64_t b_opp, 
        uint64_t empty, 
        uint64_t playable_mask
    ) noexcept {
        Score score = 0;

        // Direction 1: Vertical (d = 1)
        // Only pattern possible vertically is 3 pieces of player + 1 empty space above
        score += evaluate_vertical(b_pov, empty, playable_mask);
        score -= evaluate_vertical(b_opp, empty, playable_mask);

        // Direction 2: Horizontal (d = 7)
        // Valid starts in columns 0, 1, 2, 3
        constexpr uint64_t HORIZ_START = (0b111111ULL << 0)  | (0b111111ULL << 7) |
                                         (0b111111ULL << 14) | (0b111111ULL << 21);
        score += evaluate_direction(b_pov, empty, playable_mask, 7, HORIZ_START);
        score -= evaluate_direction(b_opp, empty, playable_mask, 7, HORIZ_START);

        // Direction 3: Diagonal Ascending / (d = 6)
        // Starts in columns 0, 1, 2, 3 and rows 0, 1, 2
        constexpr uint64_t DIAG_ASC_START = (0b000111ULL << 0)  | (0b000111ULL << 7) |
                                            (0b000111ULL << 14) | (0b000111ULL << 21);
        score += evaluate_direction(b_pov, empty, playable_mask, 6, DIAG_ASC_START);
        score -= evaluate_direction(b_opp, empty, playable_mask, 6, DIAG_ASC_START);

        // Direction 4: Diagonal Descending \ (d = 8)
        // Starts in columns 0, 1, 2, 3 and rows 3, 4, 5
        constexpr uint64_t DIAG_DESC_START = (0b111000ULL << 0)  | (0b111000ULL << 7) |
                                             (0b111000ULL << 14) | (0b111000ULL << 21);
        score += evaluate_direction(b_pov, empty, playable_mask, 8, DIAG_DESC_START);
        score -= evaluate_direction(b_opp, empty, playable_mask, 8, DIAG_DESC_START);

        return score;
    }

    /**
     * @brief Evaluate vertical 3-in-a-row threats (3 pieces with empty cell directly above).
     */
    [[nodiscard]] static Score evaluate_vertical(
        uint64_t b, 
        uint64_t empty, 
        uint64_t playable_mask
    ) noexcept {
        // Vertical 3: b at row r, r+1, r+2 and empty at r+3
        // Starts can only be rows 0, 1, 2
        constexpr uint64_t VERT_START = (0b000111ULL << 0)  | (0b000111ULL << 7)  |
                                        (0b000111ULL << 14) | (0b000111ULL << 21) |
                                        (0b000111ULL << 28) | (0b000111ULL << 35) |
                                        (0b000111ULL << 42);

        const uint64_t p1 = b & (b >> 1) & (b >> 2) & (empty >> 3) & VERT_START;
        Score s = std::popcount(p1) * THREAT_3_SCORE;

        // If the empty cell is currently playable (meaning next move wins)
        const uint64_t immediate = (p1 << 3) & playable_mask;
        s += std::popcount(immediate) * IMMEDIATE_THREAT_BONUS;

        return s;
    }

    /**
     * @brief Evaluate threats and alignments for a given direction d with valid start bitmask.
     */
    [[nodiscard]] static Score evaluate_direction(
        uint64_t b, 
        uint64_t empty, 
        uint64_t playable_mask, 
        int d, 
        uint64_t start_mask
    ) noexcept {
        Score s = 0;

        const uint64_t b1 = b >> d;
        const uint64_t b2 = b >> (2 * d);
        const uint64_t b3 = b >> (3 * d);

        const uint64_t e1 = empty >> d;
        const uint64_t e2 = empty >> (2 * d);
        const uint64_t e3 = empty >> (3 * d);

        // 4 patterns of 3 pieces + 1 empty space:
        // Pattern 1: [e, b, b, b] -> empty cell is at root position
        const uint64_t t1 = empty & b1 & b2 & b3 & start_mask;
        s += std::popcount(t1) * THREAT_3_SCORE;
        s += std::popcount(t1 & playable_mask) * IMMEDIATE_THREAT_BONUS;

        // Pattern 2: [b, e, b, b] -> empty cell is at +d
        const uint64_t t2 = b & e1 & b2 & b3 & start_mask;
        s += std::popcount(t2) * THREAT_3_SCORE;
        s += std::popcount((t2 << d) & playable_mask) * IMMEDIATE_THREAT_BONUS;

        // Pattern 3: [b, b, e, b] -> empty cell is at +2d
        const uint64_t t3 = b & b1 & e2 & b3 & start_mask;
        s += std::popcount(t3) * THREAT_3_SCORE;
        s += std::popcount((t3 << (2 * d)) & playable_mask) * IMMEDIATE_THREAT_BONUS;

        // Pattern 4: [b, b, b, e] -> empty cell is at +3d
        const uint64_t t4 = b & b1 & b2 & e3 & start_mask;
        s += std::popcount(t4) * THREAT_3_SCORE;
        s += std::popcount((t4 << (3 * d)) & playable_mask) * IMMEDIATE_THREAT_BONUS;

        // 2-in-a-row alignments with 2 empty spaces (potential developing lines):
        // [b, b, e, e], [b, e, b, e], [e, b, b, e]
        const uint64_t a1 = b & b1 & e2 & e3 & start_mask;
        const uint64_t a2 = b & e1 & b2 & e3 & start_mask;
        const uint64_t a3 = empty & b1 & b2 & e3 & start_mask;
        s += (std::popcount(a1) + std::popcount(a2) + std::popcount(a3)) * ALIGNMENT_2_SCORE;

        return s;
    }
};

} // namespace mmb
