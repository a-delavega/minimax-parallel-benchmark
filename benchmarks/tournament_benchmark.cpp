// ============================================================================
// Multi-Engine Tournament & Empirical Cross-Table Benchmark (Subphase 3.5)
// Round-Robin empirical evaluation across classical Minimax, Alpha-Beta,
// Lazy SMP, ExploitEngine, Expectiminimax, Greedy, and Random agents.
// ============================================================================

#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/expectiminimax_engine.hpp"
#include "minimax/engines/exploit_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/opponent/heuristic_agents.hpp"
#include "minimax/tournament/tournament_harness.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace mmb;

namespace {

void print_separator(char c = '=', int width = 95) {
    std::cout << std::string(width, c) << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    print_separator('=');
    std::cout << "  MINIMAX-PARALLEL-BENCHMARK: ROUND-ROBIN TOURNAMENT (SUBPHASE 3.5)\n";
    std::cout << "  Empirical Evaluation: Game Theory, Exploitation, and Suboptimal Adversaries\n";
    print_separator('=');
    std::cout << "\n";

    auto eval = std::make_shared<SimpleEvaluator>();

    TournamentHarness harness;

    // 1. Rational Benchmark Baselines
    auto seq_ab = std::make_shared<AlphaBetaEngine>(eval);
    auto lazy_smp = std::make_shared<LazySmpEngine>(eval, 4);

    // 2. Game-Theoretic Exploitative & Probabilistic Engines
    auto exploit_novice = std::make_shared<ExploitEngine>(eval, OpponentProfile::preset_novice(), 0.70);
    auto expectiminimax = std::make_shared<ExpectiminimaxEngine>(eval, nullptr, 100.0);

    // 3. Suboptimal Adversaries
    auto greedy = std::make_shared<GreedyEngine>(eval);
    auto random = std::make_shared<RandomEngine>(1337ULL);

    harness.add_participant("alpha_beta", "Alpha-Beta (d=6)", seq_ab, 6);
    harness.add_participant("lazy_smp", "Lazy SMP (4T, d=6)", lazy_smp, 6);
    harness.add_participant("exploit", "ExploitEngine (d=6)", exploit_novice, 6);
    harness.add_participant("expectiminimax", "Expectiminimax (d=4)", expectiminimax, 4);
    harness.add_participant("greedy", "Greedy (1-Ply)", greedy, 1);
    harness.add_participant("random", "Random Agent", random, 1);

    std::cout << "Registered " << harness.participant_count() << " tournament participants.\n";
    constexpr int GAMES_PER_PAIR = 4; // 2 as P1, 2 as P2 per pair
    std::cout << "Running Round-Robin Tournament: " << GAMES_PER_PAIR << " games per pairing (balanced colors)...\n\n";

    const auto start_total = std::chrono::steady_clock::now();
    const auto records = harness.run_tournament(GAMES_PER_PAIR);
    const auto end_total = std::chrono::steady_clock::now();
    const double total_duration_s = std::chrono::duration<double>(end_total - start_total).count();

    const auto standings = harness.compute_standings(records);

    // Print Markdown Report to stdout
    const std::string md_report = harness.format_markdown(records, standings);
    std::cout << md_report << "\n";

    print_separator('-');
    std::cout << "Tournament completed in " << std::fixed << std::setprecision(2) << total_duration_s << " seconds.\n";
    print_separator('-');

    // Save CSV Export
    const std::string csv_data = harness.format_csv(records);
    const std::string csv_path = "tournament_results.csv";
    std::ofstream csv_file(csv_path);
    if (csv_file.is_open()) {
        csv_file << csv_data;
        std::cout << "Exported tournament cross-table to: " << csv_path << "\n";
    }

    print_separator('=');
    return 0;
}
