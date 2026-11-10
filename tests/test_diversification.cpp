#include <catch2/catch_test_macros.hpp>
#include "minimax/board.hpp"
#include "minimax/concurrency/search_diversifier.hpp"
#include <algorithm>
#include <array>
#include <set>

using namespace mmb;

TEST_CASE("Diversification: Thread Configuration Asymmetry", "[concurrency][diversification]") {
    SECTION("Master thread (ID 0) runs exact base depth without offset") {
        const auto cfg = SearchDiversifier::configure(0, 8);
        REQUIRE(cfg.thread_id == 0);
        REQUIRE(cfg.target_depth == 8);
        REQUIRE(cfg.depth_offset == 0);
        REQUIRE(cfg.aspiration_delta == 0);
    }

    SECTION("Helper threads receive cyclic depth offsets and aspiration asymmetry") {
        const auto c1 = SearchDiversifier::configure(1, 8);
        const auto c2 = SearchDiversifier::configure(2, 8);
        const auto c3 = SearchDiversifier::configure(3, 8);
        const auto c4 = SearchDiversifier::configure(4, 8);

        REQUIRE(c1.target_depth == 9);
        REQUIRE(c1.depth_offset == 1);
        REQUIRE(c1.aspiration_delta != 0);

        REQUIRE(c2.target_depth == 8);
        REQUIRE(c2.depth_offset == 0);

        REQUIRE(c3.target_depth == 7);
        REQUIRE(c3.depth_offset == -1);

        REQUIRE(c4.target_depth == 9);
    }

    SECTION("Target depth clamps safely to at least 1 for shallow searches") {
        for (int tid = 0; tid < 8; ++tid) {
            const auto cfg = SearchDiversifier::configure(tid, 1);
            REQUIRE(cfg.target_depth >= 1);
        }
    }
}

TEST_CASE("Diversification: Move Ordering Perturbation & Invariants", "[concurrency][diversification]") {
    SECTION("Master thread maintains canonical center-outward ordering") {
        Board board;
        std::array<Column, COLS> moves{};
        const int count = SearchDiversifier::get_diversified_moves(board, 0, 0, moves);

        REQUIRE(count == 7);
        const std::array<Column, COLS> expected = {3, 2, 4, 1, 5, 0, 6};
        REQUIRE(moves == expected);
    }

    SECTION("Helper threads permute central candidates at shallow plies") {
        Board board;
        std::array<Column, COLS> master_moves{};
        std::array<Column, COLS> w1_moves{};
        std::array<Column, COLS> w2_moves{};

        const int c0 = SearchDiversifier::get_diversified_moves(board, 0, 0, master_moves);
        const int c1 = SearchDiversifier::get_diversified_moves(board, 1, 0, w1_moves);
        const int c2 = SearchDiversifier::get_diversified_moves(board, 2, 0, w2_moves);

        REQUIRE(c0 == 7);
        REQUIRE(c1 == 7);
        REQUIRE(c2 == 7);

        // Helpers differ from Master at root to prevent duplicate subtrees
        REQUIRE(w1_moves != master_moves);
        REQUIRE(w2_moves != master_moves);
        REQUIRE(w1_moves != w2_moves);

        // First moves stay in the central cluster {3, 2, 4}
        REQUIRE((w1_moves[0] == 2 || w1_moves[0] == 4));
        REQUIRE((w2_moves[0] == 2 || w2_moves[0] == 4));

        // All legal moves are preserved without duplicates
        std::set<Column> unique_moves(w1_moves.begin(), w1_moves.end());
        REQUIRE(unique_moves.size() == 7);
    }

    SECTION("Deep plies revert to canonical ordering to preserve pruning efficiency") {
        Board board;
        std::array<Column, COLS> moves{};
        const int count = SearchDiversifier::get_diversified_moves(board, 1, 3, moves);

        REQUIRE(count == 7);
        const std::array<Column, COLS> expected = {3, 2, 4, 1, 5, 0, 6};
        REQUIRE(moves == expected);
    }

    SECTION("Tactical Safety Invariant: Immediate winning move stays first across all threads") {
        Board board;
        // Player 1 stacks 3 discs in outer column 0
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        board.make_move(0); // P1
        board.make_move(1); // P2
        // P1 to move: Col 0 is an immediate vertical win!

        for (int tid = 0; tid < 8; ++tid) {
            std::array<Column, COLS> moves{};
            const int count = SearchDiversifier::get_diversified_moves(board, tid, 0, moves);
            REQUIRE(count == 7);
            REQUIRE(moves[0] == 0);
        }
    }

    SECTION("Tactical Safety Invariant: Urgent defensive block stays first across all threads") {
        Board board;
        // Player 1 stacks 3 discs in outer column 6, P2 to move must block Col 6
        board.make_move(6); // P1
        board.make_move(1); // P2
        board.make_move(6); // P1
        board.make_move(1); // P2
        board.make_move(6); // P1
        // Now P2 to move: Col 6 is an urgent defensive block!

        for (int tid = 0; tid < 8; ++tid) {
            std::array<Column, COLS> moves{};
            const int count = SearchDiversifier::get_diversified_moves(board, tid, 0, moves);
            REQUIRE(count == 7);
            REQUIRE(moves[0] == 6);
        }
    }

    SECTION("Handles full columns cleanly across all worker IDs") {
        Board board;
        // Fill center column 3 completely (6 moves)
        for (int i = 0; i < 6; ++i) {
            board.make_move(3);
        }

        for (int tid = 0; tid < 6; ++tid) {
            std::array<Column, COLS> moves{};
            const int count = SearchDiversifier::get_diversified_moves(board, tid, 1, moves);
            REQUIRE(count == 6);
            for (int i = 0; i < count; ++i) {
                REQUIRE(moves[i] != 3);
                REQUIRE(board.is_valid_move(moves[i]));
            }
        }
    }
}
