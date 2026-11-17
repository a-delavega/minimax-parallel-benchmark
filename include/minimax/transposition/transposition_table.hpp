#pragma once

#include "minimax/types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <bit>
#include <algorithm>

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
 * @brief High-performance Transposition Table with power-of-two sizing.
 * 
 * Provides O(1) lookup and storage using bitwise masking (index = key & mask)
 * with zero hardware division overhead.
 */
class TranspositionTable {
public:
    static constexpr size_t DEFAULT_SIZE_MB = 16;
    static constexpr uint8_t NO_MOVE = 255;

    struct Stats {
        size_t capacity = 0;
        size_t entries_occupied = 0;
        uint64_t probes = 0;
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t collisions = 0;
        uint64_t stores = 0;
        uint64_t overwrites = 0;

        [[nodiscard]] double hit_rate() const noexcept {
            return (probes == 0) ? 0.0 : static_cast<double>(hits) / static_cast<double>(probes);
        }

        [[nodiscard]] double occupancy() const noexcept {
            return (capacity == 0) ? 0.0 : static_cast<double>(entries_occupied) / static_cast<double>(capacity);
        }
    };

    /**
     * @brief Constructs a Transposition Table with the given size in Megabytes.
     * @param size_mb Memory size in megabytes (default: 16 MB).
     */
    explicit TranspositionTable(size_t size_mb = DEFAULT_SIZE_MB) {
        resize_mb(size_mb);
    }

    /**
     * @brief Resizes table to the nearest power of 2 entries fitting in size_mb.
     */
    void resize_mb(size_t size_mb) {
        if (size_mb == 0) size_mb = 1;
        const size_t total_bytes = size_mb * 1024 * 1024;
        const size_t raw_entries = total_bytes / sizeof(TTEntry);
        resize_entries(raw_entries);
    }

    /**
     * @brief Resizes table to the nearest power of 2 greater than or equal to count.
     */
    void resize_entries(size_t count) {
        if (count < 2) count = 2;
        // Round down or up to power of 2
        size_t power_of_two = 1;
        while (power_of_two * 2 <= count) {
            power_of_two *= 2;
        }
        capacity_ = power_of_two;
        mask_ = capacity_ - 1;
        table_.assign(capacity_, TTEntry{});
        clear_stats();
    }

    /**
     * @brief Probes table for an entry matching the given 64-bit Zobrist key in O(1).
     * @param key 64-bit Zobrist key of the board state.
     * @param out_entry Output parameter receiving the found entry.
     * @return true if an entry with matching key was found, false otherwise.
     */
    [[nodiscard]] bool probe(uint64_t key, TTEntry& out_entry) noexcept {
        stats_.probes++;
        const size_t index = key & mask_;
        const TTEntry& slot = table_[index];

        if (slot.zobrist_key == key) {
            out_entry = slot;
            stats_.hits++;
            return true;
        }

        stats_.misses++;
        if (!slot.is_empty()) {
            stats_.collisions++;
        }
        return false;
    }

    /**
     * @brief Stores an evaluation entry into the table in O(1).
     * @param key 64-bit Zobrist key.
     * @param score Search evaluation score.
     * @param best_move Best suggested move [0..6] or NO_MOVE (255).
     * @param depth Search depth reached.
     * @param flag TTFlag bound type.
     * @param age Search age / generation counter.
     */
    void store(uint64_t key, Score score, Column best_move, uint8_t depth, TTFlag flag, uint8_t age = 0) noexcept {
        stats_.stores++;
        const size_t index = key & mask_;
        TTEntry& slot = table_[index];

        if (slot.is_empty()) {
            stats_.entries_occupied++;
        } else if (slot.zobrist_key != key) {
            stats_.overwrites++;
        }

        // Store entry data
        slot.zobrist_key = key;
        slot.score = static_cast<int32_t>(score);
        slot.best_move = static_cast<uint8_t>(best_move);
        slot.depth = depth;
        slot.flag = static_cast<uint8_t>(flag);
        slot.age = age;
    }

    /**
     * @brief Clears all entries in the table and resets telemetry counters.
     */
    void clear() noexcept {
        std::fill(table_.begin(), table_.end(), TTEntry{});
        clear_stats();
    }

    /**
     * @brief Resets only the query telemetry counters, preserving stored entries.
     */
    void clear_stats() noexcept {
        stats_.probes = 0;
        stats_.hits = 0;
        stats_.misses = 0;
        stats_.collisions = 0;
        stats_.stores = 0;
        stats_.overwrites = 0;
        stats_.capacity = capacity_;
        // Recalculate occupied entries
        size_t occ = 0;
        for (const auto& entry : table_) {
            if (!entry.is_empty()) occ++;
        }
        stats_.entries_occupied = occ;
    }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t mask() const noexcept { return mask_; }
    [[nodiscard]] size_t memory_bytes() const noexcept { return capacity_ * sizeof(TTEntry); }
    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }
    [[nodiscard]] double hit_rate() const noexcept { return stats_.hit_rate(); }
    [[nodiscard]] double occupancy() const noexcept { return stats_.occupancy(); }

private:
    std::vector<TTEntry> table_;
    size_t capacity_ = 0;
    size_t mask_ = 0;
    Stats stats_{};
};

} // namespace mmb
