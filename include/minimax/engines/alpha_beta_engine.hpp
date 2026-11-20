#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include "minimax/evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/search_result.hpp"
#include "minimax/transposition/transposition_table.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>

namespace mmb {

/**
 * @brief Sequential Alpha-Beta pruning engine with optional Transposition Table integration.
 * 
 * Explores game-tree branches while maintaining [alpha, beta] bounds:
 * - alpha: Lower bound score guaranteed for the maximizing player.
 * - beta: Upper bound score guaranteed for the minimizing player.
 * 
 * Integrates:
 * 1. MoveOrderer to evaluate high-potential central columns first.
 * 2. TranspositionTable (TT) for O(1) transposition cutoffs and TT-guided move ordering.
 * 3. Distance-independent mate score normalization (score_to_tt / score_from_tt).
 */
class AlphaBetaEngine final : public IEngine {
public:
    explicit AlphaBetaEngine(
        std::shared_ptr<const IEvaluator> evaluator,
        std::shared_ptr<TranspositionTable> tt = nullptr
    ) : m_evaluator(std::move(evaluator)),
        m_tt(std::move(tt)) {}

    void set_transposition_table(std::shared_ptr<TranspositionTable> tt) noexcept {
        m_tt = std::move(tt);
    }

    [[nodiscard]] std::shared_ptr<TranspositionTable> transposition_table() const noexcept {
        return m_tt;
    }

    [[nodiscard]] SearchResult search(const Board& board, int max_depth) override {
        SearchResult result;
        result.depth_reached = max_depth;

        if (board.is_draw() || board.has_won(Player::Player1) || board.has_won(Player::Player2) || max_depth <= 0) {
            result.score = m_evaluator->evaluate(board, board.current_player, 0);
            return result;
        }

        const auto start_time = std::chrono::high_resolution_clock::now();

        if (m_tt) {
            m_tt->new_search();
        }

        Score alpha = std::numeric_limits<Score>::lowest() + 1;
        const Score beta = std::numeric_limits<Score>::max() - 1;
        const Score orig_alpha = alpha;

        Score best_score = std::numeric_limits<Score>::lowest();
        Column best_col = -1;

        const Player pov = board.current_player;

        // Query TT for root move recommendation
        Column root_tt_move = -1;
        if (m_tt) {
            TTEntry root_entry;
            if (m_tt->probe(board.zobrist_hash, root_entry)) {
                root_tt_move = root_entry.best_move;
            }
        }

        // Order root candidate moves to test TT-move and center columns first
        std::array<Column, COLS> ordered_moves{};
        const int num_moves = MoveOrderer::get_ordered_moves(board, ordered_moves, root_tt_move);

        for (int i = 0; i < num_moves; ++i) {
            const Column col = ordered_moves[static_cast<std::size_t>(i)];

            Board next_board = board;
            next_board.make_move(col);
            result.nodes_visited++;

            // Minimizing child step
            const Score score = alphabeta(
                next_board, 
                max_depth - 1, 
                1, 
                alpha, 
                beta, 
                false, 
                pov, 
                result.nodes_visited
            );

            if (score > best_score || best_col == -1) {
                best_score = score;
                best_col = col;
            }

            alpha = std::max(alpha, best_score);
            if (alpha >= beta) {
                break; // Root cutoff
            }
        }

        // Store root position in TT
        if (m_tt && best_col != -1) {
            TTFlag flag = TTFlag::Exact;
            if (best_score <= orig_alpha) {
                flag = TTFlag::UpperBound;
            } else if (best_score >= beta) {
                flag = TTFlag::LowerBound;
            }
            m_tt->store(board.zobrist_hash, TranspositionTable::score_to_tt(best_score, 0),
                        best_col, static_cast<uint8_t>(max_depth), flag);
        }

        const auto end_time = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        result.best_move = best_col;
        result.score = best_score;

        return result;
    }

