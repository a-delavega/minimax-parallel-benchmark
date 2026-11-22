#pragma once

#include "minimax/board.hpp"
#include "minimax/move_orderer.hpp"
#include "minimax/types.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace mmb {

/**
 * @brief Clock management mode for move budgeting.
 */
enum class TimeMode {
    FixedPerMove,       // Constant budget per move (e.g. 50ms, 100ms)
    DynamicGameClock    // Total match time pool divided dynamically by moves left and board complexity
};

/**
 * @brief User configuration settings for TimeManager.
 */
struct TimeSettings {
    TimeMode mode = TimeMode::FixedPerMove;
    std::chrono::milliseconds fixed_time_per_move{100};    // Time allotted per move in FixedPerMove mode
    std::chrono::milliseconds game_clock_remaining{30'000}; // Remaining match pool in DynamicGameClock mode
    std::chrono::milliseconds time_increment{0};           // Fischer increment added per move
    std::chrono::milliseconds safety_buffer{5};            // Margin reserved for OS scheduling jitter
    double default_ebf = 3.5;                              // Estimated effective branching factor
    double min_complexity_factor = 0.2;                    // Minimum multiplier for trivial/forced moves
    double max_complexity_factor = 2.5;                    // Maximum multiplier for sharp/volatile positions
};

/**
 * @brief Time limits computed for an active move search.
 */
struct TimeBudget {
    std::chrono::milliseconds soft_deadline{0}; // Threshold to decide whether to begin the next depth d+1
    std::chrono::milliseconds hard_deadline{0}; // Absolute deadline when search must cooperatively abort
};

/**
 * @brief Dynamic real-time search budget controller.
 * 
 * Provides:
 * 1. Fixed and dynamic game-clock budgeting.
 * 2. Tactical position complexity assessment (fast exit for forced moves vs deep thinking for sharp splits).
 * 3. Predictive Iterative Deepening depth continuation: determines whether d+1 can finish within budget.
 * 4. Microsecond-precision elapsed and remaining timers with safety buffer guarantees.
 */
class TimeManager {
public:
    explicit TimeManager(const TimeSettings& settings = TimeSettings{})
        : settings_(settings),
          game_clock_(settings.game_clock_remaining) {}

    TimeManager(const TimeManager& other)
        : settings_(other.settings_),
          game_clock_(other.game_clock_) {}

    TimeManager& operator=(const TimeManager& other) {
        if (this != &other) {
            stop_async_timer();
            settings_ = other.settings_;
            game_clock_ = other.game_clock_;
        }
        return *this;
    }

    ~TimeManager() {
        stop_async_timer();
    }

    void set_settings(const TimeSettings& settings) noexcept {
        settings_ = settings;
        game_clock_ = settings.game_clock_remaining;
    }

    [[nodiscard]] const TimeSettings& settings() const noexcept {
        return settings_;
    }

    [[nodiscard]] std::chrono::milliseconds game_clock() const noexcept {
        return game_clock_;
    }

    void set_game_clock(std::chrono::milliseconds clock) noexcept {
        game_clock_ = clock;
    }

    [[nodiscard]] std::atomic<bool>& stop_flag() noexcept {
        return stop_flag_;
    }

    [[nodiscard]] const std::atomic<bool>& stop_flag() const noexcept {
        return stop_flag_;
    }

    void trigger_stop() noexcept {
        stop_flag_.store(true, std::memory_order_relaxed);
    }

    void start_async_timer(std::chrono::milliseconds timeout) noexcept {
        stop_async_timer();
        stop_flag_.store(false, std::memory_order_relaxed);
        if (timeout <= std::chrono::milliseconds{0}) {
            stop_flag_.store(true, std::memory_order_relaxed);
            return;
        }

        timer_thread_ = std::jthread([this, timeout](std::stop_token st) {
            std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);
            std::condition_variable_any cv;
            if (!cv.wait_for(lock, st, timeout, [&] { return st.stop_requested(); })) {
                stop_flag_.store(true, std::memory_order_relaxed);
            }
        });
    }

    void stop_async_timer() noexcept {
        if (timer_thread_.joinable()) {
            timer_thread_.request_stop();
            timer_thread_.join();
        }
    }

    /**
     * @brief Computes tactical complexity factor in [min_factor, max_factor] based on board state.
     */
    [[nodiscard]] double evaluate_tactical_complexity(const Board& board) const noexcept {
        // Count legal columns
        std::array<Column, COLS> moves{};
        const int legal_moves = MoveOrderer::get_ordered_moves(board, moves);

        if (legal_moves <= 1) {
            return settings_.min_complexity_factor; // Only 0 or 1 legal moves available: instant response
        }

        const Player current = board.current_player;
        const Player opp = opponent(current);

        // Check for immediate winning move
        for (int i = 0; i < legal_moves; ++i) {
            const Column col = moves[static_cast<std::size_t>(i)];
            if (board.is_winning_move(col)) {
                return settings_.min_complexity_factor; // Immediate forced win: don't burn clock
            }
        }

        // Check for urgent defensive block
        int blocks_needed = 0;
        for (int i = 0; i < legal_moves; ++i) {
            const Column col = moves[static_cast<std::size_t>(i)];
            if (board.is_winning_move_for(opp, col)) {
                blocks_needed++;
            }
        }
        if (blocks_needed > 0) {
            // Forced defense: play the block quickly
            return 0.4;
        }

        // Center contest complexity: moves in columns 2, 3, 4 signify sharp opening/midgame struggles
        const int moves_played = static_cast<int>(board.moves_played);
        if (moves_played >= 4 && moves_played <= 20) {
            return 1.4; // Midgame tactical peak
        }

        return 1.0; // Standard default complexity
    }

