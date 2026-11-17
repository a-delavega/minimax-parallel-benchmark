#include <catch2/catch_test_macros.hpp>
#include "minimax/transposition/zobrist.hpp"
#include "minimax/board.hpp"
#include <unordered_set>
#include <bit>

using namespace mmb;

TEST_CASE("Zobrist: Determinism & Custom Seed (Subphase 4.1)", "[zobrist][transposition]") {
    ZobristKeys table1(ZobristKeys::DEFAULT_SEED);
    ZobristKeys table2(ZobristKeys::DEFAULT_SEED);

    // Default global table must match table1
    REQUIRE(default_zobrist_keys.turn_key() == table1.turn_key());
    REQUIRE(table1.turn_key() == table2.turn_key());
    REQUIRE(table1.turn_key() != 0ULL);

    for (int p = 0; p < 2; ++p) {
        auto player = static_cast<Player>(p);
        for (int col = 0; col < COLS; ++col) {
            for (int row = 0; row < ROWS; ++row) {
                REQUIRE(table1.piece_key(player, col, row) == table2.piece_key(player, col, row));
                REQUIRE(default_zobrist_keys.piece_key(player, col, row) == table1.piece_key(player, col, row));
            }
        }
    }

    // Custom seed must generate different keys
    ZobristKeys custom_table(0x123456789ABCDEF0ULL);
    REQUIRE(custom_table.turn_key() != table1.turn_key());
    REQUIRE(custom_table.piece_key(Player::Player1, 3, 0) != table1.piece_key(Player::Player1, 3, 0));
}

TEST_CASE("Zobrist: Key Uniqueness Across All Board Cells & Turn (Subphase 4.1)", "[zobrist][transposition]") {
    std::unordered_set<uint64_t> keys;

    // Collect all 84 piece keys
    for (int p = 0; p < 2; ++p) {
        auto player = static_cast<Player>(p);
        for (int col = 0; col < COLS; ++col) {
            for (int row = 0; row < ROWS; ++row) {
                uint64_t k = default_zobrist_keys.piece_key(player, col, row);
                REQUIRE(k != 0ULL);
                auto [iter, inserted] = keys.insert(k);
                REQUIRE(inserted); // Every key must be globally distinct
            }
        }
    }

    // Add turn key (85th key)
    REQUIRE(default_zobrist_keys.turn_key() != 0ULL);
    auto [iter, inserted] = keys.insert(default_zobrist_keys.turn_key());
    REQUIRE(inserted);
    REQUIRE(keys.size() == 85);
}

TEST_CASE("Zobrist: Uniform Bit Distribution & Bounds Safety (Subphase 4.1)", "[zobrist][transposition]") {
    size_t total_bits = 0;
    size_t key_count = 0;

    for (int p = 0; p < 2; ++p) {
        auto player = static_cast<Player>(p);
        for (int col = 0; col < COLS; ++col) {
            for (int row = 0; row < ROWS; ++row) {
                uint64_t k = default_zobrist_keys.piece_key(player, col, row);
                int bits = std::popcount(k);
                // Each 64-bit random number should have roughly 32 bits set
                REQUIRE(bits >= 20);
                REQUIRE(bits <= 44);
                total_bits += static_cast<size_t>(bits);
                key_count++;
            }
        }
    }

    int turn_bits = std::popcount(default_zobrist_keys.turn_key());
    total_bits += static_cast<size_t>(turn_bits);
    key_count++;

    double avg_bits = static_cast<double>(total_bits) / static_cast<double>(key_count);
    REQUIRE(avg_bits >= 30.0);
    REQUIRE(avg_bits <= 34.0);

    // Bounds safety: out of bound queries return 0ULL without crash
    REQUIRE(default_zobrist_keys.piece_key(Player::None, 0, 0) == 0ULL);
    REQUIRE(default_zobrist_keys.piece_key(Player::Player1, -1, 0) == 0ULL);
    REQUIRE(default_zobrist_keys.piece_key(Player::Player1, 7, 0) == 0ULL);
    REQUIRE(default_zobrist_keys.piece_key(Player::Player1, 3, -1) == 0ULL);
    REQUIRE(default_zobrist_keys.piece_key(Player::Player1, 3, 6) == 0ULL);
}

TEST_CASE("Board: Incremental vs Full Scratch Hash Equivalence (Subphase 4.2)", "[zobrist][board]") {
    Board board;
    REQUIRE(board.zobrist_hash == 0ULL);
    REQUIRE(board.compute_zobrist_hash() == 0ULL);

    // Sequence of moves across various columns
    const std::vector<Column> moves = {3, 3, 2, 4, 3, 2, 4, 5, 1, 0, 6, 3};
    for (Column col : moves) {
        board.make_move(col);
        REQUIRE(board.zobrist_hash != 0ULL);
        REQUIRE(board.zobrist_hash == board.compute_zobrist_hash());
    }

    // Now unmake in reverse order and verify at every single step
    for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
        board.unmake_move(*it);
        REQUIRE(board.zobrist_hash == board.compute_zobrist_hash());
    }

    REQUIRE(board.zobrist_hash == 0ULL);
    REQUIRE(board.moves_played == 0);
    REQUIRE(board.current_player == Player::Player1);
}

