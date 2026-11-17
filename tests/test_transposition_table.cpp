#include <catch2/catch_test_macros.hpp>
#include "minimax/transposition/transposition_table.hpp"
#include <cstddef>

using namespace mmb;

TEST_CASE("TTEntry: Cache Line Alignment & 16-Byte Packing (Subphase 4.3)", "[tt][transposition]") {
    STATIC_REQUIRE(sizeof(TTEntry) == 16);
    STATIC_REQUIRE(alignof(TTEntry) == 16);

    // Verify exact struct member offsets to guarantee zero compiler holes
    STATIC_REQUIRE(offsetof(TTEntry, zobrist_key) == 0);
    STATIC_REQUIRE(offsetof(TTEntry, score) == 8);
    STATIC_REQUIRE(offsetof(TTEntry, best_move) == 12);
    STATIC_REQUIRE(offsetof(TTEntry, depth) == 13);
    STATIC_REQUIRE(offsetof(TTEntry, flag) == 14);
    STATIC_REQUIRE(offsetof(TTEntry, age) == 15);

    // Verify default empty entry
    TTEntry entry;
    REQUIRE(entry.is_empty());
    REQUIRE(entry.zobrist_key == 0ULL);
    REQUIRE(entry.best_move == 255);
    REQUIRE(entry.depth == 0);
    REQUIRE(entry.score == 0);
    REQUIRE(entry.get_flag() == TTFlag::Exact);
}

TEST_CASE("TranspositionTable: Power-of-Two Sizing & Memory Layout (Subphase 4.3)", "[tt][transposition]") {
    // 1 MB table: 1048576 bytes / 16 bytes per entry = 65536 entries = 2^16
    TranspositionTable tt(1);
    REQUIRE(tt.capacity() == 65536);
    REQUIRE(tt.mask() == 65535);
    REQUIRE((tt.capacity() & (tt.capacity() - 1)) == 0); // Strict power of 2
    REQUIRE(tt.memory_bytes() == 65536 * sizeof(TTEntry));
    REQUIRE(tt.occupancy() == 0.0);
    REQUIRE(tt.hit_rate() == 0.0);

    // Custom entry count sizing
    tt.resize_entries(1000);
    REQUIRE(tt.capacity() == 512); // Nearest power of 2 <= 1000
    REQUIRE(tt.mask() == 511);
}

TEST_CASE("TranspositionTable: Store & Probe Round-Trip (Subphase 4.3)", "[tt][transposition]") {
    TranspositionTable tt(1);
    const uint64_t key1 = 0xA1B2C3D4E5F60718ULL;
    const Score score1 = 450;
    const Column move1 = 3;
    const uint8_t depth1 = 8;
    const TTFlag flag1 = TTFlag::Exact;
    const uint8_t age1 = 2;

    TTEntry probed;
    REQUIRE_FALSE(tt.probe(key1, probed));
    REQUIRE(tt.stats().misses == 1);
    REQUIRE(tt.stats().hits == 0);

    tt.store(key1, score1, move1, depth1, flag1, age1);
    REQUIRE(tt.stats().stores == 1);
    REQUIRE(tt.stats().entries_occupied == 1);
    REQUIRE(tt.occupancy() > 0.0);

    // Probe must succeed and match stored data exactly
    REQUIRE(tt.probe(key1, probed));
    REQUIRE(tt.stats().hits == 1);
    REQUIRE(tt.stats().probes == 2);
    REQUIRE(tt.hit_rate() == 0.5);

    REQUIRE(probed.zobrist_key == key1);
    REQUIRE(probed.score == score1);
    REQUIRE(probed.best_move == move1);
    REQUIRE(probed.depth == depth1);
    REQUIRE(probed.get_flag() == flag1);
    REQUIRE(probed.age == age1);
    REQUIRE_FALSE(probed.is_empty());
}

TEST_CASE("TranspositionTable: Flag Types & Bounds (Subphase 4.3)", "[tt][transposition]") {
    TranspositionTable tt(1);
    const uint64_t keyLower = 0x1111222233334444ULL;
    const uint64_t keyUpper = 0x5555666677778888ULL;

    tt.store(keyLower, 1000, 2, 6, TTFlag::LowerBound, 0);
    tt.store(keyUpper, -500, 4, 5, TTFlag::UpperBound, 0);

    TTEntry entryLower, entryUpper;
    REQUIRE(tt.probe(keyLower, entryLower));
    REQUIRE(tt.probe(keyUpper, entryUpper));

    REQUIRE(entryLower.get_flag() == TTFlag::LowerBound);
    REQUIRE(entryLower.score == 1000);
    REQUIRE(entryUpper.get_flag() == TTFlag::UpperBound);
    REQUIRE(entryUpper.score == -500);
}

TEST_CASE("TranspositionTable: Collisions, Overwrites & Telemetry (Subphase 4.3)", "[tt][transposition]") {
    TranspositionTable tt(1);
    const size_t cap = tt.capacity();

    // Two keys that intentionally share the same slot (identical low mask bits)
    const uint64_t slot_idx = 42;
    const uint64_t keyA = slot_idx;
    const uint64_t keyB = slot_idx + cap; // keyB & mask == slot_idx

    REQUIRE((keyA & tt.mask()) == (keyB & tt.mask()));

    tt.store(keyA, 100, 3, 4, TTFlag::Exact, 0);
    REQUIRE(tt.stats().entries_occupied == 1);

    // Probing keyB will hash to same slot but key won't match -> collision detected!
    TTEntry probed;
    REQUIRE_FALSE(tt.probe(keyB, probed));
    REQUIRE(tt.stats().collisions == 1);
    REQUIRE(tt.stats().misses == 1);

    // Overwrite slot with keyB
    tt.store(keyB, 200, 4, 6, TTFlag::Exact, 0);
    REQUIRE(tt.stats().overwrites == 1);
    REQUIRE(tt.stats().entries_occupied == 1);

    // Probing keyB now hits
    REQUIRE(tt.probe(keyB, probed));
    REQUIRE(probed.score == 200);

    // Clear operation
    tt.clear();
    REQUIRE(tt.occupancy() == 0.0);
    REQUIRE(tt.stats().entries_occupied == 0);
    REQUIRE(tt.stats().probes == 0);
    REQUIRE_FALSE(tt.probe(keyB, probed));
}
