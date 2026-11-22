// ============================================================================
// Real-Time Blitz Search & Cooperative Stop Benchmark Suite (Subphase 5.5)
// Measures:
// 1. Strict time-control deadline adherence across sub-second budgets (10ms to 500ms).
// 2. Depth reached and NPS throughput under time pressure.
// 3. Sequential Alpha-Beta vs Lazy SMP parallel scaling in real-time matches.
// 4. Exports telemetry to blitz_results.csv for automated performance visualization.
// ============================================================================

#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/time/time_manager.hpp"
#include "minimax/transposition/transposition_table.hpp"

#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace mmb;

namespace {

void print_separator(char c = '=', int width = 110) {
    std::cout << std::string(width, c) << "\n";
}

Board create_midgame_position() {
    Board board;
    board.make_move(3);
    board.make_move(3);
    return board;
}

struct BlitzRunResult {
    std::string engine_name;
    std::size_t threads;
    int budget_ms;
    double elapsed_ms;
    double deviation_ms;
    int depth_reached;
    uint64_t nodes;
    double nps;
    Column best_move;
    Score score;
};

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    print_separator('=');
    std::cout << "  MINIMAX-PARALLEL-BENCHMARK: REAL-TIME BLITZ SEARCH SUITE (SUBPHASE 5.5)\n";
    std::cout << "  Deadline Adherence, Latency Variance, Dynamic Depth & Multi-Threaded Blitz\n";
    print_separator('=');
    std::cout << "\n";

    auto eval = std::make_shared<ComplexEvaluator>();
    const Board board = create_midgame_position();

    const std::vector<int> budgets = {10, 25, 50, 100, 250, 500};
    const unsigned int hw_threads = std::thread::hardware_concurrency();
    const std::size_t smp_high_threads = (hw_threads >= 8) ? 8 : (hw_threads >= 4 ? 4 : 2);
    const std::size_t smp_low_threads = 2;

    std::vector<BlitzRunResult> results;
    results.reserve(budgets.size() * 3);

    std::ofstream csv("blitz_results.csv");
    csv << "Engine,Threads,Budget_ms,Elapsed_ms,Deviation_ms,DepthReached,NodesVisited,NPS,BestMove,Score\n";

    std::cout << std::left << std::setw(22) << "Engine"
              << std::right << std::setw(9) << "Threads"
              << std::setw(12) << "Budget(ms)"
              << std::setw(13) << "Elapsed(ms)"
              << std::setw(12) << "Delta(ms)"
              << std::setw(8)  << "Depth"
              << std::setw(14) << "Nodes"
              << std::setw(13) << "NPS"
              << std::setw(7)  << "Move"
              << "\n";
    print_separator('-');

