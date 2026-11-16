#pragma once

#include "minimax/board.hpp"
#include "minimax/engine.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace mmb {

/**
 * @brief Match outcome statistics between two competitive game-tree engines.
 */
struct HeadToHeadRecord {
    std::string player_a;
    std::string player_b;
    uint32_t a_wins = 0;
    uint32_t b_wins = 0;
    uint32_t draws = 0;
    uint32_t total_moves = 0;
    double total_time_ms = 0.0;

    [[nodiscard]] uint32_t total_games() const noexcept {
        return a_wins + b_wins + draws;
    }

    [[nodiscard]] double a_win_rate() const noexcept {
        const uint32_t total = total_games();
        return (total > 0) ? (static_cast<double>(a_wins) / static_cast<double>(total)) * 100.0 : 0.0;
    }

    [[nodiscard]] double b_win_rate() const noexcept {
        const uint32_t total = total_games();
        return (total > 0) ? (static_cast<double>(b_wins) / static_cast<double>(total)) * 100.0 : 0.0;
    }

    [[nodiscard]] double avg_moves() const noexcept {
        const uint32_t total = total_games();
        return (total > 0) ? static_cast<double>(total_moves) / static_cast<double>(total) : 0.0;
    }
};

/**
 * @brief Participant specification in the tournament.
 */
struct Participant {
    std::string id;
    std::string display_name;
    std::shared_ptr<IEngine> engine;
    int search_depth = 4;
};

/**
 * @brief Overall participant standings across the tournament.
 */
struct Standing {
    std::string name;
    uint32_t played = 0;
    uint32_t wins = 0;
    uint32_t draws = 0;
    uint32_t losses = 0;
    double points = 0.0; // 1.0 for win, 0.5 for draw
    double win_rate = 0.0;
};

/**
 * @brief Automated Round-Robin Tournament Harness for empirical agent evaluation.
 * 
 * Features:
 * 1. Fair Color Alternation: Pairs play an equal number of games as Player 1 and Player 2
 *    to completely neutralize the first-player advantage in Connect 4.
 * 2. Multi-Engine Duels: Supports pure Minimax, Alpha-Beta, Lazy SMP, ExploitEngine,
 *    Expectiminimax, Greedy, and Random agents.
 * 3. Statistical Cross-Table Generation: Computes Win %, Draw %, Loss %, and exports to CSV and Markdown.
 */
class TournamentHarness {
public:
    TournamentHarness() = default;

    void add_participant(
        std::string id,
        std::string display_name,
        std::shared_ptr<IEngine> engine,
        int search_depth = 4
    ) {
        m_participants.push_back(Participant{
            .id = std::move(id),
            .display_name = std::move(display_name),
            .engine = std::move(engine),
            .search_depth = search_depth
        });
    }

    [[nodiscard]] std::size_t participant_count() const noexcept {
        return m_participants.size();
    }

    /**
     * @brief Play a single head-to-head match between two engines.
     * @return 1 if p1_engine wins, 2 if p2_engine wins, 0 if draw.
     */
    static int play_single_game(
        IEngine& p1_engine, int p1_depth,
        IEngine& p2_engine, int p2_depth,
        uint32_t& out_moves
    ) {
        Board board;
        out_moves = 0;

        while (!board.is_draw() && !board.has_won(Player::Player1) && !board.has_won(Player::Player2)) {
            const Player current = board.current_player;
            IEngine& active_engine = (current == Player::Player1) ? p1_engine : p2_engine;
            const int depth = (current == Player::Player1) ? p1_depth : p2_depth;

            const SearchResult res = active_engine.search(board, depth);
            Column move = res.best_move;

            if (move < 0 || move >= COLS || !board.is_valid_move(move)) {
                // Illegal move fallback to first valid move
                for (Column c = 0; c < COLS; ++c) {
                    if (board.is_valid_move(c)) {
                        move = c;
                        break;
                    }
                }
            }

            board.make_move(move);
            out_moves++;

            if (board.has_won(current)) {
                return (current == Player::Player1) ? 1 : 2;
            }
        }

        return 0; // Draw
    }

