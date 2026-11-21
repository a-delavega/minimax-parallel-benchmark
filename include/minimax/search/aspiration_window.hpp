#pragma once

#include "minimax/search_result.hpp"
#include "minimax/types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>

namespace mmb {

/**
 * @brief Configuration parameters for Aspiration Window search.
 */
struct AspirationConfig {
    Score initial_delta = 50;           // Initial half-width window delta (score +/- delta)
    int min_depth = 2;                  // Minimum depth to engage aspiration windows (d=1 uses [-inf, +inf])
    int max_widening_attempts = 3;      // Max widening attempts before falling back to infinity
    Score max_delta = 10'000;           // Maximum delta before full window fallback
    double widening_factor = 2.0;       // Factor to expand delta upon fail-low or fail-high
};

/**
 * @brief Telemetry metrics tracking aspiration window performance.
 */
struct AspirationStats {
    uint64_t window_hits = 0;           // Searches that concluded successfully inside the window
    uint64_t fail_lows = 0;             // Searches that fell <= alpha requiring downward widening
    uint64_t fail_highs = 0;            // Searches that rose >= beta requiring upward widening
    uint64_t re_searches = 0;           // Total re-searches triggered by fail-lows or fail-highs

    [[nodiscard]] uint64_t total_probes() const noexcept {
        return window_hits + fail_lows + fail_highs;
    }

    [[nodiscard]] double hit_rate() const noexcept {
        const uint64_t total = total_probes();
        return (total > 0) ? (static_cast<double>(window_hits) / static_cast<double>(total)) * 100.0 : 0.0;
    }

    void reset() noexcept {
        window_hits = 0;
        fail_lows = 0;
        fail_highs = 0;
        re_searches = 0;
    }
};

/**
 * @brief Algorithmic helper executing bounded alpha-beta searches with aspiration windows.
 */
class AspirationSearcher {
public:
    /**
     * @brief Executes a search at depth d with an aspiration window centered on prev_score.
     * @tparam SearchFn Callable with signature: SearchResult(int depth, Score alpha, Score beta)
     * @param depth Depth level to search.
     * @param prev_score Score from the previous depth iteration (d-1).
     * @param search_fn Bounded search callable.
     * @param config Aspiration window configuration.
     * @param stats Telemetry statistics tracker.
     * @return SearchResult of the converged search.
     */
    template <typename SearchFn>
    static SearchResult search(
        int depth,
        Score prev_score,
        SearchFn&& search_fn,
        const AspirationConfig& config = AspirationConfig{},
        AspirationStats* stats = nullptr
    ) {
        constexpr Score INF_LOW = std::numeric_limits<Score>::lowest() + 1;
        constexpr Score INF_HIGH = std::numeric_limits<Score>::max() - 1;

        // If depth is below min_depth, or prev_score indicates an absolute mate, search full window
        if (depth < config.min_depth || std::abs(prev_score) >= SCORE_WIN - 100) {
            SearchResult res = search_fn(depth, INF_LOW, INF_HIGH);
            if (stats) {
                stats->window_hits++;
            }
            return res;
        }

        Score delta = config.initial_delta;
        Score alpha = std::max<Score>(prev_score - delta, INF_LOW);
        Score beta = std::min<Score>(prev_score + delta, INF_HIGH);

        SearchResult result;
        int attempts = 0;

        while (true) {
            result = search_fn(depth, alpha, beta);

            if (result.score <= alpha) {
                // Fail-low: true score is worse than alpha
                if (stats) {
                    stats->fail_lows++;
                    stats->re_searches++;
                }

                attempts++;
                if (attempts >= config.max_widening_attempts || delta >= config.max_delta || alpha == INF_LOW) {
                    // Fall back to full minus infinity for alpha
                    alpha = INF_LOW;
                } else {
                    delta = static_cast<Score>(delta * config.widening_factor);
                    alpha = std::max<Score>(prev_score - delta, INF_LOW);
                }
            } else if (result.score >= beta) {
                // Fail-high: true score is better than beta
                if (stats) {
                    stats->fail_highs++;
                    stats->re_searches++;
                }

                attempts++;
                if (attempts >= config.max_widening_attempts || delta >= config.max_delta || beta == INF_HIGH) {
                    // Fall back to full plus infinity for beta
                    beta = INF_HIGH;
                } else {
                    delta = static_cast<Score>(delta * config.widening_factor);
                    beta = std::min<Score>(prev_score + delta, INF_HIGH);
                }
            } else {
                // Window Hit: alpha < score < beta
                if (stats) {
                    stats->window_hits++;
                }
                break;
            }
        }

        return result;
    }
};

} // namespace mmb
