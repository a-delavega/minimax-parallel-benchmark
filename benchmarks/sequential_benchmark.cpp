#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace mmb;

struct BenchmarkRecord {
    std::string position_name;
    std::string engine_name;
    std::string evaluator_name;
    int depth = 0;
    Column best_move = -1;
    Score score = 0;
    uint64_t nodes = 0;
    double duration_ms = 0.0;
    double nps = 0.0;
    double effective_b = 0.0; // b* = (nodes)^(1/depth)
};

// Test position 1: Empty board (Opening)
Board create_opening_board() {
    return Board{};
}

// Test position 2: Asymmetric midgame (8 moves played)
Board create_midgame_board() {
    Board b;
    b.make_move(3); // P1
    b.make_move(3); // P2
    b.make_move(2); // P1
    b.make_move(4); // P2
    b.make_move(3); // P1
    b.make_move(2); // P2
    b.make_move(4); // P1
    b.make_move(1); // P2
    return b;
}

// Test position 3: Tactical complex tension (14 moves played with multiple threats)
Board create_tactical_board() {
    Board b;
    // Moves to build diagonal and horizontal threats
    b.make_move(3); // P1
    b.make_move(3); // P2
    b.make_move(2); // P1
    b.make_move(4); // P2
    b.make_move(3); // P1
    b.make_move(2); // P2
    b.make_move(4); // P1
    b.make_move(1); // P2
    b.make_move(2); // P1
    b.make_move(5); // P2
    b.make_move(1); // P1
    b.make_move(0); // P2
    b.make_move(3); // P1
    b.make_move(4); // P2
    return b;
}

void print_table(const std::vector<BenchmarkRecord>& records) {
    std::cout << "\n"
              << "+-----------------+------------------+-------+------+---------+-----------+-----------+------------+-------+\n"
              << "| Posicion        | Motor/Evaluador  | Prof. | Jug. | Score   | Nodos     | Tiempo ms | NPS        | b*    |\n"
              << "+-----------------+------------------+-------+------+---------+-----------+-----------+------------+-------+\n";

    for (const auto& r : records) {
        std::cout << "| " << std::left  << std::setw(15) << r.position_name
                  << " | " << std::left  << std::setw(16) << r.engine_name
                  << " | " << std::right << std::setw(5)  << r.depth
                  << " | " << std::right << std::setw(4)  << r.best_move
                  << " | " << std::right << std::setw(7)  << r.score
                  << " | " << std::right << std::setw(9)  << r.nodes
                  << " | " << std::right << std::setw(9)  << std::fixed << std::setprecision(2) << r.duration_ms
                  << " | " << std::right << std::setw(10) << static_cast<uint64_t>(r.nps)
                  << " | " << std::right << std::setw(5)  << std::fixed << std::setprecision(2) << r.effective_b
                  << " |\n";
    }

    std::cout << "+-----------------+------------------+-------+------+---------+-----------+-----------+------------+-------+\n";
}

void export_csv(const std::vector<BenchmarkRecord>& records, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "No se pudo crear el archivo CSV: " << filename << std::endl;
        return;
    }

    file << "position,engine,evaluator,depth,best_move,score,nodes,duration_ms,nps,effective_branching_factor\n";
    for (const auto& r : records) {
        file << r.position_name << ","
             << r.engine_name << ","
             << r.evaluator_name << ","
             << r.depth << ","
             << r.best_move << ","
             << r.score << ","
             << r.nodes << ","
             << std::fixed << std::setprecision(4) << r.duration_ms << ","
             << std::fixed << std::setprecision(2) << r.nps << ","
             << std::fixed << std::setprecision(3) << r.effective_b << "\n";
    }

    std::cout << "\n[OK] Datos exportados exitosamente a: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "============================================================" << std::endl;
    std::cout << "   minimax-parallel-benchmark: HARNESS DE BENCHMARK (Fase 1) " << std::endl;
    std::cout << "============================================================" << std::endl;

    auto simple_eval = std::make_shared<SimpleEvaluator>();
    auto complex_eval = std::make_shared<ComplexEvaluator>();

    MinimaxEngine mm_simple(simple_eval);
    AlphaBetaEngine ab_simple(simple_eval);
    AlphaBetaEngine ab_complex(complex_eval);

    std::vector<std::pair<std::string, Board>> positions = {
        {"Apertura (Vacio)", create_opening_board()},
        {"Medio Juego (8)",  create_midgame_board()},
        {"Tension Tac. (14)", create_tactical_board()}
    };

    std::vector<BenchmarkRecord> records;
    records.reserve(64);

    for (const auto& [pos_name, board] : positions) {
        std::cout << "\n>> Evaluando banco: " << pos_name << "..." << std::endl;

        // 1. Minimax puro (profundidades 1 a 6)
        for (int d = 1; d <= 6; ++d) {
            const auto res = mm_simple.search(board, d);
            const double ms = res.duration.count() / 1000.0;
            const double b_star = (d > 0 && res.nodes_visited > 0) ? std::pow(static_cast<double>(res.nodes_visited), 1.0 / d) : 0.0;
            records.push_back({
                pos_name,
                "Minimax",
                "Simple",
                d,
                res.best_move,
                res.score,
                res.nodes_visited,
                ms,
                res.nodes_per_second(),
                b_star
            });
        }

        // 2. Alpha-Beta con SimpleEvaluator (profundidades 1 a 9)
        for (int d = 1; d <= 9; ++d) {
            const auto res = ab_simple.search(board, d);
            const double ms = res.duration.count() / 1000.0;
            const double b_star = (d > 0 && res.nodes_visited > 0) ? std::pow(static_cast<double>(res.nodes_visited), 1.0 / d) : 0.0;
            records.push_back({
                pos_name,
                "AlphaBeta",
                "Simple",
                d,
                res.best_move,
                res.score,
                res.nodes_visited,
                ms,
                res.nodes_per_second(),
                b_star
            });
        }

        // 3. Alpha-Beta con ComplexEvaluator (profundidades 1 a 9)
        for (int d = 1; d <= 9; ++d) {
            const auto res = ab_complex.search(board, d);
            const double ms = res.duration.count() / 1000.0;
            const double b_star = (d > 0 && res.nodes_visited > 0) ? std::pow(static_cast<double>(res.nodes_visited), 1.0 / d) : 0.0;
            records.push_back({
                pos_name,
                "AlphaBeta",
                "Complex",
                d,
                res.best_move,
                res.score,
                res.nodes_visited,
                ms,
                res.nodes_per_second(),
                b_star
            });
        }
    }

    print_table(records);

    std::string csv_file = "benchmark_phase1_results.csv";
    if (argc > 1) {
        csv_file = argv[1];
    }
    export_csv(records, csv_file);

    return 0;
}
