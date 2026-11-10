#pragma once

#include "minimax/board.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/types.hpp"
#include <algorithm>
#include <array>

namespace mmb {

/**
 * @brief Per-worker search configuration generated for Lazy SMP diversification.
 */
struct ThreadSearchConfig {
    int thread_id = 0;
    int target_depth = 1;
    int depth_offset = 0;
    Score aspiration_delta = 0;
};

/**
 * @brief Induces controlled search asymmetry across Lazy SMP worker threads.
 * 
 * In naive parallel Alpha-Beta search, identical threads running identical move orders
 * duplicate the exact same tree traversal (work duplication). SearchDiversifier prevents
 * redundancy via three orthogonal mechanisms:
 * 1. Asymmetric Search Horizons: Helper threads explore D-1, D, or D+1 plies.
 * 2. Tactical-Safe Move Perturbation: Shallow plies permute top central candidate columns
 *    across workers while strictly locking immediate wins and defensive blocks at index 0.
 * 3. Asymmetric Aspiration Deltas: Shifts initial bound windows slightly per worker.
 */
class SearchDiversifier {
public:
    /**
     * @brief Compute search parameters for a specific worker thread.
     * @param thread_id Worker identifier (0 = Master, 1..N-1 = Helpers).
     * @param base_depth Requested search horizon D.
     * @return Diversified ThreadSearchConfig (always clamped to target_depth >= 1).
     */
    [[nodiscard]] static constexpr ThreadSearchConfig configure(int thread_id, int base_depth) noexcept {
        ThreadSearchConfig cfg;
        cfg.thread_id = thread_id;

        if (thread_id == 0 || base_depth <= 0) {
            cfg.depth_offset = 0;
            cfg.target_depth = std::max(0, base_depth);
            cfg.aspiration_delta = 0;
            return cfg;
        }

        // Helper depth offset cycle: +1 (deeper lookahead), 0 (same depth), -1 (fast bound feeder)
        constexpr std::array<int, 4> DEPTH_CYCLE = {1, 0, -1, 1};
        constexpr std::array<Score, 4> ASPIRATION_CYCLE = {25, -25, 50, -50};

        const int raw_offset = DEPTH_CYCLE[static_cast<std::size_t>(thread_id - 1) % DEPTH_CYCLE.size()];
        const int adjusted_depth = std::max(1, base_depth + raw_offset);

        cfg.depth_offset = adjusted_depth - base_depth;
        cfg.target_depth = adjusted_depth;
        cfg.aspiration_delta = ASPIRATION_CYCLE[static_cast<std::size_t>(thread_id - 1) % ASPIRATION_CYCLE.size()];
        return cfg;
    }

    /**
     * @brief Generate candidate moves with thread-dependent ordering diversification.
     * 
     * Master thread (thread_id == 0) and deep plies (ply > 2) use canonical center-outward ordering.
     * Helper threads at shallow plies rotate the top non-urgent central moves while preserving
     * immediate winning moves and urgent defensive blocks at the front.
     * 
     * @param board Current board state.
     * @param thread_id Worker ID.
     * @param ply Current distance from root (0 at root).
     * @param out_moves Output array populated with legal columns.
     * @return Number of legal moves populated.
     */
    [[nodiscard]] static int get_diversified_moves(
        const Board& board,
        int thread_id,
        int ply,
        std::array<Column, COLS>& out_moves
    ) noexcept {
        const int count = MoveOrderer::get_ordered_moves(board, out_moves);

        // Master thread or deep nodes or trivial branch counts keep canonical ordering
        if (thread_id <= 0 || ply > 2 || count <= 1) {
            return count;
        }

        // Preserve urgent tactical moves at the front of the list
        const Player current = board.current_player;
        const Player opp = opponent(current);
        int locked_prefix = 0;

        while (locked_prefix < count) {
            const Column col = out_moves[static_cast<std::size_t>(locked_prefix)];
            if (board.is_winning_move_for(current, col) || board.is_winning_move_for(opp, col)) {
                locked_prefix++;
            } else {
                break;
            }
        }

        const int tail_count = count - locked_prefix;
        if (tail_count < 2) {
            return count;
        }

        // Rotate among the top central candidates (up to 3) to explore distinct subtrees
        // without wasting first-move exploration on weak outer edges.
        const int window = std::min(tail_count, 3);
        const int shift = (thread_id + ply) % window;
        if (shift > 0) {
            auto begin_it = out_moves.begin() + locked_prefix;
            std::rotate(begin_it, begin_it + shift, begin_it + window);
        }

        return count;
    }
};

} // namespace mmb