    /**
     * @brief Computes soft and hard deadlines for a turn given the board position.
     */
    [[nodiscard]] TimeBudget compute_budget(const Board& board) const noexcept {
        TimeBudget budget;

        if (settings_.mode == TimeMode::FixedPerMove) {
            const auto fixed = settings_.fixed_time_per_move;
            budget.hard_deadline = (fixed > settings_.safety_buffer) 
                ? (fixed - settings_.safety_buffer) 
                : fixed;
            // Soft deadline allows starting depth d+1 if at least 50% of the budget remains
            budget.soft_deadline = budget.hard_deadline / 2;
            return budget;
        }

        // DynamicGameClock mode
        const int moves_played = static_cast<int>(board.moves_played);
        // Connect 4 has 42 cells total; typical games conclude within 25-35 moves
        const int estimated_moves_left = std::max(1, 40 - moves_played);

        // Base time slice: (game_clock / moves_left) + increment
        const double base_ms = (static_cast<double>(game_clock_.count()) / estimated_moves_left)
                             + settings_.time_increment.count();

        const double complexity = evaluate_tactical_complexity(board);
        const double scaled_ms = std::clamp(
            base_ms * complexity,
            static_cast<double>(settings_.safety_buffer.count()),
            static_cast<double>(game_clock_.count())
        );

        const auto allocated = std::chrono::milliseconds(static_cast<int64_t>(scaled_ms));
        budget.hard_deadline = (allocated > settings_.safety_buffer)
            ? (allocated - settings_.safety_buffer)
            : allocated;
        budget.soft_deadline = budget.hard_deadline / 2;

        return budget;
    }

    /**
     * @brief Begins tracking elapsed time for the current turn.
     */
    void start_turn(const Board& board) noexcept {
        current_budget_ = compute_budget(board);
        start_time_ = std::chrono::high_resolution_clock::now();
        start_async_timer(current_budget_.hard_deadline);
    }

    [[nodiscard]] const TimeBudget& current_budget() const noexcept {
        return current_budget_;
    }

    /**
     * @brief Returns total wall-clock duration elapsed since start_turn.
     */
    [[nodiscard]] std::chrono::microseconds elapsed() const noexcept {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_);
    }

    /**
     * @brief Returns remaining time until hard deadline.
     */
    [[nodiscard]] std::chrono::microseconds remaining() const noexcept {
        const auto el = elapsed();
        const auto hard_us = std::chrono::duration_cast<std::chrono::microseconds>(current_budget_.hard_deadline);
        return (hard_us > el) ? (hard_us - el) : std::chrono::microseconds{0};
    }

    /**
     * @brief Checks if hard deadline has been reached.
     */
    [[nodiscard]] bool is_time_up() const noexcept {
        return stop_flag_.load(std::memory_order_relaxed) || 
               (elapsed() >= std::chrono::duration_cast<std::chrono::microseconds>(current_budget_.hard_deadline));
    }

    /**
     * @brief Predicts if next depth d+1 should be initiated.
     * @param current_depth Depth d that just finished.
     * @param last_depth_duration Elapsed time taken exclusively by depth d.
     * @return true if depth d+1 is projected to finish before soft deadline, false to abort.
     */
    [[nodiscard]] bool should_start_next_depth(
        int current_depth, 
        std::chrono::microseconds last_depth_duration
    ) const noexcept {
        if (stop_flag_.load(std::memory_order_relaxed)) {
            return false;
        }

        if (current_depth <= 1) {
            return true; // Always explore at least depth 2
        }

        const auto el = elapsed();
        const auto soft_us = std::chrono::duration_cast<std::chrono::microseconds>(current_budget_.soft_deadline);
        const auto hard_us = std::chrono::duration_cast<std::chrono::microseconds>(current_budget_.hard_deadline);

        // If we have already surpassed the soft deadline, stop immediately
        if (el >= soft_us) {
            return false;
        }

        // Predict duration of depth d+1 using effective branching factor (EBF)
        const double predicted_next_us = static_cast<double>(last_depth_duration.count()) * settings_.default_ebf;
        const auto predicted_total_us = el + std::chrono::microseconds(static_cast<int64_t>(predicted_next_us));

        // Only start if projected total duration fits within hard deadline
        return predicted_total_us <= hard_us;
    }

    /**
     * @brief Updates match clock upon turn completion by deducting turn elapsed time and adding increment.
     */
    void complete_turn(std::chrono::microseconds turn_duration) noexcept {
        stop_async_timer();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(turn_duration);
        if (game_clock_ > duration_ms) {
            game_clock_ -= duration_ms;
        } else {
            game_clock_ = std::chrono::milliseconds{0};
        }
        game_clock_ += settings_.time_increment;
    }

private:
    TimeSettings settings_{};
    std::chrono::milliseconds game_clock_{30'000};
    TimeBudget current_budget_{};
    std::chrono::high_resolution_clock::time_point start_time_{};
    std::atomic<bool> stop_flag_{false};
    std::jthread timer_thread_{};
};

} // namespace mmb
