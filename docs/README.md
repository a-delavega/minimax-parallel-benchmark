# Project Documentation

Welcome to the technical documentation for **`minimax-parallel-benchmark`**, a high-performance computing (HPC) research repository evaluating parallel game-tree search strategies on CPU architectures using Connect 4.

---

## 📑 Research Reports

| Document | Focus & Highlights | Status |
|---|---|:---:|
| [**Project Overview & Roadmap**](00_introduction_and_objectives.md) | High-level research motivation, problem statement, Connect 4 state space, and multidisciplinary roadmap. | **Complete** |
| [**Phase 0: Bitboard Core Architecture**](01_phase0_bitboard_architecture.md) | 64-bit Bitboard model, 32-byte cache line alignment, mathematical proof of sentinel bits, $\mathcal{O}(1)$ branchless win detection, CMake build system. | **Complete** |
| [**Phase 1: Sequential Engine & Benchmarks**](02_phase1_sequential_engine_and_benchmarks.md) | Pure Minimax vs. Alpha-Beta pruning, static & tactical move ordering, Raw Depth vs. Complex Evaluation, Catch2 test suite (22 tests, 116 assertions), interactive CLI, duel simulator, and empirical analysis ($b^* < 3.0$). | **Complete** |
| [**Phase 2: Parallel Search (Lazy SMP)**](phase2_lazy_smp.md) | Multi-threaded tree search sharing search bounds without mutex contention on the hot path. | *Upcoming* |
| [**Phase 3: Game Theory & Adaptability**](phase3_game_theory.md) | Exploitative adjustments against suboptimal or erratic opponents. | *Planned* |
| [**Phase 4: Transposition Tables & Zobrist Hash**](phase4_transposition_tables.md) | Lock-free concurrent Transposition Tables backed by 64-bit Zobrist Hashing. | *Planned* |
| [**Phase 5: Real-Time Control & Iterative Deepening**](phase5_real_time_control.md) | Iterative Deepening search bounded by strict per-move time budgets. | *Planned* |

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
