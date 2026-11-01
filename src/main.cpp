#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
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

    auto evaluator = std::make_shared<mmb::SimpleEvaluator>();
    mmb::MinimaxEngine minimax_engine(evaluator);
    mmb::AlphaBetaEngine alphabeta_engine(evaluator);

    const int search_depth = 6;

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "Running search with " << minimax_engine.name() << " (Depth " << search_depth << ")..." << std::endl;
    const auto res_minimax = minimax_engine.search(board, search_depth);
    std::cout << "  Best move: Column " << res_minimax.best_move << std::endl;
    std::cout << "  Score: " << res_minimax.score << std::endl;
    std::cout << "  Nodes visited: " << res_minimax.nodes_visited << std::endl;
    std::cout << "  Time elapsed: " << res_minimax.duration.count() << " us (" 
              << std::fixed << std::setprecision(2) << (res_minimax.duration.count() / 1000.0) << " ms)" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(res_minimax.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "Running search with " << alphabeta_engine.name() << " (Depth " << search_depth << ")..." << std::endl;
    const auto res_ab = alphabeta_engine.search(board, search_depth);
    std::cout << "  Best move: Column " << res_ab.best_move << std::endl;
    std::cout << "  Score: " << res_ab.score << std::endl;
    std::cout << "  Nodes visited: " << res_ab.nodes_visited << std::endl;
    std::cout << "  Time elapsed: " << res_ab.duration.count() << " us (" 
              << std::fixed << std::setprecision(2) << (res_ab.duration.count() / 1000.0) << " ms)" << std::endl;
    std::cout << "  Throughput: " << static_cast<uint64_t>(res_ab.nodes_per_second()) << " NPS" << std::endl;

    if (res_minimax.score == res_ab.score) {
        std::cout << "\n[SUCCESS] Both engines agree on the evaluation score (" << res_ab.score << ")!" << std::endl;
        const double reduction = 100.0 * (1.0 - static_cast<double>(res_ab.nodes_visited) / res_minimax.nodes_visited);
        std::cout << "[PRUNING EFFICIENCY] Alpha-Beta pruned " << std::fixed << std::setprecision(2) 
                  << reduction << "% of nodes explored by pure Minimax!" << std::endl;
    } else {
        std::cerr << "\n[MISMATCH ERROR] Scores differ: Minimax=" << res_minimax.score << ", AlphaBeta=" << res_ab.score << std::endl;
    }
    
    return 0;
}
