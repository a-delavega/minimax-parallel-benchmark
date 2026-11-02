#include "minimax/board.hpp"
#include "minimax/engines/alpha_beta_engine.hpp"
#include "minimax/engines/minimax_engine.hpp"
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include "minimax/ui/board_printer.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace mmb;

void run_smoke_test() {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "               SMOKE TEST DE VERIFICACION                   " << std::endl;
    std::cout << "============================================================" << std::endl;

    Board board;
    render_board(board);

    auto simple_eval = std::make_shared<SimpleEvaluator>();
    auto complex_eval = std::make_shared<ComplexEvaluator>();

    MinimaxEngine mm_simple(simple_eval);
    AlphaBetaEngine ab_simple(simple_eval);
    AlphaBetaEngine ab_complex(complex_eval);

    const int depth = 6;

    std::cout << "1. MinimaxEngine (SimpleEvaluator, Profundidad " << depth << ")..." << std::endl;
    const auto res_mm = mm_simple.search(board, depth);
    std::cout << "   Jugada: Col " << res_mm.best_move << " | Score: " << res_mm.score
              << " | Nodos: " << res_mm.nodes_visited << " | Tiempo: " << res_mm.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_mm.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "2. AlphaBetaEngine (SimpleEvaluator, Profundidad " << depth << ")..." << std::endl;
    const auto res_ab_s = ab_simple.search(board, depth);
    std::cout << "   Jugada: Col " << res_ab_s.best_move << " | Score: " << res_ab_s.score
              << " | Nodos: " << res_ab_s.nodes_visited << " | Tiempo: " << res_ab_s.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_ab_s.nodes_per_second()) << " NPS" << std::endl;

    std::cout << "3. AlphaBetaEngine (ComplexEvaluator, Profundidad " << depth << ")..." << std::endl;
    const auto res_ab_c = ab_complex.search(board, depth);
    std::cout << "   Jugada: Col " << res_ab_c.best_move << " | Score: " << res_ab_c.score
              << " | Nodos: " << res_ab_c.nodes_visited << " | Tiempo: " << res_ab_c.duration.count() << " us"
              << " | Throughput: " << static_cast<uint64_t>(res_ab_c.nodes_per_second()) << " NPS" << std::endl;

    const double reduction = 100.0 * (1.0 - static_cast<double>(res_ab_s.nodes_visited) / res_mm.nodes_visited);
    std::cout << "\n[RESULTADO] Eficiencia de poda Alpha-Beta vs Minimax: " 
              << std::fixed << std::setprecision(2) << reduction << "% de nodos ahorrados." << std::endl;
}

