#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/lazy_smp_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/ui/board_printer.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace mmb;

void run_smoke_test() {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "               VERIFICATION SMOKE TEST                      " << std::endl;
    std::cout << "============================================================" << std::endl;

    Board board;
    render_board(board);

    auto simple_eval = std::make_shared<SimpleEvaluator>();
    auto complex_eval = std::make_shared<ComplexEvaluator>();

    MinimaxEngine mm_simple(simple_eval);
    AlphaBetaEngine ab_simple(simple_eval);
    AlphaBetaEngine ab_complex(complex_eval);

    const int depth = 6;

    std::cout << "1. MinimaxEngine (SimpleEvaluator, Depth " << depth << ")..." << std::endl;
    const auto res_mm = mm_simple.search(board, depth);
    std::cout << "   Move: Col " << (res_mm.best_move + 1) << " | Score: " << res_mm.score
              << " | Nodes: " << res_mm.nodes_visited << " | Time: " << res_mm.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_mm.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "2. AlphaBetaEngine (SimpleEvaluator, Depth " << depth << ")..." << std::endl;
    const auto res_ab_s = ab_simple.search(board, depth);
    std::cout << "   Move: Col " << (res_ab_s.best_move + 1) << " | Score: " << res_ab_s.score
              << " | Nodes: " << res_ab_s.nodes_visited << " | Time: " << res_ab_s.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_ab_s.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "3. AlphaBetaEngine (ComplexEvaluator, Depth " << depth << ")..." << std::endl;
    const auto res_ab_c = ab_complex.search(board, depth);
    std::cout << "   Move: Col " << (res_ab_c.best_move + 1) << " | Score: " << res_ab_c.score
              << " | Nodes: " << res_ab_c.nodes_visited << " | Time: " << res_ab_c.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_ab_c.nodes_per_second()) << " NPS" << std::endl;

    const double reduction = 100.0 * (1.0 - static_cast<double>(res_ab_s.nodes_visited) / res_mm.nodes_visited);
    std::cout << "\n[RESULT] Alpha-Beta pruning efficiency vs Minimax: " 
              << std::fixed << std::setprecision(2) << reduction << "% of nodes pruned." << std::endl;

    std::cout << "\n4. LazySmpEngine (SimpleEvaluator, 4 Threads, Depth " << depth << ")..." << std::endl;
    LazySmpEngine lazy_smp(simple_eval, 4);
    const auto res_smp = lazy_smp.search(board, depth);
    std::cout << "   Move: Col " << (res_smp.best_move + 1) << " | Score: " << res_smp.score
              << " | Nodes: " << res_smp.nodes_visited << " | Time: " << res_smp.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_smp.nodes_per_second()) << " NPS" << std::endl;
}

void play_human_vs_engine(int depth = 6, bool human_starts = true, bool use_complex = true) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "            INTERACTIVE MODE: HUMAN VS ENGINE               " << std::endl;
    std::cout << "============================================================" << std::endl;

    std::shared_ptr<IEvaluator> eval;
    if (use_complex) {
        eval = std::make_shared<ComplexEvaluator>();
        std::cout << "Configuration: AlphaBetaEngine with ComplexEvaluator (Depth " << depth << ")" << std::endl;
    } else {
        eval = std::make_shared<SimpleEvaluator>();
        std::cout << "Configuration: AlphaBetaEngine with SimpleEvaluator (Depth " << depth << ")" << std::endl;
    }

    AlphaBetaEngine engine(eval);
    Board board;

    const Player human_player = human_starts ? Player::Player1 : Player::Player2;
    const Player ai_player = opponent(human_player);

    std::cout << "Human player: " << (human_player == Player::Player1 ? "Player 1 [X]" : "Player 2 [O]") << std::endl;
    std::cout << "AI Engine:    " << (ai_player == Player::Player1 ? "Player 1 [X]" : "Player 2 [O]") << std::endl;
    std::cout << "(Type 'q' at any time to exit)\n" << std::endl;

    while (!board.is_draw() && !board.has_won(Player::Player1) && !board.has_won(Player::Player2)) {
        render_board(board);

        if (board.current_player == human_player) {
            bool valid_input = false;
            while (!valid_input) {
                std::cout << "Your turn. Select column [1-7]: ";
                std::string input;
                if (!(std::cin >> input)) return;

                if (input == "q" || input == "Q") {
                    std::cout << "Game aborted by user." << std::endl;
                    return;
                }

                if (input.length() == 1 && input[0] >= '1' && input[0] <= '7') {
                    const Column col = (input[0] - '1'); // Convert 1-based [1, 7] to 0-based [0, 6]
                    if (board.is_valid_move(col)) {
                        board.make_move(col);
                        valid_input = true;
                    } else {
                        std::cout << ">> Column " << (col + 1) << " is full. Please choose another column." << std::endl;
                    }
                } else {
                    std::cout << ">> Invalid input. Please enter a column number from 1 to 7." << std::endl;
                }
            }
        } else {
            std::cout << "AI is searching for the best move..." << std::endl;
            const auto res = engine.search(board, depth);
            std::cout << ">> AI played Column " << (res.best_move + 1) 
                      << " (Score: " << res.score 
                      << ", Nodes: " << res.nodes_visited 
                      << ", Time: " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms"
                      << ", " << static_cast<uint64_t>(res.nodes_per_second()) << " NPS)" << std::endl;
            board.make_move(res.best_move);
        }
    }

    render_board(board);

    if (board.has_won(human_player)) {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       CONGRATULATIONS! You won the game!                   " << std::endl;
        std::cout << "************************************************************" << std::endl;
    } else if (board.has_won(ai_player)) {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       THE AI ENGINE WON THE GAME.                          " << std::endl;
        std::cout << "************************************************************" << std::endl;
    } else {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       DRAW: Board is full with no four-in-a-row.           " << std::endl;
        std::cout << "************************************************************" << std::endl;
    }
}