    for (int budget_ms : budgets) {
        TimeSettings settings;
        settings.mode = TimeMode::FixedPerMove;
        settings.fixed_time_per_move = std::chrono::milliseconds(budget_ms);
        settings.safety_buffer = std::chrono::milliseconds(2);

        // 1. AlphaBeta Sequential Engine with 4MB TT
        {
            auto tt = std::make_shared<TranspositionTable>(4);
            AlphaBetaEngine engine(eval, tt);
            TimeManager tm(settings);

            const auto t0 = std::chrono::high_resolution_clock::now();
            SearchResult res = engine.search_timed(board, tm, 42);
            const auto t1 = std::chrono::high_resolution_clock::now();

            const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double deviation = elapsed_ms - budget_ms;
            const double nps = (elapsed_ms > 0.0) ? (static_cast<double>(res.nodes_visited) / (elapsed_ms / 1000.0)) : 0.0;

            BlitzRunResult run{"AlphaBeta", 1, budget_ms, elapsed_ms, deviation, res.depth_reached, res.nodes_visited, nps, res.best_move, res.score};
            results.push_back(run);

            csv << run.engine_name << ","
                << run.threads << ","
                << run.budget_ms << ","
                << std::fixed << std::setprecision(2) << run.elapsed_ms << ","
                << run.deviation_ms << ","
                << run.depth_reached << ","
                << run.nodes << ","
                << std::setprecision(0) << run.nps << ","
                << run.best_move << ","
                << run.score << "\n";

            std::cout << std::left << std::setw(22) << run.engine_name
                      << std::right << std::setw(9) << run.threads
                      << std::setw(12) << run.budget_ms
                      << std::fixed << std::setprecision(2)
                      << std::setw(13) << run.elapsed_ms
                      << std::setw(12) << run.deviation_ms
                      << std::setw(8)  << run.depth_reached
                      << std::setw(14) << run.nodes
                      << std::setprecision(0)
                      << std::setw(13) << run.nps
                      << std::setw(7)  << run.best_move
                      << "\n";
        }

        // 2. Lazy SMP (Low Threads) with Shared 4MB TT
        {
            auto tt = std::make_shared<TranspositionTable>(4);
            LazySmpEngine engine(eval, smp_low_threads, tt);
            TimeManager tm(settings);

            const auto t0 = std::chrono::high_resolution_clock::now();
            SearchResult res = engine.search_timed(board, tm, 42);
            const auto t1 = std::chrono::high_resolution_clock::now();

            const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double deviation = elapsed_ms - budget_ms;
            const double nps = (elapsed_ms > 0.0) ? (static_cast<double>(res.nodes_visited) / (elapsed_ms / 1000.0)) : 0.0;

            BlitzRunResult run{"LazySMP", smp_low_threads, budget_ms, elapsed_ms, deviation, res.depth_reached, res.nodes_visited, nps, res.best_move, res.score};
            results.push_back(run);

            csv << run.engine_name << ","
                << run.threads << ","
                << run.budget_ms << ","
                << std::fixed << std::setprecision(2) << run.elapsed_ms << ","
                << run.deviation_ms << ","
                << run.depth_reached << ","
                << run.nodes << ","
                << std::setprecision(0) << run.nps << ","
                << run.best_move << ","
                << run.score << "\n";

            std::cout << std::left << std::setw(22) << run.engine_name
                      << std::right << std::setw(9) << run.threads
                      << std::setw(12) << run.budget_ms
                      << std::fixed << std::setprecision(2)
                      << std::setw(13) << run.elapsed_ms
                      << std::setw(12) << run.deviation_ms
                      << std::setw(8)  << run.depth_reached
                      << std::setw(14) << run.nodes
                      << std::setprecision(0)
                      << std::setw(13) << run.nps
                      << std::setw(7)  << run.best_move
                      << "\n";
        }

        // 3. Lazy SMP (High Threads) with Shared 4MB TT
        {
            auto tt = std::make_shared<TranspositionTable>(4);
            LazySmpEngine engine(eval, smp_high_threads, tt);
            TimeManager tm(settings);

            const auto t0 = std::chrono::high_resolution_clock::now();
            SearchResult res = engine.search_timed(board, tm, 42);
            const auto t1 = std::chrono::high_resolution_clock::now();

            const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            const double deviation = elapsed_ms - budget_ms;
            const double nps = (elapsed_ms > 0.0) ? (static_cast<double>(res.nodes_visited) / (elapsed_ms / 1000.0)) : 0.0;

            BlitzRunResult run{"LazySMP", smp_high_threads, budget_ms, elapsed_ms, deviation, res.depth_reached, res.nodes_visited, nps, res.best_move, res.score};
            results.push_back(run);

            csv << run.engine_name << ","
                << run.threads << ","
                << run.budget_ms << ","
                << std::fixed << std::setprecision(2) << run.elapsed_ms << ","
                << run.deviation_ms << ","
                << run.depth_reached << ","
                << run.nodes << ","
                << std::setprecision(0) << run.nps << ","
                << run.best_move << ","
                << run.score << "\n";

            std::cout << std::left << std::setw(22) << run.engine_name
                      << std::right << std::setw(9) << run.threads
                      << std::setw(12) << run.budget_ms
                      << std::fixed << std::setprecision(2)
                      << std::setw(13) << run.elapsed_ms
                      << std::setw(12) << run.deviation_ms
                      << std::setw(8)  << run.depth_reached
                      << std::setw(14) << run.nodes
                      << std::setprecision(0)
                      << std::setw(13) << run.nps
                      << std::setw(7)  << run.best_move
                      << "\n";
        }
        print_separator('-');
    }

    // Summary statistics
    double total_abs_dev = 0.0;
    for (const auto& r : results) {
        total_abs_dev += std::abs(r.deviation_ms);
    }
    const double mean_abs_dev = results.empty() ? 0.0 : (total_abs_dev / results.size());

    std::cout << "\n[OK] Blitz benchmark complete across " << results.size() << " test configurations.\n";
    std::cout << "     Mean Absolute Deadline Deviation: " << std::fixed << std::setprecision(2) 
              << mean_abs_dev << " ms\n";
    std::cout << "     Detailed telemetry written to: blitz_results.csv\n\n";

    return 0;
}
