#pragma once

#include "minimax/types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <bit>
#include <algorithm>
#include <atomic>

namespace mmb {

/**
 * @brief Transposition Table Entry Flag indicating the bound type of the stored score.
 */
enum class TTFlag : uint8_t {
    Exact = 0,       ///< Exact minimax score (PV-node: alpha < score < beta)
    LowerBound = 1,  ///< Fail-high cutoff (beta-cutoff: score >= beta)
    UpperBound = 2   ///< Fail-low cutoff (all moves < alpha: score <= alpha)
};

/**
 * @brief Replacement policy when a collision occurs at a transposition table slot.
 */
enum class ReplacementPolicy : uint8_t {
    AlwaysReplace = 0,   ///< Always overwrite slot with the incoming entry
    DepthPreferred = 1,  ///< Overwrite only if incoming entry has higher depth or younger generation
    TwoTier = 2          ///< 2-slot bucket: Slot 0 (Depth-Preferred), Slot 1 (Always-Replace / Recent)
};

/**
 * @brief Cache-aligned Transposition Table Entry (exactly 16 bytes).
 * 
 * Memory Layout:
 * - [0..7]   uint64_t zobrist_key (8 bytes) : Full 64-bit verification key
 * - [8..11]  int32_t  score       (4 bytes) : Evaluation score [-100'000, 100'000]
 * - [12]     uint8_t  best_move   (1 byte)  : Suggested column [0..6], or 255 if none
 * - [13]     uint8_t  depth       (1 byte)  : Search depth reached [0..42]
 * - [14]     uint8_t  flag        (1 byte)  : TTFlag bound type
 * - [15]     uint8_t  age         (1 byte)  : Search generation counter for replacement
 * 
 * Exactly 4 entries fit per 64-byte CPU cache line (L1/L2), eliminating
 * cache line splitting and optimizing hardware prefetching.
 */
struct alignas(16) TTEntry {
    uint64_t zobrist_key = 0ULL;
    int32_t  score       = 0;
    uint8_t  best_move   = 255;
    uint8_t  depth       = 0;
    uint8_t  flag        = 0;
    uint8_t  age         = 0;

    [[nodiscard]] constexpr TTFlag get_flag() const noexcept {
        return static_cast<TTFlag>(flag);
    }

    constexpr void set_flag(TTFlag f) noexcept {
        flag = static_cast<uint8_t>(f);
    }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return zobrist_key == 0ULL;
    }
};

static_assert(sizeof(TTEntry) == 16, "TTEntry must be exactly 16 bytes for 4-entry cache line packing");
static_assert(alignof(TTEntry) == 16, "TTEntry must be 16-byte aligned");

/**
 * @brief Lock-free atomic storage slot using Hyatt's XOR verification trick.
 * 
 * Stored fields:
 * - key_xor_data: std::atomic<uint64_t> (key ^ data)
 * - data:         std::atomic<uint64_t> (packed score, best_move, depth, flag, age)
 * 
 * Guarantees zero torn-read corruption under concurrent multithreaded Lazy SMP.
 * Any partial/interleaved write causes (key_xor_data ^ data) != key, which
 * immediately triggers an undetected cache miss rather than returning corrupt data.
 */
struct alignas(16) TTAtomicSlot {
    std::atomic<uint64_t> key_xor_data{0ULL};
    std::atomic<uint64_t> data{0ULL};

    TTAtomicSlot() noexcept = default;

