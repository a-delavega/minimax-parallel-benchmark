#include <catch2/catch_test_macros.hpp>
#include "minimax/board.hpp"

using namespace mmb;

TEST_CASE("Board: Default Initialization", "[board]") {
    Board board;

    REQUIRE(board.bitboard[0] == 0ULL);
    REQUIRE(board.bitboard[1] == 0ULL);
    REQUIRE(board.occupied() == 0ULL);
    REQUIRE(board.moves_played == 0);
    REQUIRE(board.current_player == Player::Player1);
    REQUIRE_FALSE(board.is_draw());
    REQUIRE_FALSE(board.has_won(Player::Player1));
    REQUIRE_FALSE(board.has_won(Player::Player2));

    for (Column col = 0; col < COLS; ++col) {
        REQUIRE(board.is_valid_move(col));
        REQUIRE(board.height[col] == col * 7);
    }
}

TEST_CASE("Board: Gravity and Valid Moves", "[board]") {
    Board board;

    // Fill column 0
    for (int r = 0; r < ROWS; ++r) {
        REQUIRE(board.is_valid_move(0));
        board.make_move(0);
        REQUIRE(board.moves_played == r + 1);
    }

    // Column 0 is now full (6 pieces)
    REQUIRE_FALSE(board.is_valid_move(0));

    // Other columns remain valid
    for (Column col = 1; col < COLS; ++col) {
        REQUIRE(board.is_valid_move(col));
    }

    // Verify sentinel bit (bit 6 for column 0) was NOT set
    REQUIRE((board.occupied() & (1ULL << 6)) == 0ULL);
}

TEST_CASE("Board: Vertical Win Detection", "[board]") {
    Board board;

    // Player 1 in col 0, Player 2 in col 1
    // Turns: P1(0), P2(1), P1(0), P2(1), P1(0), P2(1), P1(0) -> P1 wins vertically
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    REQUIRE_FALSE(board.has_won(Player::Player1));

    board.make_move(0); // P1 (4th vertical piece)
    REQUIRE(board.has_won(Player::Player1));
    REQUIRE_FALSE(board.has_won(Player::Player2));
}

TEST_CASE("Board: Horizontal Win Detection", "[board]") {
    Board board;

    // P1 plays in cols 0, 1, 2, 3 at row 0
    // P2 plays in cols 0, 1, 2 at row 1
    board.make_move(0); // P1 (col 0, row 0)
    board.make_move(0); // P2 (col 0, row 1)
    board.make_move(1); // P1 (col 1, row 0)
    board.make_move(1); // P2 (col 1, row 1)
    board.make_move(2); // P1 (col 2, row 0)
    board.make_move(2); // P2 (col 2, row 1)
    REQUIRE_FALSE(board.has_won(Player::Player1));

    board.make_move(3); // P1 (col 3, row 0) -> Horizontal 4 in a row!
    REQUIRE(board.has_won(Player::Player1));
    REQUIRE_FALSE(board.has_won(Player::Player2));
}

TEST_CASE("Board: Diagonal Ascending Win (/)", "[board]") {
    Board board;

    // Goal: P1 gets (0,0), (1,1), (2,2), (3,3)
    // Col 0: P1(0)
    // Col 1: P2(0), P1(1)
    // Col 2: P2(0), P2(1), P1(2)
    // Col 3: P2(0), P2(1), P2(2), P1(3)
    board.make_move(0); // P1 at (0,0)
    board.make_move(1); // P2 at (1,0)
    board.make_move(1); // P1 at (1,1)
    board.make_move(2); // P2 at (2,0)
    board.make_move(3); // P1 at (3,0)
    board.make_move(2); // P2 at (2,1)
    board.make_move(2); // P1 at (2,2)
    board.make_move(3); // P2 at (3,1)
    board.make_move(4); // P1 at (4,0) - throwaway
    board.make_move(3); // P2 at (3,2)
    REQUIRE_FALSE(board.has_won(Player::Player1));

    board.make_move(3); // P1 at (3,3) -> Diagonal / win!
    REQUIRE(board.has_won(Player::Player1));
    REQUIRE_FALSE(board.has_won(Player::Player2));
}

TEST_CASE("Board: Diagonal Descending Win (\\)", "[board]") {
    Board board;

    // Goal: P1 gets (0,3), (1,2), (2,1), (3,0)
    board.make_move(3); // P1 at (3,0)
    board.make_move(2); // P2 at (2,0)
    board.make_move(2); // P1 at (2,1)
    board.make_move(1); // P2 at (1,0)
    board.make_move(0); // P1 at (0,0)
    board.make_move(1); // P2 at (1,1)
    board.make_move(1); // P1 at (1,2)
    board.make_move(0); // P2 at (0,1)
    board.make_move(4); // P1 at (4,0) - throwaway
    board.make_move(0); // P2 at (0,2)
    REQUIRE_FALSE(board.has_won(Player::Player1));

    board.make_move(0); // P1 at (0,3) -> Diagonal \ win!
    REQUIRE(board.has_won(Player::Player1));
    REQUIRE_FALSE(board.has_won(Player::Player2));
}

TEST_CASE("Board: Sentinel Isolation Across Column Boundaries", "[board]") {
    Board board;

    // Place 3 pieces in column 0 at rows 3, 4, 5 (bits 3, 4, 5 - no vertical win)
    board.bitboard[0] |= (1ULL << 3) | (1ULL << 4) | (1ULL << 5);
    // Place 2 pieces in column 1 at rows 0, 1 (bits 7, 8 - no vertical win)
    board.bitboard[0] |= (1ULL << 7) | (1ULL << 8);

    // Sentinel bit at 6 must be 0
    REQUIRE((board.bitboard[0] & (1ULL << 6)) == 0ULL);
    // Must NOT detect any win across boundary
    REQUIRE_FALSE(Board::has_won(board.bitboard[0]));
}