TEST_CASE("Board: Transposition Invariance Under Move Permutations (Subphase 4.2)", "[zobrist][board]") {
    // Sequence A: P1 plays 3, P2 plays 2, P1 plays 4, P2 plays 3
    Board boardA;
    boardA.make_move(3); // P1
    boardA.make_move(2); // P2
    boardA.make_move(4); // P1
    boardA.make_move(3); // P2

    // Sequence B: P1 plays 4, P2 plays 3, P1 plays 3, P2 plays 2
    Board boardB;
    boardB.make_move(4); // P1
    boardB.make_move(3); // P2
    boardB.make_move(3); // P1
    boardB.make_move(2); // P2

    // Both board states have:
    // P1 pieces at: (col 4, row 0), (col 3, row 0) or (col 3, row 1)
    // Wait, in seq A: P1 plays 3 (col 3, row 0), P2 plays 2 (col 2, row 0), P1 plays 4 (col 4, row 0), P2 plays 3 (col 3, row 1)
    // In seq B: P1 plays 4 (col 4, row 0), P2 plays 3 (col 3, row 0), P1 plays 3 (col 3, row 1), P2 plays 2 (col 2, row 0)
    // In seq A, col 3 row 0 is P1, row 1 is P2. In seq B, col 3 row 0 is P2, row 1 is P1.
    // Those are NOT the same state! (gravity stacks pieces differently)
    REQUIRE(boardA.bitboard[0] != boardB.bitboard[0]);
    REQUIRE(boardA.zobrist_hash != boardB.zobrist_hash);

    // True independent column transposition:
    // Col 2 and Col 4 are independent stacks!
    // Seq 1: P1(col 2), P2(col 4), P1(col 4), P2(col 2)
    // Seq 2: P1(col 4), P2(col 2), P1(col 2), P2(col 4)
    Board b1, b2;
    b1.make_move(2); // P1 @ (2, 0)
    b1.make_move(4); // P2 @ (4, 0)
    b1.make_move(4); // P1 @ (4, 1)
    b1.make_move(2); // P2 @ (2, 1)

    b2.make_move(4); // P1 @ (4, 0)
    b2.make_move(2); // P2 @ (2, 0)
    b2.make_move(2); // P1 @ (2, 1)
    b2.make_move(4); // P2 @ (4, 1)
    // Wait! In b1: P1 has (2,0) and (4,1). In b2: P1 has (4,0) and (2,1). Also different!

    // Exact transposition with 2 independent columns:
    // P1 plays C, P2 plays D. P1 plays E, P2 plays F.
    // If we reorder pairs:
    // Seq X: P1(col 1), P2(col 3), P1(col 5), P2(col 6)
    // Seq Y: P1(col 5), P2(col 6), P1(col 1), P2(col 3)
    // In both:
    // P1 pieces: col 1 row 0, col 5 row 0.
    // P2 pieces: col 3 row 0, col 6 row 0.
    // Current player: P1. Moves played: 4.
    Board bx, by;
    bx.make_move(1);
    bx.make_move(3);
    bx.make_move(5);
    bx.make_move(6);

    by.make_move(5);
    by.make_move(6);
    by.make_move(1);
    by.make_move(3);

    REQUIRE(bx == by);
    REQUIRE(bx.zobrist_hash == by.zobrist_hash);
    REQUIRE(bx.zobrist_hash == bx.compute_zobrist_hash());
    REQUIRE(by.zobrist_hash == by.compute_zobrist_hash());
}

TEST_CASE("Board: Hash State Discrimination (Subphase 4.2)", "[zobrist][board]") {
    Board empty;
    Board b1;
    b1.make_move(3); // 1 move

    Board b2;
    b2.make_move(4); // 1 move in different column

    REQUIRE(b1.zobrist_hash != empty.zobrist_hash);
    REQUIRE(b2.zobrist_hash != empty.zobrist_hash);
    REQUIRE(b1.zobrist_hash != b2.zobrist_hash);

    // Turn difference on same piece layout
    Board b3 = b1;
    b3.current_player = Player::Player1; // Artificially different turn
    b3.zobrist_hash ^= default_zobrist_keys.turn_key();
    REQUIRE(b3.zobrist_hash != b1.zobrist_hash);
}
