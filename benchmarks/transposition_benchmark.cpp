// ============================================================================
// Transposition Table & Zobrist Hashing Benchmark Suite (Subphase 4.7)
// Measures:
// 1. Game-tree reduction & nodes saved % across search horizons (d = 6 to 10).
// 2. Memory scaling & cache hit rate across TT capacities (1MB, 4MB, 16MB, 64MB).
// 3. Parallel Lazy SMP scaling with shared lock-free transposition tables.
// ============================================================================

#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/transposition/transposition_table.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace mmb;

namespace {

void print_separator(char c = '=', int width = 100) {
    std::cout << std::string(width, c) << "\n";
}

Board create_test_position() {
    Board board;
    const std::vector<Column> moves = {3, 3, 2, 4, 3, 2, 4, 4};
    for (Column col : moves) {
        board.make_move(col);
    }
    return board;
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    print_separator('=');
    std::cout << "  MINIMAX-PARALLEL-BENCHMARK: TRANSPOSITION TABLE SUITE (SUBPHASE 4.7)\n";
    std::cout << "  Cache Telemetry, Zobrist Hashing, Tree Pruning & Memory Scaling\n";
    print_separator('=');
    std::cout << "\n";

    auto eval = std::make_shared<SimpleEvaluator>();
    const Board board = create_test_position();

    std::ofstream csv("transposition_results.csv");
    csv << "Category,Configuration,Depth,MemoryMB,NodesNoTT,NodesWithTT,NodesSavedPct,TimeNoTT_ms,TimeWithTT_ms,Speedup,TTHits,TTHitRatePct\n";

    // ------------------------------------------------------------------------
    // Part 1: Game-Tree Reduction Across Search Horizons (d = 6 to 10)
    // ------------------------------------------------------------------------
    std::cout << "--- Part 1: Game-Tree Reduction vs Search Depth (TT = 16 MB) ---\n\n";
    std::cout << std::left
              << std::setw(8)  << "Depth"
              << std::setw(15) << "Nodes (No TT)"
              << std::setw(15) << "Nodes (With TT)"
              << std::setw(14) << "Saved (%)"
              << std::setw(15) << "Time No TT (ms)"
              << std::setw(16) << "Time TT (ms)"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "TT Hit Rate"
              << "\n";
    print_separator('-', 100);

    AlphaBetaEngine engine_no_tt(eval);
    auto tt_16mb = std::make_shared<TranspositionTable>(16);
    AlphaBetaEngine engine_with_tt(eval, tt_16mb);

    for (int depth = 6; depth <= 10; ++depth) {
        tt_16mb->clear();

        const auto res_no_tt = engine_no_tt.search(board, depth);
        const auto res_tt = engine_with_tt.search(board, depth);

        const double saved_pct = (res_no_tt.nodes_visited > 0)
            ? (100.0 * static_cast<double>(res_no_tt.nodes_visited - res_tt.nodes_visited) / static_cast<double>(res_no_tt.nodes_visited))
            : 0.0;

        const double time_no_tt_ms = res_no_tt.duration.count() / 1000.0;
        const double time_tt_ms = res_tt.duration.count() / 1000.0;
        const double speedup = (time_tt_ms > 0.0) ? (time_no_tt_ms / time_tt_ms) : 1.0;
        const double hit_rate_pct = tt_16mb->stats().hit_rate() * 100.0;

        std::string saved_str = std::to_string(static_cast<int>(saved_pct)) + "." +
                                std::to_string(static_cast<int>(saved_pct * 10) % 10) + "%";
        std::string speedup_str = std::to_string(static_cast<int>(speedup)) + "." +
                                  std::to_string(static_cast<int>(speedup * 100) % 100) + "x";
        std::string hit_str = std::to_string(static_cast<int>(hit_rate_pct)) + "." +
                              std::to_string(static_cast<int>(hit_rate_pct * 10) % 10) + "%";

        std::cout << std::left
                  << std::setw(8)  << depth
                  << std::setw(15) << res_no_tt.nodes_visited
                  << std::setw(15) << res_tt.nodes_visited
                  << std::setw(14) << saved_str
                  << std::setw(15) << std::fixed << std::setprecision(2) << time_no_tt_ms
                  << std::setw(16) << std::fixed << std::setprecision(2) << time_tt_ms
                  << std::setw(10) << speedup_str
                  << std::setw(12) << hit_str
                  << "\n";

        csv << "DepthScaling,AlphaBeta," << depth << ",16,"
            << res_no_tt.nodes_visited << "," << res_tt.nodes_visited << ","
            << std::fixed << std::setprecision(2) << saved_pct << ","
            << time_no_tt_ms << "," << time_tt_ms << ","
            << speedup << "," << tt_16mb->stats().hits << ","
            << hit_rate_pct << "\n";
    }

    // ------------------------------------------------------------------------
    // Part 2: Memory Scaling & Cache Sizing (Depth = 9)
    // ------------------------------------------------------------------------
    std::cout << "\n\n--- Part 2: Transposition Table Memory Scaling (Depth = 9) ---\n\n";
    std::cout << std::left
              << std::setw(12) << "Memory (MB)"
              << std::setw(14) << "Capacity"
              << std::setw(14) << "Occupied"
              << std::setw(14) << "Occupancy (%)"
              << std::setw(14) << "Probes"
              << std::setw(14) << "Hits"
              << std::setw(12) << "Hit Rate"
              << "\n";
    print_separator('-', 95);

    const int test_depth = 9;
    const std::vector<size_t> mem_sizes_mb = {1, 4, 16, 64};

    for (size_t mb : mem_sizes_mb) {
        auto custom_tt = std::make_shared<TranspositionTable>(mb);
        AlphaBetaEngine engine(eval, custom_tt);
        const auto warmup_res = engine.search(board, test_depth);
        (void)warmup_res;

        const auto stats = custom_tt->stats();
        const double occ_pct = stats.occupancy() * 100.0;
        const double hit_pct = stats.hit_rate() * 100.0;

        std::string occ_str = std::to_string(static_cast<int>(occ_pct)) + "." +
                              std::to_string(static_cast<int>(occ_pct * 10) % 10) + "%";
        std::string hit_str = std::to_string(static_cast<int>(hit_pct)) + "." +
                              std::to_string(static_cast<int>(hit_pct * 10) % 10) + "%";

        std::cout << std::left
                  << std::setw(12) << (std::to_string(mb) + " MB")
                  << std::setw(14) << stats.capacity
                  << std::setw(14) << stats.entries_occupied
                  << std::setw(14) << occ_str
                  << std::setw(14) << stats.probes
                  << std::setw(14) << stats.hits
                  << std::setw(12) << hit_str
                  << "\n";

        csv << "MemoryScaling,AlphaBeta," << test_depth << "," << mb << ",0,0,0,0,0,0,"
            << stats.hits << "," << hit_pct << "\n";
    }

    // ------------------------------------------------------------------------
    // Part 3: Parallel Lazy SMP with Shared Lock-Free Transposition Table
    // ------------------------------------------------------------------------
    std::cout << "\n\n--- Part 3: Parallel Lazy SMP (4 Threads) with Shared TT vs Baseline (Depth = 9) ---\n\n";

    LazySmpEngine smp_no_tt(eval, 4);
    auto smp_shared_tt = std::make_shared<TranspositionTable>(16);
    LazySmpEngine smp_with_tt(eval, 4, smp_shared_tt);

    const auto par_no_tt = smp_no_tt.search(board, test_depth);
    const auto par_tt = smp_with_tt.search(board, test_depth);

    const double smp_saved_pct = (par_no_tt.nodes_visited > 0)
        ? (100.0 * static_cast<double>(par_no_tt.nodes_visited - par_tt.nodes_visited) / static_cast<double>(par_no_tt.nodes_visited))
        : 0.0;
    const double smp_time_no_tt_ms = par_no_tt.duration.count() / 1000.0;
    const double smp_time_tt_ms = par_tt.duration.count() / 1000.0;
    const double smp_speedup = (smp_time_tt_ms > 0.0) ? (smp_time_no_tt_ms / smp_time_tt_ms) : 1.0;

    std::cout << "  - Lazy SMP (4T, No TT)    : " << par_no_tt.nodes_visited << " nodes in "
              << std::fixed << std::setprecision(2) << smp_time_no_tt_ms << " ms ("
              << static_cast<uint64_t>(par_no_tt.nodes_per_second()) << " NPS)\n";
    std::cout << "  - Lazy SMP (4T, Shared TT): " << par_tt.nodes_visited << " nodes in "
              << std::fixed << std::setprecision(2) << smp_time_tt_ms << " ms ("
              << static_cast<uint64_t>(par_tt.nodes_per_second()) << " NPS)\n";
    std::cout << "  - Node Reduction          : " << std::fixed << std::setprecision(1) << smp_saved_pct << "%\n";
    std::cout << "  - Throughput Speedup      : " << std::fixed << std::setprecision(2) << smp_speedup << "x\n";
    std::cout << "  - Shared TT Hit Rate      : " << std::fixed << std::setprecision(1) << (smp_shared_tt->stats().hit_rate() * 100.0) << "%\n";

    csv << "ParallelLazySMP,LazySMP_4T," << test_depth << ",16,"
        << par_no_tt.nodes_visited << "," << par_tt.nodes_visited << ","
        << std::fixed << std::setprecision(2) << smp_saved_pct << ","
        << smp_time_no_tt_ms << "," << smp_time_tt_ms << ","
        << smp_speedup << "," << smp_shared_tt->stats().hits << ","
        << (smp_shared_tt->stats().hit_rate() * 100.0) << "\n";

    csv.close();
    std::cout << "\nResults successfully exported to: transposition_results.csv\n\n";

    return 0;
}
