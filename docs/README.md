# Project Documentation

Welcome to the technical documentation for **`minimax-parallel-benchmark`**, a high-performance computing (HPC) research repository evaluating parallel game-tree search strategies on CPU architectures using Connect 4.

---

## 📑 Research Reports

| Document | Focus & Highlights | Status |
|---|---|:---:|
| [**Project Overview & Roadmap**](00_introduction_and_objectives.md) | High-level research motivation, problem statement, Connect 4 state space, and multidisciplinary roadmap. | **Complete** |
| [**Phase 0: Bitboard Core Architecture**](01_phase0_bitboard_architecture.md) | 64-bit Bitboard model, 32-byte cache line alignment, mathematical proof of sentinel bits, $O(1)$ branchless win detection, CMake build system. | **Complete** |
| [**Phase 1: Sequential Engine & Benchmarks**](02_phase1_sequential_engine_and_benchmarks.md) | Pure Minimax vs. Alpha-Beta pruning, static & tactical move ordering, Raw Depth vs. Complex Evaluation, Catch2 test suite (22 tests, 116 assertions), interactive CLI, duel simulator, and empirical analysis ($b^* < 3.0$). | **Complete** |
| [**Phase 2: Parallel Search (Lazy SMP)**](03_phase2_lazy_smp_and_concurrency.md) | Multi-threaded tree search, cache-line isolation (`alignas(64)`), search diversification, and lock-free indivisible 64-bit CAS reduction. | **Complete** |
| [**Phase 3: Game Theory & Adaptability**](04_phase3_game_theory_and_suboptimal_opponents.md) | Exploitative adjustments against suboptimal opponents, Bayesian belief models, and Expectiminimax chance nodes. | **Complete** |
| [**Phase 4: Transposition Tables & Zobrist Hash**](05_phase4_transposition_tables_and_zobrist_hashing.md) | Lock-free concurrent Transposition Tables backed by 64-bit Zobrist Hashing, Hyatt XOR verification, and Two-Tier buckets. | **Complete** |
| [**Phase 5: Real-Time Control & Iterative Deepening**](06_phase5_real_time_control_and_iterative_deepening.md) | Iterative Deepening Search (IDS), Aspiration Windows, dynamic TimeManager, zero-latency cooperative stop, and blitz benchmarks. | **Complete** |

---

## 🚀 Quick Execution Guide

Run the pre-compiled tools directly from your terminal:

```powershell
# Interactive Human vs AI gameplay
.\build\Release\mmb_runner.exe --human

# Automated Engine vs Engine duel
.\build\Release\mmb_runner.exe --duel

# Automated benchmark harness across 3 test suites
.\build\benchmarks\Release\mmb_benchmark.exe

# Unit test suite
.\build\tests\Release\mmb_tests.exe
```