    [[nodiscard]] const char* name() const noexcept override {
        return (m_tt != nullptr)
            ? "AlphaBetaEngine (Sequential Alpha-Beta with Transposition Table)"
            : "AlphaBetaEngine (Sequential Alpha-Beta with Move Ordering)";
    }

private:
    [[nodiscard]] Score alphabeta(
        const Board& board, 
        int depth, 
        int ply, 
        Score alpha, 
        Score beta, 
        bool is_maximizing, 
        Player pov, 
        uint64_t& nodes_count
    ) const noexcept {
        const Player current = board.current_player;
        const Player previous = opponent(current);

        // Terminal condition: previous player just won
        if (board.has_won(previous)) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        // Terminal condition: draw or maximum depth reached
        if (board.is_draw() || depth == 0) {
            return m_evaluator->evaluate(board, pov, ply);
        }

        const Score orig_alpha = alpha;
        const Score orig_beta = beta;
        Column tt_move = -1;

        // 1. Transposition Table Probe & Cutoff
        if (m_tt) {
            TTEntry tt_entry;
            if (m_tt->probe(board.zobrist_hash, tt_entry)) {
                tt_move = tt_entry.best_move;
                if (tt_entry.depth >= depth) {
                    const Score tt_score = TranspositionTable::score_from_tt(tt_entry.score, ply);
                    if (tt_entry.get_flag() == TTFlag::Exact) {
                        return tt_score;
                    }
                    if (tt_entry.get_flag() == TTFlag::LowerBound && tt_score >= beta) {
                        return tt_score;
                    }
                    if (tt_entry.get_flag() == TTFlag::UpperBound && tt_score <= alpha) {
                        return tt_score;
                    }
                }
            }
        }

        std::array<Column, COLS> ordered_moves{};
        const int num_moves = MoveOrderer::get_ordered_moves(board, ordered_moves, tt_move);
        Column best_child_move = -1;

        if (is_maximizing) {
            Score max_eval = std::numeric_limits<Score>::lowest();

            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[static_cast<std::size_t>(i)];

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = alphabeta(
                    next_board, 
                    depth - 1, 
                    ply + 1, 
                    alpha, 
                    beta, 
                    false, 
                    pov, 
                    nodes_count
                );

                if (eval > max_eval || best_child_move == -1) {
                    max_eval = eval;
                    best_child_move = col;
                }

                alpha = std::max(alpha, eval);
                if (beta <= alpha) {
                    break; // Beta cutoff
                }
            }

            if (m_tt && best_child_move != -1) {
                TTFlag flag = TTFlag::Exact;
                if (max_eval <= orig_alpha) {
                    flag = TTFlag::UpperBound;
                } else if (max_eval >= beta) {
                    flag = TTFlag::LowerBound;
                }
                m_tt->store(board.zobrist_hash, TranspositionTable::score_to_tt(max_eval, ply),
                            best_child_move, static_cast<uint8_t>(depth), flag);
            }

            return max_eval;
        } else {
            Score min_eval = std::numeric_limits<Score>::max();

            for (int i = 0; i < num_moves; ++i) {
                const Column col = ordered_moves[static_cast<std::size_t>(i)];

                Board next_board = board;
                next_board.make_move(col);
                nodes_count++;

                const Score eval = alphabeta(
                    next_board, 
                    depth - 1, 
                    ply + 1, 
                    alpha, 
                    beta, 
                    true, 
                    pov, 
                    nodes_count
                );

                if (eval < min_eval || best_child_move == -1) {
                    min_eval = eval;
                    best_child_move = col;
                }

                beta = std::min(beta, eval);
                if (beta <= alpha) {
                    break; // Alpha cutoff
                }
            }

            if (m_tt && best_child_move != -1) {
                TTFlag flag = TTFlag::Exact;
                if (min_eval <= alpha) {
                    flag = TTFlag::UpperBound;
                } else if (min_eval >= orig_beta) {
                    flag = TTFlag::LowerBound;
                }
                m_tt->store(board.zobrist_hash, TranspositionTable::score_to_tt(min_eval, ply),
                            best_child_move, static_cast<uint8_t>(depth), flag);
            }

            return min_eval;
        }
    }

    std::shared_ptr<const IEvaluator> m_evaluator;
    std::shared_ptr<TranspositionTable> m_tt;
};

} // namespace mmb
