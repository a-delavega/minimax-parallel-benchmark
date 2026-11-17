#include <catch2/catch_test_macros.hpp>
#include "minimax/transposition/zobrist.hpp"
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