void play_human_vs_engine(int depth = 6, bool human_starts = true, bool use_complex = true) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "            MODO INTERACTIVO: HUMANO VS MOTOR               " << std::endl;
    std::cout << "============================================================" << std::endl;

    std::shared_ptr<IEvaluator> eval;
    if (use_complex) {
        eval = std::make_shared<ComplexEvaluator>();
        std::cout << "Configuracion: AlphaBetaEngine con ComplexEvaluator (Profundidad " << depth << ")" << std::endl;
    } else {
        eval = std::make_shared<SimpleEvaluator>();
        std::cout << "Configuracion: AlphaBetaEngine con SimpleEvaluator (Profundidad " << depth << ")" << std::endl;
    }

    AlphaBetaEngine engine(eval);
    Board board;

    const Player human_player = human_starts ? Player::Player1 : Player::Player2;
    const Player ai_player = opponent(human_player);

    std::cout << "Humano juega como: " << (human_player == Player::Player1 ? "Jugador 1 [X]" : "Jugador 2 [O]") << std::endl;
    std::cout << "Motor IA juega como: " << (ai_player == Player::Player1 ? "Jugador 1 [X]" : "Jugador 2 [O]") << std::endl;
    std::cout << "(Escribe 'q' en cualquier momento para salir)\n" << std::endl;

    while (!board.is_draw() && !board.has_won(Player::Player1) && !board.has_won(Player::Player2)) {
        render_board(board);

        if (board.current_player == human_player) {
            bool valid_input = false;
            while (!valid_input) {
                std::cout << "Tu turno. Selecciona columna [0-6]: ";
                std::string input;
                if (!(std::cin >> input)) return;

                if (input == "q" || input == "Q") {
                    std::cout << "Partida finalizada por el usuario." << std::endl;
                    return;
                }

                if (input.length() == 1 && input[0] >= '0' && input[0] <= '6') {
                    const Column col = input[0] - '0';
                    if (board.is_valid_move(col)) {
                        board.make_move(col);
                        valid_input = true;
                    } else {
                        std::cout << ">> Columna " << col << " llena. Elige otra." << std::endl;
                    }
                } else {
                    std::cout << ">> Entrada invalida. Introduce un digito entre 0 y 6." << std::endl;
                }
            }
        } else {
            std::cout << "Pensando jugada del motor..." << std::endl;
            const auto res = engine.search(board, depth);
            std::cout << ">> Motor eligio Columna " << res.best_move 
                      << " (Score: " << res.score 
                      << ", Nodos: " << res.nodes_visited 
                      << ", Tiempo: " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms"
                      << ", " << static_cast<uint64_t>(res.nodes_per_second()) << " NPS)" << std::endl;
            board.make_move(res.best_move);
        }
    }

    render_board(board);

    if (board.has_won(human_player)) {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       ¡ENHORABUENA! Has ganado la partida.                 " << std::endl;
        std::cout << "************************************************************" << std::endl;
    } else if (board.has_won(ai_player)) {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       EL MOTOR IA HA GANADO LA PARTIDA.                    " << std::endl;
        std::cout << "************************************************************" << std::endl;
    } else {
        std::cout << "************************************************************" << std::endl;
        std::cout << "       EMPATE: Tablero completo sin cuatro en raya.         " << std::endl;
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
        std::cout << "                     DUELO DE MOTORES                       " << std::endl;
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
                std::cout << "[Jugada " << std::setw(2) << static_cast<int>(board.moves_played) + 1 << "] P1 (X) -> Col " 
                          << res.best_move << " | Score: " << std::setw(6) << res.score 
                          << " | Nodos: " << std::setw(7) << res.nodes_visited 
                          << " | " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms" << std::endl;
            }
            board.make_move(res.best_move);
        } else {
            const auto res = engine2.search(board, depth2);
            total_nodes2 += res.nodes_visited;
            total_time2 += res.duration;
            if (verbose) {
                std::cout << "[Jugada " << std::setw(2) << static_cast<int>(board.moves_played) + 1 << "] P2 (O) -> Col " 
                          << res.best_move << " | Score: " << std::setw(6) << res.score 
                          << " | Nodos: " << std::setw(7) << res.nodes_visited 
                          << " | " << std::fixed << std::setprecision(2) << (res.duration.count() / 1000.0) << " ms" << std::endl;
            }
            board.make_move(res.best_move);
        }
    }

    if (verbose) {
        render_board(board);
        std::cout << "============================================================" << std::endl;
        if (board.has_won(Player::Player1)) {
            std::cout << "RESULTADO FINAL: GANADOR -> " << name1 << std::endl;
        } else if (board.has_won(Player::Player2)) {
            std::cout << "RESULTADO FINAL: GANADOR -> " << name2 << std::endl;
        } else {
            std::cout << "RESULTADO FINAL: EMPATE" << std::endl;
        }
        std::cout << "Jugadas totales: " << static_cast<int>(board.moves_played) << std::endl;
        std::cout << "Stats P1 (X): " << total_nodes1 << " nodos en " 
                  << std::fixed << std::setprecision(2) << (total_time1.count() / 1000.0) << " ms" << std::endl;
        std::cout << "Stats P2 (O): " << total_nodes2 << " nodos en " 
                  << std::fixed << std::setprecision(2) << (total_time2.count() / 1000.0) << " ms" << std::endl;
        std::cout << "============================================================" << std::endl;
    }
}