void play_engine_vs_engine(
    int depth1 = 6, 
    bool complex1 = true, 
    int depth2 = 8, 
    bool complex2 = false, 
    bool verbose = true
) {
    std::shared_ptr<IEvaluator> eval1 = complex1 
        ? std::static_pointer_cast<IEvaluator>(std::make_shared<ComplexEvaluator>())
        : std::static_pointer_cast<IEvaluator>(std::make_shared<SimpleEvaluator>());

    std::shared_ptr<IEvaluator> eval2 = complex2 
        ? std::static_pointer_cast<IEvaluator>(std::make_shared<ComplexEvaluator>())
        : std::static_pointer_cast<IEvaluator>(std::make_shared<SimpleEvaluator>());

    AlphaBetaEngine engine1(eval1);
    AlphaBetaEngine engine2(eval2);

    const std::string name1 = std::string("Engine 1 [X] (") + eval1->name() + ", d=" + std::to_string(depth1) + ")";
    const std::string name2 = std::string("Engine 2 [O] (") + eval2->name() + ", d=" + std::to_string(depth2) + ")";

    if (verbose) {
        std::cout << "\n============================================================" << std::endl;
        std::cout << "                     ENGINE DUEL                            " << std::endl;
        std::cout << "============================================================" << std::endl;
        std::cout << "P1 (X): " << name1 << std::endl;
        std::cout << "P2 (O): " << name2 << std::endl;
        std::cout << "------------------------------------------------------------" << std::endl;
    }

    Board board;
    uint64_t total_nodes1 = 0, total_nodes2 = 0;
    std::chrono::microseconds total_time1{0}, total_time2{0};

    while (!board.is_draw() && !board.has_won(Player::Player1) && !board.has_won(Player::Player2)) {
        if (board.current_player == Player::Player1) {
            const auto res = engine1.search(board, depth1);
            total_nodes1 += res.nodes_visited;
            total_time1 += res.duration;
            if (verbose) {
                std::cout << "[Move " << std::setw(2) << static_cast<int>(board.moves_played) + 1 << "] P1 (X) -> Col " 
                          << (res.best_move + 1) << " | Score: " << std::setw(6) << res.score 
                          << " | Nodes: " << std::setw(7) << res.nodes_visited 
                          << " | " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms" << std::endl;
            }
            board.make_move(res.best_move);
        } else {
            const auto res = engine2.search(board, depth2);
            total_nodes2 += res.nodes_visited;
            total_time2 += res.duration;
            if (verbose) {
                std::cout << "[Move " << std::setw(2) << static_cast<int>(board.moves_played) + 1 << "] P2 (O) -> Col " 
                          << (res.best_move + 1) << " | Score: " << std::setw(6) << res.score 
                          << " | Nodes: " << std::setw(7) << res.nodes_visited 
                          << " | " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms" << std::endl;
            }
            board.make_move(res.best_move);
        }
    }

    if (verbose) {
        render_board(board);
        std::cout << "============================================================" << std::endl;
        if (board.has_won(Player::Player1)) {
            std::cout << "FINAL RESULT: WINNER -> " << name1 << std::endl;
        } else if (board.has_won(Player::Player2)) {
            std::cout << "FINAL RESULT: WINNER -> " << name2 << std::endl;
        } else {
            std::cout << "FINAL RESULT: DRAW" << std::endl;
        }
        std::cout << "Total moves played: " << static_cast<int>(board.moves_played) << std::endl;
        std::cout << "Stats P1 (X): " << total_nodes1 << " nodes in " 
                  << std::fixed << std::setprecision(2) << (total_time1.count() / 1000.0) << " ms" << std::endl;
        std::cout << "Stats P2 (O): " << total_nodes2 << " nodes in " 
                  << std::fixed << std::setprecision(2) << (total_time2.count() / 1000.0) << " ms" << std::endl;
        std::cout << "============================================================" << std::endl;
    }
}