    /**
     * @brief Executes a full round-robin tournament among all registered participants.
     * @param games_per_pair Total matches per pair (must be even to balance colors).
     */
    [[nodiscard]] std::vector<HeadToHeadRecord> run_tournament(int games_per_pair = 10) {
        std::vector<HeadToHeadRecord> records;
        const std::size_t n = m_participants.size();
        if (n < 2) {
            return records;
        }

        const int half_games = std::max(1, games_per_pair / 2);

        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                Participant& pA = m_participants[i];
                Participant& pB = m_participants[j];

                HeadToHeadRecord rec;
                rec.player_a = pA.display_name;
                rec.player_b = pB.display_name;

                const auto start_time = std::chrono::steady_clock::now();

                // Half 1: A as P1, B as P2
                for (int g = 0; g < half_games; ++g) {
                    uint32_t moves = 0;
                    const int winner = play_single_game(*pA.engine, pA.search_depth, *pB.engine, pB.search_depth, moves);
                    rec.total_moves += moves;
                    if (winner == 1) rec.a_wins++;
                    else if (winner == 2) rec.b_wins++;
                    else rec.draws++;
                }

                // Half 2: B as P1, A as P2
                for (int g = 0; g < half_games; ++g) {
                    uint32_t moves = 0;
                    const int winner = play_single_game(*pB.engine, pB.search_depth, *pA.engine, pA.search_depth, moves);
                    rec.total_moves += moves;
                    if (winner == 1) rec.b_wins++;
                    else if (winner == 2) rec.a_wins++;
                    else rec.draws++;
                }

                const auto end_time = std::chrono::steady_clock::now();
                rec.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                records.push_back(rec);
            }
        }

        return records;
    }

    /**
     * @brief Computes overall standings from head-to-head match records.
     */
    [[nodiscard]] std::vector<Standing> compute_standings(const std::vector<HeadToHeadRecord>& records) const {
        std::vector<Standing> standings;
        for (const auto& p : m_participants) {
            Standing s;
            s.name = p.display_name;
            standings.push_back(s);
        }

        for (const auto& rec : records) {
            for (auto& s : standings) {
                if (s.name == rec.player_a) {
                    s.played += rec.total_games();
                    s.wins += rec.a_wins;
                    s.draws += rec.draws;
                    s.losses += rec.b_wins;
                    s.points += (static_cast<double>(rec.a_wins) * 1.0) + (static_cast<double>(rec.draws) * 0.5);
                } else if (s.name == rec.player_b) {
                    s.played += rec.total_games();
                    s.wins += rec.b_wins;
                    s.draws += rec.draws;
                    s.losses += rec.a_wins;
                    s.points += (static_cast<double>(rec.b_wins) * 1.0) + (static_cast<double>(rec.draws) * 0.5);
                }
            }
        }

        for (auto& s : standings) {
            s.win_rate = (s.played > 0) ? (static_cast<double>(s.wins) / static_cast<double>(s.played)) * 100.0 : 0.0;
        }

        std::sort(standings.begin(), standings.end(), [](const Standing& a, const Standing& b) {
            if (a.points != b.points) return a.points > b.points;
            return a.wins > b.wins;
        });

        return standings;
    }

    /**
     * @brief Generates Markdown formatted cross-table matrix and leaderboard.
     */
    [[nodiscard]] std::string format_markdown(
        const std::vector<HeadToHeadRecord>& records,
        const std::vector<Standing>& standings
    ) const {
        std::ostringstream ss;
        ss << "### Tournament Leaderboard Standings\n\n";
        ss << "| Rank | Engine | Played | Wins | Draws | Losses | Points | Win Rate % |\n";
        ss << "| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: |\n";

        for (std::size_t i = 0; i < standings.size(); ++i) {
            const auto& s = standings[i];
            ss << "| " << (i + 1)
               << " | **" << s.name << "**"
               << " | " << s.played
               << " | " << s.wins
               << " | " << s.draws
               << " | " << s.losses
               << " | " << std::fixed << std::setprecision(1) << s.points
               << " | " << std::fixed << std::setprecision(1) << s.win_rate << "% |\n";
        }

        ss << "\n### Head-to-Head Cross-Table Results\n\n";
        ss << "| Matchup | Score (A - B) | Draws | Total Games | Win Rate A % | Win Rate B % | Avg Moves |\n";
        ss << "| :--- | :---: | :---: | :---: | :---: | :---: | :---: |\n";

        for (const auto& r : records) {
            ss << "| **" << r.player_a << "** vs **" << r.player_b << "**"
               << " | " << r.a_wins << " - " << r.b_wins
               << " | " << r.draws
               << " | " << r.total_games()
               << " | " << std::fixed << std::setprecision(1) << r.a_win_rate() << "%"
               << " | " << std::fixed << std::setprecision(1) << r.b_win_rate() << "%"
               << " | " << std::fixed << std::setprecision(1) << r.avg_moves() << " |\n";
        }

        return ss.str();
    }

    /**
     * @brief Formats head-to-head records into standard CSV.
     */
    [[nodiscard]] std::string format_csv(const std::vector<HeadToHeadRecord>& records) const {
        std::ostringstream ss;
        ss << "player_a,player_b,a_wins,b_wins,draws,total_games,a_win_rate,b_win_rate,avg_moves,total_time_ms\n";
        for (const auto& r : records) {
            ss << "\"" << r.player_a << "\","
               << "\"" << r.player_b << "\","
               << r.a_wins << ","
               << r.b_wins << ","
               << r.draws << ","
               << r.total_games() << ","
               << std::fixed << std::setprecision(2) << r.a_win_rate() << ","
               << std::fixed << std::setprecision(2) << r.b_win_rate() << ","
               << std::fixed << std::setprecision(2) << r.avg_moves() << ","
               << std::fixed << std::setprecision(2) << r.total_time_ms << "\n";
        }
        return ss.str();
    }

private:
    std::vector<Participant> m_participants;
};

} // namespace mmb
