#pragma once

#include "minimax/concurrency/shared_search_state.hpp"
#include "minimax/concurrency/worker_data.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

namespace mmb {

/**
 * @brief High-performance, zero-mutex worker thread manager for Lazy SMP search.
 * 
 * Architecture:
 * - Thread 0 (Master): Executes search on the calling thread to eliminate startup latency.
 * - Threads 1..N-1 (Helpers): Executed concurrently via std::jthread.
 * - All worker states are 64-byte cache-aligned to strictly avoid False Sharing.
 * - Zero mutexes or locking primitives on the search recursion hot-path.
 */
class WorkerPool {
public:
    explicit WorkerPool(std::size_t num_threads = default_thread_count())
        : m_thread_count(std::max<std::size_t>(1, num_threads)),
          m_workers(m_thread_count) {
        for (std::size_t i = 0; i < m_thread_count; ++i) {
            m_workers[i].reset(static_cast<int>(i));
        }
    }

    [[nodiscard]] static std::size_t default_thread_count() noexcept {
        const unsigned int hw = std::thread::hardware_concurrency();
        return (hw > 0) ? hw : 1;
    }

    [[nodiscard]] std::size_t thread_count() const noexcept {
        return m_thread_count;
    }

    void set_thread_count(std::size_t count) {
        m_thread_count = std::max<std::size_t>(1, count);
        m_workers.resize(m_thread_count);
        for (std::size_t i = 0; i < m_thread_count; ++i) {
            m_workers[i].reset(static_cast<int>(i));
        }
    }

    [[nodiscard]] WorkerData& worker_data(std::size_t index) noexcept {
        return m_workers[index];
    }

    [[nodiscard]] const WorkerData& worker_data(std::size_t index) const noexcept {
        return m_workers[index];
    }

    [[nodiscard]] uint64_t total_nodes_visited() const noexcept {
        uint64_t total = 0;
        for (const auto& w : m_workers) {
            total += w.nodes_visited;
        }
        return total;
    }

    /**
     * @brief Dispatches a parallel task across all workers using the Master-Worker topology.
     * 
     * @tparam Func Invocable with signature: void(int thread_id, WorkerData& data, SharedSearchState& state)
     * @param task Search or evaluation task to execute concurrently.
     * @param shared Shared lock-free coordination state.
     */
    template <typename Func>
    void execute(Func&& task, SharedSearchState& shared) {
        shared.reset();
        for (std::size_t i = 0; i < m_thread_count; ++i) {
            m_workers[i].reset(static_cast<int>(i));
        }

        // Launch helper threads (1..N-1)
        std::vector<std::jthread> helpers;
        helpers.reserve(m_thread_count > 1 ? m_thread_count - 1 : 0);

        for (std::size_t i = 1; i < m_thread_count; ++i) {
            helpers.emplace_back([this, i, &task, &shared]() {
                task(static_cast<int>(i), m_workers[i], shared);
                shared.completed_workers.fetch_add(1, std::memory_order_release);
            });
        }

        // Master thread (thread 0) executes on the caller thread directly
        task(0, m_workers[0], shared);
        shared.completed_workers.fetch_add(1, std::memory_order_release);

        // When the master thread completes, signal helper threads to terminate
        shared.request_stop();

        // Join all helpers (std::jthread destructor joins automatically, but explicit join ensures prompt aggregation)
        for (auto& helper : helpers) {
            if (helper.joinable()) {
                helper.join();
            }
        }
    }

private:
    std::size_t m_thread_count;
    std::vector<WorkerData> m_workers;
};

} // namespace mmb
