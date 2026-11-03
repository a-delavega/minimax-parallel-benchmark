#include <catch2/catch_test_macros.hpp>
#include "minimax/move_orderer.hpp"

using namespace mmb;

TEST_CASE("MoveOrderer: Default Center-Outward Sequence", "[move_orderer]") {
    Board board;
    std::array<Column, COLS> moves{};

    const int count = MoveOrderer::get_ordered_moves(board, moves);
    REQUIRE(count == 7);
    REQUIRE(moves[0] == 3);
    REQUIRE(moves[1] == 2);
    REQUIRE(moves[2] == 4);
    REQUIRE(moves[3] == 1);
    REQUIRE(moves[4] == 5);
    REQUIRE(moves[5] == 0);
    REQUIRE(moves[6] == 6);
}

TEST_CASE("MoveOrderer: Skips Full Columns", "[move_orderer]") {
    Board board;

    // Fill column 3 (center)
    for (int r = 0; r < ROWS; ++r) {
        board.make_move(3);
    }
    REQUIRE_FALSE(board.is_valid_move(3));

    std::array<Column, COLS> moves{};
    const int count = MoveOrderer::get_ordered_moves(board, moves);

    REQUIRE(count == 6);
    // Col 3 should be skipped, so col 2 is first
    REQUIRE(moves[0] == 2);
    REQUIRE(moves[1] == 4);
    REQUIRE(moves[2] == 1);
    REQUIRE(moves[3] == 5);
    REQUIRE(moves[4] == 0);
    REQUIRE(moves[5] == 6);
}

TEST_CASE("MoveOrderer: Full Board Has Zero Moves", "[move_orderer]") {
    Board board;

    // Fill all columns
    for (Column c = 0; c < COLS; ++c) {
        for (int r = 0; r < ROWS; ++r) {
            board.make_move(c);
        }
    }

    std::array<Column, COLS> moves{};
    const int count = MoveOrderer::get_ordered_moves(board, moves);
    REQUIRE(count == 0);
}

TEST_CASE("MoveOrderer: Winning Move Prioritized First", "[move_orderer]") {
    Board board;
    // Set up 3 pieces in column 0 for Player 1 (winning move would be col 0)
    // Normally column 0 is the SECOND-TO-LAST in center-outward order [3,2,4,1,5,0,6].
    board.make_move(0); // P1
    board.make_move(6); // P2
    board.make_move(0); // P1
    board.make_move(6); // P2
    board.make_move(0); // P1
    board.make_move(6); // P2

    // It's P1's turn. Col 0 is an immediate winning move.
    std::array<Column, COLS> moves{};
    const int count = MoveOrderer::get_ordered_moves(board, moves);
    REQUIRE(count == 7);
    // Col 0 MUST be prioritized to index 0 ahead of col 3!
    REQUIRE(moves[0] == 0);
}

TEST_CASE("MoveOrderer: Defensive Block Prioritized First", "[move_orderer]") {
    Board board;
    // Set up 3 pieces in column 0 for Player 1
    board.make_move(0); // P1
    board.make_move(6); // P2
    board.make_move(0); // P1
    board.make_move(6); // P2
    board.make_move(0); // P1

    // It's P2's turn. P1 threatens win at col 0. P2 has no immediate win.
    // P2 MUST prioritize blocking col 0 to avert defeat!
    std::array<Column, COLS> moves{};
    const int count = MoveOrderer::get_ordered_moves(board, moves);
    REQUIRE(count == 7);
    REQUIRE(moves[0] == 0);
}
