#include <catch2/catch_test_macros.hpp>
#include "minimax/evaluators/complex_evaluator.hpp"
#include "minimax/evaluators/simple_evaluator.hpp"

using namespace mmb;

TEST_CASE("SimpleEvaluator: Scoring Logic", "[evaluators]") {
    SimpleEvaluator eval;
    Board board;

    // Neutral position
    REQUIRE(eval.evaluate(board, Player::Player1, 0) == 0);
    REQUIRE(eval.evaluate(board, Player::Player2, 0) == 0);

    // Setup a win for Player 1
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1 wins!

    REQUIRE(eval.evaluate(board, Player::Player1, 2) == SimpleEvaluator::WIN_SCORE - 2);
    REQUIRE(eval.evaluate(board, Player::Player2, 2) == -SimpleEvaluator::WIN_SCORE + 2);
}

TEST_CASE("ComplexEvaluator: Center Preference", "[evaluators]") {
    ComplexEvaluator eval;
    Board board1;
    Board board2;

    // Board 1: Player 1 plays in center column 3
    board1.make_move(3);

    // Board 2: Player 1 plays in lateral column 0
    board2.make_move(0);

    const Score score_center = eval.evaluate(board1, Player::Player1, 0);
    const Score score_lateral = eval.evaluate(board2, Player::Player1, 0);

    REQUIRE(score_center > score_lateral);
}

TEST_CASE("ComplexEvaluator: 3-in-a-Row Threat Detection", "[evaluators]") {
    ComplexEvaluator eval;
    Board board;

    // P1 makes 3 horizontal pieces (cols 1, 2, 3 row 0)
    // P2 plays harmlessly in row 1 of other columns
    board.make_move(1); // P1 (1,0)
    board.make_move(5); // P2 (5,0)
    board.make_move(2); // P1 (2,0)
    board.make_move(5); // P2 (5,1)
    board.make_move(3); // P1 (3,0)
    board.make_move(5); // P2 (5,2)

    // Player 1 has a double threat (col 0 and col 4 are both winning squares!)
    const Score score_p1 = eval.evaluate(board, Player::Player1, 0);
    REQUIRE(score_p1 > 1000); // Substantial threat score
}

TEST_CASE("ComplexEvaluator: Terminal State Dominance", "[evaluators]") {
    ComplexEvaluator eval;
    Board board;

    // Terminal win for P1
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1
    board.make_move(1); // P2
    board.make_move(0); // P1 wins!

    const Score win_score = eval.evaluate(board, Player::Player1, 5);
    REQUIRE(win_score == ComplexEvaluator::WIN_SCORE - 5);
    // Win score is far higher than any non-terminal heuristic score
    REQUIRE(win_score > 50'000);
}
