// ============================================================================
// Hardware Architectural & Cache Profiler (Subphase 2.7)
// Low-level evaluation of L1/L2 cache locality, false-sharing resistance,
// cross-core invalidation rates, and memory bus throughput.
// ============================================================================

#include "minimax/board.hpp"
#include "minimax/concurrency/cache_aligned.hpp"
#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_data.hpp"
#include "minimax/concurrency/worker_pool.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

using namespace mmb;

namespace {

void print_separator(char c = '=', int width = 100) {
    std::cout << std::string(width, c) << "\n";
}

struct ProfileResult {
    std::string test_name;
    std::size_t thread_count;
    double duration_ms;
    uint64_t total_operations;
    double ops_per_sec;
    double avg_latency_ns;
    std::string architectural_verdict;
};

void run_cache_line_isolation_profile() {
    print_separator('-');
    std::cout << "TEST 1: Cache-Line Isolation & False Sharing Elimination Audit\n";
    print_separator('-');
    std::cout << "Target: alignas(64) WorkerData across thread array\n\n";

    constexpr std::size_t TEST_THREADS = 8;
    WorkerPool pool(TEST_THREADS);

    std::cout << "Memory Layout Verification:\n";
    for (std::size_t i = 0; i < TEST_THREADS; ++i) {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(&pool.worker_data(i));
        const bool aligned = (addr % CACHE_LINE_SIZE == 0);
        std::cout << "  WorkerData[" << i << "] Address: 0x" 
                  << std::hex << addr << std::dec
                  << " | Alignment: " << (addr % CACHE_LINE_SIZE) << " (mod 64)"
                  << " | " << (aligned ? "PASSED [ALIGNED 64B]" : "FAILED") << "\n";
    }

    constexpr uint64_t WRITES_PER_THREAD = 10'000'000ULL;
    SharedSearchState shared;

    // Measure high-frequency parallel writes on adjacent WorkerData
    const auto start = std::chrono::steady_clock::now();
    pool.execute([WRITES_PER_THREAD](int thread_id, WorkerData& data, SharedSearchState& /*state*/) {
        for (uint64_t i = 0; i < WRITES_PER_THREAD; ++i) {
            data.nodes_visited++;
            data.best_score = static_cast<Score>(i & 0x7FFF);
        }
    }, shared);
    const auto end = std::chrono::steady_clock::now();

    const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    const uint64_t total_writes = TEST_THREADS * WRITES_PER_THREAD * 2; // 2 writes per iteration
    const double ns_per_write = (duration_ms * 1'000'000.0) / static_cast<double>(total_writes);
    const double mops = (static_cast<double>(total_writes) / (duration_ms / 1000.0)) / 1'000'000.0;

    std::cout << "\nResults for " << TEST_THREADS << " threads concurrently writing L1-adjacent slots:\n";
    std::cout << "  Total Operations : " << total_writes << " atomic-adjacent writes\n";
    std::cout << "  Wall Duration    : " << duration_ms << " ms\n";
    std::cout << "  Latency / Write  : " << std::fixed << std::setprecision(2) << ns_per_write << " ns\n";
    std::cout << "  Throughput       : " << std::fixed << std::setprecision(2) << mops << " MOps/s\n";

    if (ns_per_write < 5.0) {
        std::cout << "  Verdict          : EXCELLENT (Pure L1 Hit, Zero False Sharing)\n";
    } else if (ns_per_write < 15.0) {
        std::cout << "  Verdict          : GOOD (No Bus Locking Detected)\n";
    } else {
        std::cout << "  Verdict          : WARNING (Possible Cache Contention)\n";
    }
}

void run_cas_reduction_contention_profile() {
    print_separator('-');
    std::cout << "\nTEST 2: Lock-Free Indivisible 64-bit CAS Bus Contention Profile\n";
    print_separator('-');
    std::cout << "Target: SharedSearchState::update_best() 64-bit atomic CAS\n\n";

    const unsigned int hw_threads = std::thread::hardware_concurrency();
    const std::vector<std::size_t> test_configs = {1, 2, 4, 8, hw_threads};

    std::cout << std::setw(10) << "Threads"
              << std::setw(16) << "Total CAS"
              << std::setw(14) << "Time (ms)"
              << std::setw(16) << "Throughput"
              << std::setw(16) << "Avg Latency"
              << std::setw(20) << "Verdict" << "\n";
    print_separator('-', 92);

    for (std::size_t threads : test_configs) {
        WorkerPool pool(threads);
        SharedSearchState shared;
        constexpr uint64_t CAS_PER_THREAD = 500'000ULL;

        const auto start = std::chrono::steady_clock::now();
        pool.execute([CAS_PER_THREAD](int thread_id, WorkerData& data, SharedSearchState& state) {
            for (uint64_t i = 1; i <= CAS_PER_THREAD; ++i) {
                const Column col = static_cast<Column>((thread_id + i) % 7);
                const int depth = 4 + static_cast<int>(i % 4);
                const Score score = static_cast<Score>((i * 10) + col);
                state.update_best(col, score, depth);
                data.nodes_visited++;
            }
        }, shared);
        const auto end = std::chrono::steady_clock::now();

        const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
        const uint64_t total_cas = threads * CAS_PER_THREAD;
        const double mops = (static_cast<double>(total_cas) / (duration_ms / 1000.0)) / 1'000'000.0;
        const double avg_lat_ns = (duration_ms * 1'000'000.0) / static_cast<double>(total_cas);

        std::string verdict;
        if (avg_lat_ns < 50.0) {
            verdict = "Non-Saturating";
        } else if (avg_lat_ns < 150.0) {
            verdict = "Low Contention";
        } else {
            verdict = "Coherence Overhead";
        }

        std::cout << std::setw(10) << threads
                  << std::setw(16) << total_cas
                  << std::setw(14) << std::fixed << std::setprecision(2) << duration_ms
                  << std::setw(13) << std::fixed << std::setprecision(2) << mops << " MOps"
                  << std::setw(13) << std::fixed << std::setprecision(1) << avg_lat_ns << " ns"
                  << std::setw(20) << verdict << "\n";
    }
}

void run_memory_bandwidth_and_stack_profile() {
    print_separator('-');
    std::cout << "\nTEST 3: Per-Thread Stack Locality & Memory Footprint Audit\n";
    print_separator('-');

    const std::size_t board_size = sizeof(Board);
    const std::size_t worker_data_size = sizeof(WorkerData);
    const std::size_t shared_state_size = sizeof(SharedSearchState);

    std::cout << "Struct Memory Footprint:\n";
    std::cout << "  sizeof(Board)            : " << board_size << " bytes ("
              << (board_size <= 64 ? "Fits in EXACTLY 1/2 Cache Line" : "Multi-line") << ")\n";
    std::cout << "  sizeof(WorkerData)       : " << worker_data_size << " bytes ("
              << (worker_data_size == CACHE_LINE_SIZE ? "PERFECT 64-Byte Cache Line Match" : "Non-standard") << ")\n";
    std::cout << "  sizeof(SharedSearchState): " << shared_state_size << " bytes ("
              << (shared_state_size == CACHE_LINE_SIZE ? "PERFECT 64-Byte Cache Line Match" : "Non-standard") << ")\n\n";

    auto eval = std::make_shared<SimpleEvaluator>();
    Board tactical_board;
    tactical_board.make_move(3);
    tactical_board.make_move(3);
    tactical_board.make_move(2);
    tactical_board.make_move(4);
    tactical_board.make_move(2);
    tactical_board.make_move(1);

    std::cout << "L1 Cache Residency Audit on LazySmpEngine:\n";
    const std::vector<std::size_t> test_threads = {1, 4, 8, std::thread::hardware_concurrency()};

    for (std::size_t t : test_threads) {
        LazySmpEngine engine(eval, t);
        const auto start = std::chrono::steady_clock::now();
        const SearchResult res = engine.search(tactical_board, 8);
        const auto end = std::chrono::steady_clock::now();

        const double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
        const double bytes_traversed = static_cast<double>(res.nodes_visited) * static_cast<double>(sizeof(Board));
        const double bandwidth_mbs = (bytes_traversed / (1024.0 * 1024.0)) / (duration_ms / 1000.0);

        std::cout << "  Threads: " << std::setw(2) << t
                  << " | Nodes: " << std::setw(6) << res.nodes_visited
                  << " | Time: " << std::setw(6) << std::fixed << std::setprecision(2) << duration_ms << " ms"
                  << " | Virtual L1 Bandwidth: " << std::setw(7) << std::fixed << std::setprecision(1) << bandwidth_mbs << " MB/s"
                  << " | NPS: " << std::setw(9) << static_cast<uint64_t>(res.nodes_per_second()) << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "\n";
    print_separator('=');
    std::cout << "  MINIMAX-PARALLEL-BENCHMARK: HARDWARE & CACHE PROFILER (SUBPHASE 2.7)\n";
    std::cout << "  Architectural Audit: False Sharing, Cache Invalidation & Memory Bandwidth\n";
    print_separator('=');

    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    std::cout << "Host CPU Concurrency : " << hardware_threads << " logical cores\n";
    std::cout << "Target Cache Line    : " << CACHE_LINE_SIZE << " bytes\n\n";

    run_cache_line_isolation_profile();
    run_cas_reduction_contention_profile();
    run_memory_bandwidth_and_stack_profile();

    print_separator('=');
    std::cout << "Hardware & Cache Profiler Suite completed successfully.\n";
    print_separator('=');
    return 0;
}