void play_engine_match(int games = 6) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "       ENGINE TOURNAMENT: COMPLEX EVAL vs RAW DEPTH         " << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Strategy A: AlphaBeta + ComplexEvaluator (d = 6)" << std::endl;
    std::cout << "Strategy B: AlphaBeta + SimpleEvaluator  (d = 8)" << std::endl;
    std::cout << "Total games: " << games << " (alternating first player)\n" << std::endl;

    int wins_a = 0, wins_b = 0, draws = 0;

    for (int g = 1; g <= games; ++g) {
        Board board;
        const bool a_is_p1 = (g % 2 != 0);

        auto eval_a = std::make_shared<ComplexEvaluator>();
        auto eval_b = std::make_shared<SimpleEvaluator>();
        AlphaBetaEngine engine_a(eval_a);
        AlphaBetaEngine engine_b(eval_b);

        std::cout << "Game " << g << "/" << games << ": ";
        if (a_is_p1) {
            std::cout << "Strategy A is P1 [X], Strategy B is P2 [O] -> ";
        } else {
            std::cout << "Strategy B is P1 [X], Strategy A is P2 [O] -> ";
        }

        while (!board.is_draw() && !board.has_won(Player::Player1) && !board.has_won(Player::Player2)) {
            if (board.current_player == Player::Player1) {
                const auto res = a_is_p1 ? engine_a.search(board, 6) : engine_b.search(board, 8);
                board.make_move(res.best_move);
            } else {
                const auto res = a_is_p1 ? engine_b.search(board, 8) : engine_a.search(board, 6);
                board.make_move(res.best_move);
            }
        }

        if (board.has_won(Player::Player1)) {
            if (a_is_p1) {
                wins_a++;
                std::cout << "Strategy A [X] wins in " << static_cast<int>(board.moves_played) << " moves" << std::endl;
            } else {
                wins_b++;
                std::cout << "Strategy B [X] wins in " << static_cast<int>(board.moves_played) << " moves" << std::endl;
            }
        } else if (board.has_won(Player::Player2)) {
            if (a_is_p1) {
                wins_b++;
                std::cout << "Strategy B [O] wins in " << static_cast<int>(board.moves_played) << " moves" << std::endl;
            } else {
                wins_a++;
                std::cout << "Strategy A [O] wins in " << static_cast<int>(board.moves_played) << " moves" << std::endl;
            }
        } else {
            draws++;
            std::cout << "Draw in 42 moves" << std::endl;
        }
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "                  TOURNAMENT SCOREBOARD                     " << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Strategy A (Complex d=6): " << wins_a << " wins" << std::endl;
    std::cout << "Strategy B (Simple  d=8): " << wins_b << " wins" << std::endl;
    std::cout << "Draws:                   " << draws << std::endl;
    std::cout << "============================================================" << std::endl;
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Available options:\n"
              << "  -s, --smoke       Run verification smoke test (Sequential & Lazy SMP)\n"
              << "  -h, --human       Start interactive Human vs Engine game\n"
              << "  -d, --duel        Run a single-game duel between engines\n"
              << "  -m, --match [N]   Run an automated match of N alternating games (default: 6)\n"
              << "  -p, --parallel    Run Lazy SMP parallel search duel against Alpha-Beta\n"
              << "  --help            Show this help message\n"
              << "\nWithout arguments, opens the interactive console menu." << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== minimax-parallel-benchmark: CLI Suite (Phase 1) ===" << std::endl;

    if (argc > 1) {
        const std::string arg = argv[1];
        if (arg == "--smoke" || arg == "-s") {
            run_smoke_test();
            return 0;
        }
        if (arg == "--human" || arg == "-h") {
            play_human_vs_engine(6, true, true);
            return 0;
        }
        if (arg == "--duel" || arg == "-d") {
            play_engine_vs_engine(6, true, 8, false, true);
            return 0;
        }
        if (arg == "--match" || arg == "-m") {
            int games = 6;
            if (argc > 2) {
                try {
                    games = std::stoi(argv[2]);
                } catch (...) {}
            }
            play_engine_match(games);
            return 0;
        }
        if (arg == "--parallel" || arg == "-p") {
            const unsigned int hw = std::thread::hardware_concurrency();
            const std::size_t threads = (hw > 1) ? hw : 4;
            std::cout << "\nRunning Lazy SMP (" << threads << " threads) vs Alpha-Beta (1 thread) Duel...\n";
            auto simple_eval = std::make_shared<SimpleEvaluator>();
            auto complex_eval = std::make_shared<ComplexEvaluator>();

            LazySmpEngine engine_smp(simple_eval, threads);
            AlphaBetaEngine engine_seq(complex_eval);

            Board b;
            const auto res_seq = engine_seq.search(b, 7);
            const auto res_par = engine_smp.search(b, 7);

            std::cout << "Sequential Alpha-Beta [d=7]: Move Col " << (res_seq.best_move + 1)
                      << " | " << res_seq.nodes_visited << " nodes | " << res_seq.duration.count() << " us"
                      << " | " << static_cast<uint64_t>(res_seq.nodes_per_second()) << " NPS\n";
            std::cout << "Parallel Lazy SMP    [d=7]: Move Col " << (res_par.best_move + 1)
                      << " | " << res_par.nodes_visited << " nodes | " << res_par.duration.count() << " us"
                      << " | " << static_cast<uint64_t>(res_par.nodes_per_second()) << " NPS\n";
            return 0;
        }
        if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
        std::cerr << "Unrecognized option: " << arg << "\n" << std::endl;
        print_help(argv[0]);
        return 1;
    }

    // Interactive Menu
    while (true) {
        std::cout << "\n------------------------------------------------------------\n"
                  << "Select an option:\n"
                  << "  [1] Play against the AI Engine (Human vs AlphaBeta)\n"
                  << "  [2] Engine Duel (ComplexEval d=6 vs RawDepth d=8)\n"
                  << "  [3] Automated Tournament of 6 alternating games\n"
                  << "  [4] Telemetry Smoke Test (includes Lazy SMP 4-thread)\n"
                  << "  [5] Parallel Lazy SMP Benchmark Duel\n"
                  << "  [0] Exit\n"
                  << "Choice: ";
        
        std::string choice;
        if (!(std::cin >> choice)) break;

        if (choice == "1") {
            play_human_vs_engine(6, true, true);
        } else if (choice == "2") {
            play_engine_vs_engine(6, true, 8, false, true);
        } else if (choice == "3") {
            play_engine_match(6);
        } else if (choice == "4") {
            run_smoke_test();
        } else if (choice == "5") {
            const unsigned int hw = std::thread::hardware_concurrency();
            const std::size_t threads = (hw > 1) ? hw : 4;
            std::cout << "\nRunning Lazy SMP (" << threads << " threads) vs Alpha-Beta (1 thread) Duel...\n";
            auto simple_eval = std::make_shared<SimpleEvaluator>();
            auto complex_eval = std::make_shared<ComplexEvaluator>();

            LazySmpEngine engine_smp(simple_eval, threads);
            AlphaBetaEngine engine_seq(complex_eval);

            Board b;
            const auto res_seq = engine_seq.search(b, 7);
            const auto res_par = engine_smp.search(b, 7);

            std::cout << "Sequential Alpha-Beta [d=7]: Move Col " << (res_seq.best_move + 1)
                      << " | " << res_seq.nodes_visited << " nodes | " << res_seq.duration.count() << " us"
                      << " | " << static_cast<uint64_t>(res_seq.nodes_per_second()) << " NPS\n";
            std::cout << "Parallel Lazy SMP    [d=7]: Move Col " << (res_par.best_move + 1)
                      << " | " << res_par.nodes_visited << " nodes | " << res_par.duration.count() << " us"
                      << " | " << static_cast<uint64_t>(res_par.nodes_per_second()) << " NPS\n";
        } else if (choice == "0" || choice == "q" || choice == "Q") {
            std::cout << "Exiting program..." << std::endl;
            break;
        } else {
            std::cout << ">> Invalid choice. Please try again." << std::endl;
        }
    }

    return 0;
}
