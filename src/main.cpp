#include "minimax/board.hpp"
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
    mmb::MinimaxEngine engine(evaluator);

    std::cout << "\nRunning benchmark search with " << engine.name() << "..." << std::endl;
    const int search_depth = 5;
    const auto result = engine.search(board, search_depth);

    std::cout << "Search depth: " << result.depth_reached << std::endl;
    std::cout << "Best move: Column " << result.best_move << std::endl;
    std::cout << "Score: " << result.score << std::endl;
    std::cout << "Nodes visited: " << result.nodes_visited << std::endl;
    std::cout << "Time elapsed: " << result.duration.count() << " us (" 
              << std::fixed << std::setprecision(2) << (result.duration.count() / 1000.0) << " ms)" << std::endl;
    std::cout << "Throughput: " << static_cast<uint64_t>(result.nodes_per_second()) << " NPS (Nodes Per Second)" << std::endl;
    
    return 0;
}