    TTAtomicSlot(const TTAtomicSlot& other) noexcept {
        key_xor_data.store(other.key_xor_data.load(std::memory_order_relaxed), std::memory_order_relaxed);
        data.store(other.data.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    TTAtomicSlot& operator=(const TTAtomicSlot& other) noexcept {
        if (this != &other) {
            key_xor_data.store(other.key_xor_data.load(std::memory_order_relaxed), std::memory_order_relaxed);
            data.store(other.data.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    [[nodiscard]] bool is_empty() const noexcept {
        return key_xor_data.load(std::memory_order_relaxed) == 0ULL &&
               data.load(std::memory_order_relaxed) == 0ULL;
    }
};

static_assert(sizeof(TTAtomicSlot) == 16, "TTAtomicSlot must be exactly 16 bytes for 4-entry cache line packing");
static_assert(alignof(TTAtomicSlot) == 16, "TTAtomicSlot must be 16-byte aligned");

/**
 * @brief High-performance Lock-Free Transposition Table with configurable replacement policies.
 * 
 * Features:
 * - O(1) probe and store with bitwise power-of-two masking (zero division instructions).
 * - Lock-free concurrency with Hyatt XOR verification (immune to torn reads).
 * - Configurable replacement policies: AlwaysReplace, DepthPreferred, TwoTier (2-slot buckets).
 * - Generational search aging (age counters & new_search() generational promotion).
 * - Real-time thread-safe telemetric monitoring.
 */
class TranspositionTable {
public:
    static constexpr size_t DEFAULT_SIZE_MB = 16;
    static constexpr uint8_t NO_MOVE = 255;

    struct Stats {
        size_t capacity = 0;
        std::atomic<size_t> entries_occupied{0};
        std::atomic<uint64_t> probes{0};
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> collisions{0};
        std::atomic<uint64_t> stores{0};
        std::atomic<uint64_t> overwrites{0};
        std::atomic<uint64_t> rejections{0};

        Stats() noexcept = default;

        Stats(const Stats& other) noexcept
            : capacity(other.capacity),
              entries_occupied(other.entries_occupied.load(std::memory_order_relaxed)),
              probes(other.probes.load(std::memory_order_relaxed)),
              hits(other.hits.load(std::memory_order_relaxed)),
              misses(other.misses.load(std::memory_order_relaxed)),
              collisions(other.collisions.load(std::memory_order_relaxed)),
              stores(other.stores.load(std::memory_order_relaxed)),
              overwrites(other.overwrites.load(std::memory_order_relaxed)),
              rejections(other.rejections.load(std::memory_order_relaxed)) {}

        Stats& operator=(const Stats& other) noexcept {
            if (this != &other) {
                capacity = other.capacity;
                entries_occupied.store(other.entries_occupied.load(std::memory_order_relaxed), std::memory_order_relaxed);
                probes.store(other.probes.load(std::memory_order_relaxed), std::memory_order_relaxed);
                hits.store(other.hits.load(std::memory_order_relaxed), std::memory_order_relaxed);
                misses.store(other.misses.load(std::memory_order_relaxed), std::memory_order_relaxed);
                collisions.store(other.collisions.load(std::memory_order_relaxed), std::memory_order_relaxed);
                stores.store(other.stores.load(std::memory_order_relaxed), std::memory_order_relaxed);
                overwrites.store(other.overwrites.load(std::memory_order_relaxed), std::memory_order_relaxed);
                rejections.store(other.rejections.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return *this;
        }

        [[nodiscard]] double hit_rate() const noexcept {
            const uint64_t p = probes.load(std::memory_order_relaxed);
            const uint64_t h = hits.load(std::memory_order_relaxed);
            return (p == 0) ? 0.0 : static_cast<double>(h) / static_cast<double>(p);
        }

        [[nodiscard]] double occupancy() const noexcept {
            const size_t occ = entries_occupied.load(std::memory_order_relaxed);
            return (capacity == 0) ? 0.0 : static_cast<double>(occ) / static_cast<double>(capacity);
        }
    };

    /**
     * @brief Constructs a Transposition Table with the given size in Megabytes and replacement policy.
     * @param size_mb Memory size in megabytes (default: 16 MB).
     * @param policy Replacement policy (default: DepthPreferred).
     */
    explicit TranspositionTable(size_t size_mb = DEFAULT_SIZE_MB, ReplacementPolicy policy = ReplacementPolicy::DepthPreferred)
        : policy_(policy) {
        resize_mb(size_mb);
    }

    /**
     * @brief Resizes table to the nearest power of 2 entries fitting in size_mb.
     */
    void resize_mb(size_t size_mb) {
        if (size_mb == 0) size_mb = 1;
        const size_t total_bytes = size_mb * 1024 * 1024;
        const size_t raw_entries = total_bytes / sizeof(TTAtomicSlot);
        resize_entries(raw_entries);
    }

    /**
     * @brief Resizes table to the nearest power of 2 greater than or equal to count.
     */
    void resize_entries(size_t count) {
        if (count < 2) count = 2;
        size_t power_of_two = 1;
        while (power_of_two * 2 <= count) {
            power_of_two *= 2;
        }
        capacity_ = power_of_two;
        mask_ = capacity_ - 1;
        table_ = std::vector<TTAtomicSlot>(capacity_);
        clear_stats();
    }

    /**
     * @brief Probes table for an entry matching the given 64-bit Zobrist key in O(1).
     * @param key 64-bit Zobrist key of the board state.
     * @param out_entry Output parameter receiving the found entry.
     * @return true if an entry with matching key was found, false otherwise.
     */
    [[nodiscard]] bool probe(uint64_t key, TTEntry& out_entry) noexcept {
        stats_.probes.fetch_add(1, std::memory_order_relaxed);

        if (policy_ == ReplacementPolicy::TwoTier && capacity_ >= 2) {
            const size_t bucket_mask = (capacity_ / 2) - 1;
            const size_t slot0_idx = (key & bucket_mask) * 2;
            const size_t slot1_idx = slot0_idx + 1;

            // Probe Slot 0 (Deep)
            if (probe_slot(table_[slot0_idx], key, out_entry)) {
                stats_.hits.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            // Probe Slot 1 (Recent)
            if (probe_slot(table_[slot1_idx], key, out_entry)) {
                stats_.hits.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            stats_.misses.fetch_add(1, std::memory_order_relaxed);
            if (!table_[slot0_idx].is_empty() || !table_[slot1_idx].is_empty()) {
                stats_.collisions.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        // Single-tier (AlwaysReplace or DepthPreferred)
        const size_t index = key & mask_;
        const auto& slot = table_[index];

        if (probe_slot(slot, key, out_entry)) {
            stats_.hits.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        if (!slot.is_empty()) {
            stats_.collisions.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }

    /**
     * @brief Stores an evaluation entry into the table in O(1) guided by replacement policy.
     * @param key 64-bit Zobrist key.
     * @param score Search evaluation score.
     * @param best_move Best suggested move [0..6] or NO_MOVE (255).
     * @param depth Search depth reached.
     * @param flag TTFlag bound type.
     * @param age Search age / generation counter (0 to use current table age).
     */
    void store(uint64_t key, Score score, Column best_move, uint8_t depth, TTFlag flag, uint8_t age = 0) noexcept {
        stats_.stores.fetch_add(1, std::memory_order_relaxed);
        if (age == 0) {
            age = current_age_.load(std::memory_order_relaxed);
        }

        if (policy_ == ReplacementPolicy::TwoTier && capacity_ >= 2) {
            store_two_tier(key, score, best_move, depth, flag, age);
            return;
        }

        const size_t index = key & mask_;
        auto& slot = table_[index];

        const uint64_t old_d = slot.data.load(std::memory_order_relaxed);
        const uint64_t old_k = slot.key_xor_data.load(std::memory_order_relaxed);
        const bool was_empty = (old_d == 0ULL && old_k == 0ULL);

        if (was_empty) {
            stats_.entries_occupied.fetch_add(1, std::memory_order_relaxed);
            write_slot(slot, key, score, best_move, depth, flag, age);
            return;
        }

        const uint64_t old_key = old_k ^ old_d;

        if (policy_ == ReplacementPolicy::AlwaysReplace) {
            if (old_key != key) {
                stats_.overwrites.fetch_add(1, std::memory_order_relaxed);
            }
            write_slot(slot, key, score, best_move, depth, flag, age);
            return;
        }

        // DepthPreferred policy with Generational Aging
        int32_t ex_score = 0;
        uint8_t ex_move = 255;
        uint8_t ex_depth = 0;
        uint8_t ex_flag = 0;
        uint8_t ex_age = 0;
        decode_data(old_d, ex_score, ex_move, ex_depth, ex_flag, ex_age);

        if (old_key == key) {
            // Same position: upgrade if depth is at least equal or exact flag
            if (depth >= ex_depth || flag == TTFlag::Exact || age > ex_age) {
                write_slot(slot, key, score, best_move, depth, flag, age);
            } else {
                stats_.rejections.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }

        // Different position collision: compare depth discounted by generational age
        const int age_diff = static_cast<int>(age) - static_cast<int>(ex_age);
        const int effective_ex_depth = std::max(0, static_cast<int>(ex_depth) - std::max(0, age_diff) * 2);

        if (static_cast<int>(depth) >= effective_ex_depth) {
            stats_.overwrites.fetch_add(1, std::memory_order_relaxed);
            write_slot(slot, key, score, best_move, depth, flag, age);
        } else {
            stats_.rejections.fetch_add(1, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Advances generation age counter for Iterative Deepening and multi-move aging.
     */
    void new_search() noexcept {
        current_age_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Manually sets generation age counter.
     */
    void set_age(uint8_t age) noexcept {
        current_age_.store(age, std::memory_order_relaxed);
    }

    [[nodiscard]] uint8_t current_age() const noexcept {
        return current_age_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] ReplacementPolicy replacement_policy() const noexcept {
        return policy_;
    }

    void set_replacement_policy(ReplacementPolicy policy) noexcept {
        policy_ = policy;
    }

    /**
     * @brief Clears all entries in the table and resets telemetry counters.
     */
    void clear() noexcept {
        for (auto& slot : table_) {
            slot.key_xor_data.store(0ULL, std::memory_order_relaxed);
            slot.data.store(0ULL, std::memory_order_relaxed);
        }
        clear_stats();
    }

    /**
     * @brief Resets only the query telemetry counters, preserving stored entries.
     */
    void clear_stats() noexcept {
        stats_.probes.store(0, std::memory_order_relaxed);
        stats_.hits.store(0, std::memory_order_relaxed);
        stats_.misses.store(0, std::memory_order_relaxed);
        stats_.collisions.store(0, std::memory_order_relaxed);
        stats_.stores.store(0, std::memory_order_relaxed);
        stats_.overwrites.store(0, std::memory_order_relaxed);
        stats_.rejections.store(0, std::memory_order_relaxed);
        stats_.capacity = capacity_;

        size_t occ = 0;
        for (const auto& slot : table_) {
            if (!slot.is_empty()) occ++;
        }
        stats_.entries_occupied.store(occ, std::memory_order_relaxed);
    }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t mask() const noexcept { return mask_; }
    [[nodiscard]] size_t memory_bytes() const noexcept { return capacity_ * sizeof(TTAtomicSlot); }
    [[nodiscard]] Stats stats() const noexcept { return stats_; }
    [[nodiscard]] double hit_rate() const noexcept { return stats_.hit_rate(); }
    [[nodiscard]] double occupancy() const noexcept { return stats_.occupancy(); }

    /**
     * @brief Normalizes mate scores to distance-independent root-relative scores before storing.
     */
    [[nodiscard]] static constexpr Score score_to_tt(Score score, int ply) noexcept {
        if (score >= MATE_THRESHOLD) return score + ply;
        if (score <= -MATE_THRESHOLD) return score - ply;
        return score;
    }

    /**
     * @brief Denormalizes stored mate scores back to current ply distance after probing.
     */
    [[nodiscard]] static constexpr Score score_from_tt(Score score, int ply) noexcept {
        if (score >= MATE_THRESHOLD) return score - ply;
        if (score <= -MATE_THRESHOLD) return score + ply;
        return score;
    }

    /**
     * @brief Packs score, best_move, depth, flag, and age into a single 64-bit word.
     */
    static constexpr uint64_t encode_data(Score score, Column best_move, uint8_t depth, TTFlag flag, uint8_t age) noexcept {
        const uint64_t s = static_cast<uint64_t>(static_cast<uint32_t>(score));
        const uint64_t bm = static_cast<uint64_t>(static_cast<uint8_t>(best_move));
        const uint64_t d = static_cast<uint64_t>(depth);
        const uint64_t f = static_cast<uint64_t>(static_cast<uint8_t>(flag));
        const uint64_t a = static_cast<uint64_t>(age);
        return (s & 0xFFFFFFFFULL) | (bm << 32) | (d << 40) | (f << 48) | (a << 56);
    }

    /**
     * @brief Unpacks score, best_move, depth, flag, and age from a 64-bit word.
     */
    static constexpr void decode_data(uint64_t d, int32_t& score, uint8_t& best_move, uint8_t& depth, uint8_t& flag, uint8_t& age) noexcept {
        score = static_cast<int32_t>(static_cast<uint32_t>(d & 0xFFFFFFFFULL));
        best_move = static_cast<uint8_t>((d >> 32) & 0xFFULL);
        depth = static_cast<uint8_t>((d >> 40) & 0xFFULL);
        flag = static_cast<uint8_t>((d >> 48) & 0xFFULL);
        age = static_cast<uint8_t>((d >> 56) & 0xFFULL);
    }

    // Direct access to underlying slot for advanced concurrency / unit testing
    [[nodiscard]] const TTAtomicSlot& raw_slot(size_t index) const noexcept {
        return table_[index & mask_];
    }
    [[nodiscard]] TTAtomicSlot& raw_slot(size_t index) noexcept {
        return table_[index & mask_];
    }

private:
    static bool probe_slot(const TTAtomicSlot& slot, uint64_t key, TTEntry& out_entry) noexcept {
        const uint64_t d = slot.data.load(std::memory_order_relaxed);
        const uint64_t k = slot.key_xor_data.load(std::memory_order_relaxed);

        if ((k ^ d) == key && !slot.is_empty()) {
            out_entry.zobrist_key = key;
            decode_data(d, out_entry.score, out_entry.best_move, out_entry.depth, out_entry.flag, out_entry.age);
            return true;
        }
        return false;
    }

    static void write_slot(TTAtomicSlot& slot, uint64_t key, Score score, Column best_move, uint8_t depth, TTFlag flag, uint8_t age) noexcept {
        const uint64_t new_d = encode_data(score, best_move, depth, flag, age);
        const uint64_t new_k = key ^ new_d;
        slot.key_xor_data.store(new_k, std::memory_order_relaxed);
        slot.data.store(new_d, std::memory_order_relaxed);
    }

    void store_two_tier(uint64_t key, Score score, Column best_move, uint8_t depth, TTFlag flag, uint8_t age) noexcept {
        const size_t bucket_mask = (capacity_ / 2) - 1;
        const size_t slot0_idx = (key & bucket_mask) * 2;
        const size_t slot1_idx = slot0_idx + 1;

        auto& deep_slot = table_[slot0_idx];
        auto& recent_slot = table_[slot1_idx];

        const uint64_t d0 = deep_slot.data.load(std::memory_order_relaxed);
        const uint64_t k0 = deep_slot.key_xor_data.load(std::memory_order_relaxed);
        const uint64_t key0 = k0 ^ d0;
        const bool empty0 = deep_slot.is_empty();

        const uint64_t d1 = recent_slot.data.load(std::memory_order_relaxed);
        const uint64_t k1 = recent_slot.key_xor_data.load(std::memory_order_relaxed);
        const uint64_t key1 = k1 ^ d1;
        const bool empty1 = recent_slot.is_empty();

        // Case 1: Key matches Deep Slot
        if (!empty0 && key0 == key) {
            uint8_t ex_depth = static_cast<uint8_t>((d0 >> 40) & 0xFFULL);
            uint8_t ex_age = static_cast<uint8_t>((d0 >> 56) & 0xFFULL);
            if (depth >= ex_depth || age > ex_age) {
                write_slot(deep_slot, key, score, best_move, depth, flag, age);
            } else {
                write_slot(recent_slot, key, score, best_move, depth, flag, age);
            }
            return;
        }

        // Case 2: Key matches Recent Slot
        if (!empty1 && key1 == key) {
            uint8_t deep_depth = empty0 ? 0 : static_cast<uint8_t>((d0 >> 40) & 0xFFULL);
            if (depth > deep_depth) {
                // Promote to deep slot and demote old deep to recent slot
                if (!empty0) {
                    recent_slot.key_xor_data.store(k0, std::memory_order_relaxed);
                    recent_slot.data.store(d0, std::memory_order_relaxed);
                }
                write_slot(deep_slot, key, score, best_move, depth, flag, age);
            } else {
                write_slot(recent_slot, key, score, best_move, depth, flag, age);
            }
            return;
        }

        // Case 3: Empty slot available
        if (empty0) {
            stats_.entries_occupied.fetch_add(1, std::memory_order_relaxed);
            write_slot(deep_slot, key, score, best_move, depth, flag, age);
            return;
        }
        if (empty1) {
            stats_.entries_occupied.fetch_add(1, std::memory_order_relaxed);
            write_slot(recent_slot, key, score, best_move, depth, flag, age);
            return;
        }

        // Case 4: Neither matches and both full -> replacement evaluation
        uint8_t deep_depth = static_cast<uint8_t>((d0 >> 40) & 0xFFULL);
        uint8_t deep_age = static_cast<uint8_t>((d0 >> 56) & 0xFFULL);
        const int age_diff = static_cast<int>(age) - static_cast<int>(deep_age);

        if (depth > deep_depth || age_diff > 0) {
            // New entry is deeper or from younger generation than current deep slot:
            // Move current deep entry into recent slot, and store new in deep slot
            recent_slot.key_xor_data.store(k0, std::memory_order_relaxed);
            recent_slot.data.store(d0, std::memory_order_relaxed);
            write_slot(deep_slot, key, score, best_move, depth, flag, age);
            stats_.overwrites.fetch_add(1, std::memory_order_relaxed);
        } else {
            // Otherwise, overwrite recent slot (Always-Replace behavior for secondary slot)
            write_slot(recent_slot, key, score, best_move, depth, flag, age);
            stats_.overwrites.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::vector<TTAtomicSlot> table_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    ReplacementPolicy policy_ = ReplacementPolicy::DepthPreferred;
    std::atomic<uint8_t> current_age_{0};
    Stats stats_{};
};

} // namespace mmb