void play_engine_match(int games = 6) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << "       TORNEO DE MOTORES: COMPLEX EVAL vs RAW DEPTH         " << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Estrategia A: AlphaBeta + ComplexEvaluator (d = 6)" << std::endl;
    std::cout << "Estrategia B: AlphaBeta + SimpleEvaluator  (d = 8)" << std::endl;
    std::cout << "Total de partidas: " << games << " (alternando primer movimiento)\n" << std::endl;

    int wins_a = 0, wins_b = 0, draws = 0;

    for (int g = 1; g <= games; ++g) {
        Board board;
        const bool a_is_p1 = (g % 2 != 0);

        auto eval_a = std::make_shared<ComplexEvaluator>();
        auto eval_b = std::make_shared<SimpleEvaluator>();
        AlphaBetaEngine engine_a(eval_a);
        AlphaBetaEngine engine_b(eval_b);

        std::cout << "Partida " << g << "/" << games << ": ";
        if (a_is_p1) {
            std::cout << "Estrategia A es P1 [X], Estrategia B es P2 [O] -> ";
        } else {
            std::cout << "Estrategia B es P1 [X], Estrategia A es P2 [O] -> ";
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
                std::cout << "Gana Estrategia A [X] en " << static_cast<int>(board.moves_played) << " jugadas" << std::endl;
            } else {
                wins_b++;
                std::cout << "Gana Estrategia B [X] en " << static_cast<int>(board.moves_played) << " jugadas" << std::endl;
            }
        } else if (board.has_won(Player::Player2)) {
            if (a_is_p1) {
                wins_b++;
                std::cout << "Gana Estrategia B [O] en " << static_cast<int>(board.moves_played) << " jugadas" << std::endl;
            } else {
                wins_a++;
                std::cout << "Gana Estrategia A [O] en " << static_cast<int>(board.moves_played) << " jugadas" << std::endl;
            }
        } else {
            draws++;
            std::cout << "Empate en 42 jugadas" << std::endl;
        }
    }

    std::cout << "\n============================================================" << std::endl;
    std::cout << "                  MARCADOR DEL TORNEO                       " << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Estrategia A (Complex d=6): " << wins_a << " victorias" << std::endl;
    std::cout << "Estrategia B (Simple  d=8): " << wins_b << " victorias" << std::endl;
    std::cout << "Empates:                   " << draws << std::endl;
    std::cout << "============================================================" << std::endl;
}

void print_help(const char* prog) {
    std::cout << "Uso: " << prog << " [opciones]\n\n"
              << "Opciones disponibles:\n"
              << "  -s, --smoke       Ejecuta el smoke test de verificacion inicial\n"
              << "  -h, --human       Inicia partida interactiva Humano vs Motor\n"
              << "  -d, --duel        Ejecuta un duelo de una partida entre motores\n"
              << "  -m, --match [N]   Ejecuta un match de N partidas alternadas (defecto: 6)\n"
              << "  --help            Muestra esta ayuda\n"
              << "\nSin argumentos, se abre el menu interactivo de consola." << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== minimax-parallel-benchmark: CLI Suite (Fase 1) ===" << std::endl;

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
        if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
        std::cerr << "Opcion no reconocida: " << arg << "\n" << std::endl;
        print_help(argv[0]);
        return 1;
    }

    // Interactive Menu
    while (true) {
        std::cout << "\n------------------------------------------------------------\n"
                  << "Selecciona una opcion:\n"
                  << "  [1] Jugar contra la Maquina (Humano vs Motor AlphaBeta)\n"
                  << "  [2] Duelo entre Motores (ComplexEval d=6 vs RawDepth d=8)\n"
                  << "  [3] Torneo Automatizado de 6 partidas alternadas\n"
                  << "  [4] Smoke Test de Telemetria\n"
                  << "  [0] Salir\n"
                  << "Opcion: ";
        
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
        } else if (choice == "0" || choice == "q" || choice == "Q") {
            std::cout << "Saliendo del programa..." << std::endl;
            break;
        } else {
            std::cout << ">> Opcion no valida." << std::endl;
        }
    }

    return 0;
}
