#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include <iomanip>
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== minimax-parallel-benchmark: Core Engine Init ===" << std::endl;
    
    mmb::Board board;
    std::cout << "Board initialized successfully." << std::endl;
    std::cout << "Memory size of struct Board: " << sizeof(board) << " bytes." << std::endl;
    std::cout << "Initial occupancy bitmask: " << board.occupied() << std::endl;

    auto simple_eval = std::make_shared<mmb::SimpleEvaluator>();
    auto complex_eval = std::make_shared<mmb::ComplexEvaluator>();

    mmb::AlphaBetaEngine ab_simple(simple_eval);
    mmb::AlphaBetaEngine ab_complex(complex_eval);

    const int depth = 6;

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "Searching initial board with " << ab_simple.name() << "\n  using: " << simple_eval->name() << " (Depth " << depth << ")..." << std::endl;
    const auto res_s = ab_simple.search(board, depth);
    std::cout << "  Best move: Column " << res_s.best_move << std::endl;
    std::cout << "  Score: " << res_s.score << std::endl;
    std::cout << "  Nodes visited: " << res_s.nodes_visited << std::endl;
    std::cout << "  Time elapsed: " << res_s.duration.count() << " us (" 
              << std::fixed << std::setprecision(2) << (res_s.duration.count() / 1000.0) << " ms)" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(res_s.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "Searching initial board with " << ab_complex.name() << "\n  using: " << complex_eval->name() << " (Depth " << depth << ")..." << std::endl;
    const auto res_c = ab_complex.search(board, depth);
    std::cout << "  Best move: Column " << res_c.best_move << std::endl;
    std::cout << "  Score: " << res_c.score << std::endl;
    std::cout << "  Nodes visited: " << res_c.nodes_visited << std::endl;
    std::cout << "  Time elapsed: " << res_c.duration.count() << " us (" 
              << std::fixed << std::setprecision(2) << (res_c.duration.count() / 1000.0) << " ms)" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(res_c.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "\n[EVALUATION COMPARISON]" << std::endl;
    std::cout << "  Simple Evaluator recommended Col " << res_s.best_move << " with Score " << res_s.score << std::endl;
    std::cout << "  Complex Evaluator recommended Col " << res_c.best_move << " with Score " << res_c.score << " (strategic positional advantage)" << std::endl;

    return 0;
}
