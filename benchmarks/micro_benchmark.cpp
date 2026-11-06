#include "minimax/board.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/move_orderer.hpp"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma optimize("", off)
void escape(void* p) {
    (void)p;
}
void clobber() {}
#pragma optimize("", on)
#else
inline void escape(void* p) {
    asm volatile("" : : "g"(p) : "memory");
}
inline void clobber() {
    asm volatile("" : : : "memory");
}
#endif

namespace {

struct BenchmarkResult {
    std::string name;
    uint64_t iterations;
    double duration_ms;
    double ns_per_op;
    double mops; // Million Operations Per Second
};

void print_table(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n"
              << std::left << std::setw(42) << "Microbenchmark Target"
              << std::right << std::setw(14) << "Iterations"
              << std::setw(14) << "Time (ms)"
              << std::setw(16) << "Latency (ns)"
              << std::setw(18) << "Throughput"
              << "\n";
    std::cout << std::string(104, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(42) << r.name
                  << std::right << std::setw(14) << r.iterations
                  << std::fixed << std::setprecision(2)
                  << std::setw(14) << r.duration_ms
                  << std::setprecision(2)
                  << std::setw(16) << r.ns_per_op
                  << std::setprecision(2)
                  << std::setw(11) << r.mops << " MOps/s"
                  << "\n";
    }
    std::cout << std::string(104, '-') << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    bool quick_mode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quick" || arg == "-q" || arg == "--smoke") {
            quick_mode = true;
        }
    }

    const uint64_t BASE_ITERS = quick_mode ? 200'000ULL : 10'000'000ULL;

    std::cout << "============================================================\n"
              << "       minimax-parallel-benchmark: CPU Microbenchmark       \n"
              << "============================================================\n"
              << "Mode: " << (quick_mode ? "Quick / CI Smoke" : "High-Precision Comprehensive") << "\n"
              << "Base iterations per primitive: " << BASE_ITERS << "\n";

    std::vector<BenchmarkResult> results;
    results.reserve(8);

    using Clock = std::chrono::high_resolution_clock;

    // Create diverse board positions for benchmarks
    mmb::Board empty_board;

    mmb::Board tactical_board;
    tactical_board.make_move(3); // P1
    tactical_board.make_move(0); // P2
    tactical_board.make_move(3); // P1
    tactical_board.make_move(0); // P2
    tactical_board.make_move(3); // P1 (three in col 3)
    tactical_board.make_move(0); // P2

    mmb::Board midgame_board;
    const std::vector<mmb::Column> midgame_moves = {3, 3, 2, 4, 3, 2, 4, 4};
    for (mmb::Column col : midgame_moves) {
        midgame_board.make_move(col);
    }

    // ------------------------------------------------------------
    // 1. Board::has_won (No Win / Negative Branchless Path)
    // ------------------------------------------------------------
    {
        uint64_t dummy_board = 0b0000011'0000010'0000100'0001000'0000000ULL;
        uint32_t sink = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < BASE_ITERS; ++i) {
            sink += mmb::Board::has_won(dummy_board + (i & 0x7));
            escape(&sink);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(BASE_ITERS);
        double mops = (static_cast<double>(BASE_ITERS) / (ms * 1'000.0));
        results.push_back({"Board::has_won (Negative/No Win)", BASE_ITERS, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 2. Board::has_won (Horizontal 4-in-a-row Win)
    // ------------------------------------------------------------
    {
        uint64_t winning_board = (1ULL << 7) | (1ULL << 14) | (1ULL << 21) | (1ULL << 28);
        uint32_t sink = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < BASE_ITERS; ++i) {
            sink += mmb::Board::has_won(winning_board);
            escape(&sink);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(BASE_ITERS);
        double mops = (static_cast<double>(BASE_ITERS) / (ms * 1'000.0));
        results.push_back({"Board::has_won (Horizontal Win)", BASE_ITERS, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 3. Board::make_move & unmake_move (Roundtrip Stack Mutation)
    // ------------------------------------------------------------
    {
        mmb::Board board;
        const uint64_t iters = BASE_ITERS;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < iters; ++i) {
            const mmb::Column col = static_cast<mmb::Column>(i % 7);
            board.make_move(col);
            escape(&board);
            board.unmake_move(col);
            escape(&board);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(iters * 2);
        double mops = (static_cast<double>(iters * 2) / (ms * 1'000.0));
        results.push_back({"Board::make + unmake_move", iters * 2, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 4. Board::is_winning_move (Tactical Lookahead 1-ply)
    // ------------------------------------------------------------
    {
        uint32_t sink = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < BASE_ITERS; ++i) {
            const mmb::Column col = static_cast<mmb::Column>(i % 7);
            sink += tactical_board.is_winning_move(col);
            escape(&sink);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(BASE_ITERS);
        double mops = (static_cast<double>(BASE_ITERS) / (ms * 1'000.0));
        results.push_back({"Board::is_winning_move (Tactical 1-ply)", BASE_ITERS, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 5. MoveOrderer::get_ordered_moves
    // ------------------------------------------------------------
    {
        std::array<mmb::Column, mmb::COLS> moves{};
        int total_moves = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < BASE_ITERS; ++i) {
            total_moves += mmb::MoveOrderer::get_ordered_moves(tactical_board, moves);
            escape(moves.data());
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(BASE_ITERS);
        double mops = (static_cast<double>(BASE_ITERS) / (ms * 1'000.0));
        results.push_back({"MoveOrderer::get_ordered_moves", BASE_ITERS, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 6. SimpleEvaluator::evaluate (O(1) Terminal Leaf Check)
    // ------------------------------------------------------------
    {
        mmb::SimpleEvaluator simple_eval;
        mmb::Score sink = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < BASE_ITERS; ++i) {
            sink += simple_eval.evaluate(midgame_board, mmb::Player::Player1, static_cast<int>(i & 7));
            escape(&sink);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(BASE_ITERS);
        double mops = (static_cast<double>(BASE_ITERS) / (ms * 1'000.0));
        results.push_back({"SimpleEvaluator::evaluate", BASE_ITERS, ms, ns, mops});
    }

    // ------------------------------------------------------------
    // 7. ComplexEvaluator::evaluate (Pattern & Threat Evaluation)
    // ------------------------------------------------------------
    {
        mmb::ComplexEvaluator complex_eval;
        const uint64_t complex_iters = quick_mode ? 50'000ULL : 2'000'000ULL;
        mmb::Score sink = 0;
        auto t0 = Clock::now();
        for (uint64_t i = 0; i < complex_iters; ++i) {
            sink += complex_eval.evaluate(midgame_board, mmb::Player::Player1, static_cast<int>(i & 7));
            escape(&sink);
        }
        auto t1 = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ns = (ms * 1'000'000.0) / static_cast<double>(complex_iters);
        double mops = (static_cast<double>(complex_iters) / (ms * 1'000.0));
        results.push_back({"ComplexEvaluator::evaluate", complex_iters, ms, ns, mops});
    }

    print_table(results);
    std::cout << "[SUCCESS] CPU Microbenchmark completed successfully.\n" << std::endl;
    return 0;
}
