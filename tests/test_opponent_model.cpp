#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "minimax/opponent/opponent_type.hpp"
#include <array>
#include <numeric>

using namespace mmb;
using Catch::Approx;

TEST_CASE("OpponentModel: Profile Presets & Invariants", "[opponent][model]") {
    SECTION("Optimal profile guarantees zero blunders and maximum tactical awareness") {
        const auto p = OpponentProfile::preset_optimal();
        REQUIRE(p.type == OpponentType::OptimalMinimax);
        REQUIRE(p.blunder_rate == 0.0);
        REQUIRE(p.tactical_awareness == 1.0);
        REQUIRE(p.estimated_depth >= 10);
        REQUIRE(p.to_string() == "OptimalMinimax");
    }

    SECTION("Greedy profile reflects shallow horizon and moderate blunder rate") {
        const auto p = OpponentProfile::preset_greedy();
        REQUIRE(p.type == OpponentType::Greedy);
        REQUIRE(p.estimated_depth == 1);
        REQUIRE(p.blunder_rate > 0.0);
        REQUIRE(p.to_string().find("Greedy") != std::string::npos);
    }

    SECTION("Random profile exhibits maximum blunder rate and zero tactical awareness") {
        const auto p = OpponentProfile::preset_random();
        REQUIRE(p.type == OpponentType::Random);
        REQUIRE(p.blunder_rate >= 0.80);
        REQUIRE(p.tactical_awareness == 0.0);
        REQUIRE(p.estimated_depth == 0);
    }
}

TEST_CASE("OpponentModel: Softmax Probability Distribution", "[opponent][math]") {
    SECTION("Normalizes probabilities to sum to 1.0 across legal columns") {
        std::array<Score, COLS> scores = {100, 200, 500, 800, 500, 200, 100};
        std::array<bool, COLS> legal = {true, true, true, true, true, true, true};

        const auto probs = OpponentMath::softmax(scores, legal, 150.0);
        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);

        REQUIRE(sum == Approx(1.0).margin(1e-6));
        REQUIRE(probs[3] > probs[2]); // Col 3 has highest score, must have highest prob
        REQUIRE(probs[2] == Approx(probs[4])); // Symmetric scores yield equal probabilities
    }

    SECTION("Illegal columns receive exactly zero probability") {
        std::array<Score, COLS> scores = {100, 200, 500, 999, 500, 200, 100};
        // Col 3 (highest score) is illegal (full column)
        std::array<bool, COLS> legal = {true, true, true, false, true, true, true};

        const auto probs = OpponentMath::softmax(scores, legal, 100.0);
        REQUIRE(probs[3] == 0.0);

        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
        REQUIRE(sum == Approx(1.0).margin(1e-6));
        // Next best moves (cols 2 and 4) now have highest probabilities
        REQUIRE(probs[2] > probs[1]);
        REQUIRE(probs[4] > probs[5]);
    }

    SECTION("High temperature approaches uniform distribution") {
        std::array<Score, COLS> scores = {10, 20, 30, 40, 50, 60, 70};
        std::array<bool, COLS> legal = {true, true, true, true, true, true, true};

        const auto probs = OpponentMath::softmax(scores, legal, 100'000.0);
        for (double p : probs) {
            REQUIRE(p == Approx(1.0 / 7.0).margin(1e-2));
        }
    }
}

TEST_CASE("OpponentModel: Epsilon-Greedy Probability Distribution", "[opponent][math]") {
    SECTION("Assigns 1 - eps to argmax and distributes eps uniformly among others") {
        std::array<Score, COLS> scores = {50, 60, 70, 500, 70, 60, 50};
        std::array<bool, COLS> legal = {true, true, true, true, true, true, true};

        constexpr double EPSILON = 0.30;
        const auto probs = OpponentMath::epsilon_greedy(scores, legal, EPSILON);

        // Optimal move (Col 3) receives 1 - eps = 0.70
        REQUIRE(probs[3] == Approx(0.70).margin(1e-6));

        // Remaining 6 moves each receive 0.30 / 6 = 0.05
        for (std::size_t i = 0; i < COLS; ++i) {
            if (i != 3) {
                REQUIRE(probs[i] == Approx(0.05).margin(1e-6));
            }
        }

        const double sum = std::accumulate(probs.begin(), probs.end(), 0.0);
        REQUIRE(sum == Approx(1.0).margin(1e-6));
    }

    SECTION("Zero epsilon degenerates to deterministic argmax") {
        std::array<Score, COLS> scores = {10, 20, 30, 400, 30, 20, 10};
        std::array<bool, COLS> legal = {true, true, true, true, true, true, true};

        const auto probs = OpponentMath::epsilon_greedy(scores, legal, 0.0);
        REQUIRE(probs[3] == 1.0);
        for (std::size_t i = 0; i < COLS; ++i) {
            if (i != 3) {
                REQUIRE(probs[i] == 0.0);
            }
        }
    }
}

TEST_CASE("OpponentModel: Expected Value Calculation", "[opponent][math]") {
    std::array<Score, COLS> scores = {0, 0, 0, 100, 0, 0, 0};
    std::array<double, COLS> probs = {0.0, 0.0, 0.0, 0.8, 0.05, 0.05, 0.1};

    const double ev = OpponentMath::expected_value(scores, probs);
    REQUIRE(ev == Approx(80.0));
}
