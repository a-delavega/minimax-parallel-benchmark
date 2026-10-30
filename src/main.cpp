#include "minimax/board.hpp"
#include <iostream>

int main() {
    std::cout << "=== minimax-parallel-benchmark: Core Engine Init ===" << std::endl;
    
    mmb::Board board;
    std::cout << "Board initialized successfully." << std::endl;
    std::cout << "Memory size of struct Board: " << sizeof(board) << " bytes." << std::endl;
    std::cout << "Initial occupancy bitmask: " << board.occupied() << std::endl;
    
    return 0;
}
