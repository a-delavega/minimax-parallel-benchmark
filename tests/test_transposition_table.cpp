#include <catch2/catch_test_macros.hpp>
#include "minimax/transposition/transposition_table.hpp"
#include <cstddef>
#include <thread>
#include <vector>
#include <atomic>

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

TEST_CASE("TTAtomicSlot: 16-Byte Size, Alignment & XOR Trick Correctness (Subphase 4.4)", "[tt][concurrency]") {
    STATIC_REQUIRE(sizeof(TTAtomicSlot) == 16);
    STATIC_REQUIRE(alignof(TTAtomicSlot) == 16);

    TTAtomicSlot slot;
    REQUIRE(slot.is_empty());

    const uint64_t key = 0xFEDCBA9876543210ULL;
    const Score score = 12345;
    const Column best_move = 3;
    const uint8_t depth = 10;
    const TTFlag flag = TTFlag::Exact;
    const uint8_t age = 4;

    const uint64_t data = TranspositionTable::encode_data(score, best_move, depth, flag, age);
    const uint64_t key_xor_data = key ^ data;

    slot.key_xor_data.store(key_xor_data, std::memory_order_relaxed);
    slot.data.store(data, std::memory_order_relaxed);
    REQUIRE_FALSE(slot.is_empty());

    // Valid read with XOR trick
    const uint64_t read_d = slot.data.load(std::memory_order_relaxed);
    const uint64_t read_k = slot.key_xor_data.load(std::memory_order_relaxed);
    REQUIRE((read_k ^ read_d) == key);

    // Simulated torn read: corrupt one bit of data
    const uint64_t corrupt_d = read_d ^ 0x1ULL;
    REQUIRE((read_k ^ corrupt_d) != key); // Torn read is rejected!
}

TEST_CASE("TranspositionTable: Concurrent Multi-Threaded Read/Write Stress Test (Subphase 4.4)", "[tt][concurrency]") {
    // 2 MB table (131072 entries)
    TranspositionTable tt(2);

    constexpr size_t NUM_THREADS = 8;
    constexpr size_t OPS_PER_THREAD = 10'000;

    std::atomic<bool> start_gate{false};
    std::atomic<size_t> torn_reads_detected{0};
    std::atomic<size_t> successful_hits{0};
    std::vector<std::thread> workers;
    workers.reserve(NUM_THREADS);

    for (size_t t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([&, t]() {
            while (!start_gate.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < OPS_PER_THREAD; ++i) {
                // Generate a key with thread id embedded and high bits
                const uint64_t key = (static_cast<uint64_t>(t) << 48) | (static_cast<uint64_t>(i % 500) << 16) | 0xA5A5ULL;
                const Score score = static_cast<Score>((key & 0x7FFF) ^ 0x1234);
                const Column move = static_cast<Column>((key >> 4) % 7);
                const uint8_t depth = static_cast<uint8_t>((key >> 8) % 20);
                const TTFlag flag = static_cast<TTFlag>((key >> 2) % 3);
                const uint8_t age = static_cast<uint8_t>(t & 0xFF);

                // Alternating write and probe
                tt.store(key, score, move, depth, flag, age);

                TTEntry probed;
                if (tt.probe(key, probed)) {
                    successful_hits.fetch_add(1, std::memory_order_relaxed);
                    // Critical verification: ensure data is never a torn mixture
                    const Score expected_score = static_cast<Score>((probed.zobrist_key & 0x7FFF) ^ 0x1234);
                    const Column expected_move = static_cast<Column>((probed.zobrist_key >> 4) % 7);
                    const uint8_t expected_depth = static_cast<uint8_t>((probed.zobrist_key >> 8) % 20);
                    const TTFlag expected_flag = static_cast<TTFlag>((probed.zobrist_key >> 2) % 3);

                    if (probed.score != expected_score ||
                        probed.best_move != expected_move ||
                        probed.depth != expected_depth ||
                        probed.get_flag() != expected_flag) {
                        torn_reads_detected.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }

    start_gate.store(true, std::memory_order_release);
    for (auto& w : workers) {
        w.join();
    }

    // Zero torn reads under heavy multithreaded contention
    REQUIRE(torn_reads_detected.load() == 0);
    REQUIRE(successful_hits.load() > 0);

    // Verify telemetry integrity under concurrency
    const auto stats = tt.stats();
    REQUIRE(stats.probes == stats.hits + stats.misses);
    REQUIRE(stats.stores == NUM_THREADS * OPS_PER_THREAD);
    REQUIRE(stats.entries_occupied > 0);
    REQUIRE(stats.hit_rate() > 0.0);
}

