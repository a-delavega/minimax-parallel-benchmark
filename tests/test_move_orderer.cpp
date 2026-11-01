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
