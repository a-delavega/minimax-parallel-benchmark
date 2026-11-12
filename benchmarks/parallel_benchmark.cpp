// ============================================================================
// Parallel Scalability Benchmark Suite (Subphase 2.6)
// Strong and Weak Scalability evaluation for LazySmpEngine
// ============================================================================

#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace mmb;

struct BenchmarkResult {
    std::size_t thread_count{1};
    int depth{8};
    double duration_ms{0.0};
    uint64_t total_nodes{0};
    double nps{0.0};
    double speedup{1.0};
    double efficiency{1.0};
    Column best_move{-1};
    Score score{0};
};

void print_separator(char c = '=', int width = 100) {
    std::cout << std::string(width, c) << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    print_separator('=');
    std::cout << "  MINIMAX-PARALLEL-BENCHMARK: PARALLEL SCALABILITY SUITE (SUBPHASE 2.6)\n";
    std::cout << "  Lazy SMP Strong & Weak Scaling Analysis (C++20 Zero-Mutex Lock-Free TT)\n";
    print_separator('=');

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    std::cout << "Hardware Concurrency detected: " << hardware_threads << " logical cores\n\n";

    // Setup benchmark thread configurations
    std::vector<std::size_t> thread_configs;
    for (std::size_t t = 1; t <= hardware_threads; t = (t == 1 ? 2 : (t * 2 > hardware_threads && t < hardware_threads ? hardware_threads : t * 2))) {
        thread_configs.push_back(t);
        if (t == hardware_threads) break;
    }
    if (thread_configs.back() != hardware_threads) {
        thread_configs.push_back(hardware_threads);
    }

    auto eval = std::make_shared<SimpleEvaluator>();

    // Positions for benchmarking
    // Position 1: Tactical midgame position (6 plies)
    Board tactical_board;
    tactical_board.make_move(3); // P1
    tactical_board.make_move(3); // P2
    tactical_board.make_move(2); // P1
    tactical_board.make_move(4); // P2
    tactical_board.make_move(2); // P1
    tactical_board.make_move(1); // P2

    // Position 2: Opening Board (Empty 7x6)
    Board empty_board;

    std::vector<std::pair<std::string, Board>> benchmark_positions = {
        {"Midgame Tactical (6 plies played)", tactical_board},
        {"Opening Board (Empty 7x6)", empty_board}
    };

    const int strong_depth = 8;

    for (const auto& [pos_name, board] : benchmark_positions) {
        print_separator('-');
        std::cout << "Strong Scaling Benchmark: " << pos_name << " (Fixed Depth: " << strong_depth << ")\n";
        print_separator('-');
        std::cout << std::setw(8)  << "Threads"
                  << std::setw(12) << "Time (ms)"
                  << std::setw(12) << "Speedup"
                  << std::setw(14) << "Efficiency"
                  << std::setw(14) << "Total Nodes"
                  << std::setw(16) << "Nodes / Sec"
                  << std::setw(10) << "BestMove"
                  << std::setw(8)  << "Score" << "\n";
        print_separator('-');

        double baseline_time_ms = 1.0;
        std::vector<BenchmarkResult> results;

        for (std::size_t threads : thread_configs) {
            LazySmpEngine engine(eval, threads);

            const auto start = std::chrono::steady_clock::now();
            const SearchResult res = engine.search(board, strong_depth);
            const auto end = std::chrono::steady_clock::now();

            const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

            if (threads == 1) {
                baseline_time_ms = duration_ms;
            }

            BenchmarkResult r;
            r.thread_count = threads;
            r.depth = strong_depth;
            r.duration_ms = duration_ms;
            r.total_nodes = res.nodes_visited;
            r.nps = res.nodes_per_second();
            r.speedup = (duration_ms > 0.0) ? (baseline_time_ms / duration_ms) : 1.0;
            r.efficiency = r.speedup / static_cast<double>(threads);
            r.best_move = res.best_move;
            r.score = res.score;

            results.push_back(r);

            std::cout << std::setw(8)  << r.thread_count
                      << std::setw(12) << std::fixed << std::setprecision(2) << r.duration_ms
                      << std::setw(11) << std::fixed << std::setprecision(2) << r.speedup << "x"
                      << std::setw(13) << std::fixed << std::setprecision(1) << (r.efficiency * 100.0) << "%"
                      << std::setw(14) << r.total_nodes
                      << std::setw(16) << std::fixed << std::setprecision(0) << r.nps
                      << std::setw(10) << (r.best_move + 1)
                      << std::setw(8)  << r.score << "\n";
        }
        std::cout << "\n";
    }

    // Weak Scaling Benchmark: Problem size scales with thread count
    print_separator('-');
    std::cout << "Weak Scaling Analysis (Workload scales with available cores)\n";
    print_separator('-');
    std::cout << std::setw(8)  << "Threads"
              << std::setw(8)  << "Depth"
              << std::setw(14) << "Time (ms)"
              << std::setw(16) << "Total Nodes"
              << std::setw(18) << "Nodes / Sec"
              << std::setw(10) << "BestMove" << "\n";
    print_separator('-');

    for (std::size_t threads : thread_configs) {
        const int depth = 7 + (threads >= 4 ? 1 : 0);
        LazySmpEngine engine(eval, threads);

        const auto start = std::chrono::steady_clock::now();
        const SearchResult res = engine.search(tactical_board, depth);
        const auto end = std::chrono::steady_clock::now();

        const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << std::setw(8)  << threads
                  << std::setw(8)  << depth
                  << std::setw(14) << std::fixed << std::setprecision(2) << duration_ms
                  << std::setw(16) << res.nodes_visited
                  << std::setw(18) << std::fixed << std::setprecision(0) << res.nodes_per_second()
                  << std::setw(10) << (res.best_move + 1) << "\n";
    }

    print_separator('=');
    std::cout << "Parallel Scalability Suite completed successfully.\n";
    print_separator('=');
    return 0;
}
