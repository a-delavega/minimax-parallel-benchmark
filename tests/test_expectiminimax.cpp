#include <catch2/catch_test_macros.hpp>
#include "minimax/board.hpp"
#include "minimax/engines/expectiminimax_engine.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"
#include <memory>

using namespace mmb;

TEST_CASE("ExpectiminimaxEngine: Configuration & Interface", "[engine][expectiminimax]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    auto bayesian = std::make_shared<BayesianOpponentModel>();
    ExpectiminimaxEngine engine(eval, bayesian, 150.0);

    SECTION("Engine properties and accessors") {
        REQUIRE(std::string(engine.name()).find("ExpectiminimaxEngine") != std::string::npos);
        REQUIRE(engine.temperature() == 150.0);
        REQUIRE(engine.bayesian_model() == bayesian);
    }

    SECTION("Temperature dynamic tuning") {
        engine.set_temperature(50.0);
        REQUIRE(engine.temperature() == 50.0);
    }
}

TEST_CASE("ExpectiminimaxEngine: Search Execution & Game Logic", "[engine][expectiminimax]") {
    auto eval = std::make_shared<SimpleEvaluator>();
    ExpectiminimaxEngine engine(eval);

    SECTION("Direct immediate win is always executed") {
        Board board;
        // P1 has 3 pieces in column 3
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2
        board.make_move(3); // P1
        board.make_move(0); // P2
        // P1 to move -> col 3 is immediate win
        const SearchResult res = engine.search(board, 3);
        REQUIRE(res.best_move == 3);
        REQUIRE(res.score >= MATE_THRESHOLD);
    }

    SECTION("Selects valid legal moves across increasing depths") {
        Board board;
        board.make_move(3);
        board.make_move(3);

        for (int d = 1; d <= 4; ++d) {
            const SearchResult res = engine.search(board, d);
            REQUIRE(res.best_move >= 0);
            REQUIRE(res.best_move < COLS);
            REQUIRE(board.is_valid_move(res.best_move));
            REQUIRE(res.nodes_visited > 0);
        }
    }
}
