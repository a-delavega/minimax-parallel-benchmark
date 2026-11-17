#pragma once

#include "minimax/types.hpp"
#include <array>
#include <cstdint>
#include <random>

namespace mmb {

/**
 * @brief Zobrist hashing keys for Connect 4 (64-bit pseudo-random numbers).
 * 
 * Generates and stores uniformly distributed 64-bit integers for each player's
 * piece on every valid board cell (2 players x 7 columns x 6 rows = 84 keys),
 * plus an additional key for the active player's turn.
 * 
 * Uses std::mt19937_64 seeded with a fixed constant for deterministic behavior
 * across all compilers, platforms, and runs.
 */
class ZobristKeys {
public:
    static constexpr uint64_t DEFAULT_SEED = 0x9876543210FEDCBAULL;

    /**
     * @brief Constructs and initializes Zobrist keys using a fixed or custom seed.
     * @param seed PRNG seed (defaults to DEFAULT_SEED for reproducibility).
     */
    explicit ZobristKeys(uint64_t seed = DEFAULT_SEED) noexcept {
        init(seed);
    }

    /**
     * @brief Reinitializes the table with a new seed.
     */
    void init(uint64_t seed) noexcept {
        std::mt19937_64 rng(seed);
        for (size_t p = 0; p < 2; ++p) {
            for (size_t col = 0; col < static_cast<size_t>(COLS); ++col) {
                for (size_t row = 0; row < static_cast<size_t>(ROWS); ++row) {
                    pieces_[p][col][row] = rng();
                }
            }
        }
        turn_ = rng();
    }

    /**
     * @brief Returns the 64-bit Zobrist key for a specific piece placement.
     * @param player Player (Player1 or Player2).
     * @param col Column index [0, 6].
     * @param row Row index [0, 5].
     */
    [[nodiscard]] constexpr uint64_t piece_key(Player player, Column col, int row) const noexcept {
        const auto p = static_cast<size_t>(player);
        if (p >= 2 || col < 0 || col >= COLS || row < 0 || row >= ROWS) {
            return 0ULL;
        }
        return pieces_[p][static_cast<size_t>(col)][static_cast<size_t>(row)];
    }

    /**
     * @brief Returns the 64-bit Zobrist key toggled when Player 2 has the turn.
     */
    [[nodiscard]] constexpr uint64_t turn_key() const noexcept {
        return turn_;
    }

    /**
     * @brief Total number of piece keys in the table (84).
     */
    static constexpr size_t NUM_PIECE_KEYS = 2 * 7 * 6;

private:
    std::array<std::array<std::array<uint64_t, ROWS>, COLS>, 2> pieces_{};
    uint64_t turn_{0ULL};
};

/**
 * @brief Global default Zobrist keys instance initialized with DEFAULT_SEED.
 * Zero-cost inline reference shared across all engine instances and threads.
 */
inline const ZobristKeys default_zobrist_keys{};

} // namespace mmb
