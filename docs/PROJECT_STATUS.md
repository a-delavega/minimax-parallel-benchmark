# Project Progress & Status Report: `minimax-parallel-benchmark`

**Date:** October 30, 2026  
**Repository:** [https://github.com/a-delavega/minimax-parallel-benchmark](https://github.com/a-delavega/minimax-parallel-benchmark)  
**Current State:** Phase 0 (Setup) Complete & Phase 1 (Subphases 1-3) Complete

---

## 1. Executive Summary

The **`minimax-parallel-benchmark`** project is an applied High-Performance Computing (HPC) research effort designed to benchmark CPU-bound parallel game-tree search algorithms against traditional sequential Alpha-Beta pruning and heavy evaluation heuristics.

The target environment is a high-performance **Connect 4** engine, engineered from the ground up under strict HPC constraints: zero heap allocations on the hot path, cache-friendly data structures, bit-parallel evaluation, and lockless thread-safety.

---

## 2. What We Have Built So Far

### Phase 0: Project Setup & Bitboard Core Engine

#### 1. Zero-Allocation 64-bit Bitboard Representation (`Board`)
- **Memory Footprint:** The entire board fits in **32 bytes** on the stack (`sizeof(Board) == 32`), well within a typical 64-byte CPU L1 cache line.
- **Data Structure:**
  - `uint64_t bitboard[2]`: One 64-bit mask for Player 1 and one for Player 2.
  - `std::array<uint8_t, 7> height`: Next playable bit position per column.
  - `uint8_t moves_played` & `Player current_player`: Turn and terminal detection metadata.
- **7-Bits-per-Column Mapping with Sentinel Bits:**
  - Connect 4 is $7 \times 6 = 42$ playable cells. We allocate **7 bits per column** (rows 0–5 playable, row 6 as a **sentinel bit permanently set to 0**).
  - **Why Sentinel Bits Matter:** In diagonal win detection, bit shifts of $\pm 6$ (`/`) and $\pm 8$ (`\`) could wrap across column edges and cause false-positive wins. Because row 6 is always `0`, any shift trying to cross a column boundary hits a zero mask, completely eliminating false positives in $O(1)$ without conditional branches.
- **$O(1)$ Win Detection:**
  Using double bit-shifts:
  ```cpp
  uint64_t m = b & (b >> d);
  bool won   = (m & (m >> (2 * d))) != 0;
  ```
  Evaluated across all 4 directions ($d = 1$ vertical, $d = 7$ horizontal, $d = 6$ diagonal `/`, $d = 8$ diagonal `\`).

#### 2. Cross-Platform Build Architecture (CMake & C++20)
- Configured for both **Windows (MSVC 2022)** and **Linux (GCC/Clang)**.
- Specific compiler optimization flags:
  - MSVC: `/O2 /GL /LTCG` (Release) with full link-time optimization.
  - GCC/Clang: `-O3 -march=native -pthread -DNDEBUG`.
- Created an extensible project layout (`include/`, `src/`, `tools/`, `tests/`, `benchmarks/`).
- Built CLI tools:
  - `mmb_runner`: Core smoke test executable.
  - `pretty_print`: ASCII board visualizer for terminal debugging.

---

### Phase 1: Sequential Engine Infrastructure (Subphases 1–3)

We initiated Phase 1, breaking it down into granular milestones to maintain a high-quality, professional Git commit history:

#### Milestone 1: Search & Evaluation Contracts
- **`SearchResult` (`include/minimax/search_result.hpp`)**: Struct capturing search telemetry (`best_move`, `score`, `nodes_visited`, execution duration in microseconds, and `nodes_per_second()`).
- **`IEvaluator` (`include/minimax/evaluator.hpp`)**: Abstract interface for heuristic strategies.
- **`IEngine` (`include/minimax/engine.hpp`)**: Abstract interface for game-tree search engines.

#### Milestone 2: Center-Outward Move Ordering
- **`MoveOrderer` (`include/minimax/move_orderer.hpp`)**:
  - Implements the static sequence `[3, 2, 4, 1, 5, 0, 6]`.
  - In Connect 4, column 3 (center) can participate in up to 16 four-in-a-row lines, compared to only 7 for outer columns. Exploring central moves first triggers early Alpha-Beta cutoffs, drastically reducing tree size.
  - Uses zero heap memory.

#### Milestone 3: Raw Depth Leaf Evaluator
- **`SimpleEvaluator` (`include/minimax/evaluators/simple_evaluator.hpp`)**:
  - $O(1)$ terminal evaluator with depth discounting:
    - Win: $+100,000 - \text{depth}$ (incentivizes fastest possible wins).
    - Loss: $-100,000 + \text{depth}$ (incentivizes longest resistance).
    - Draw / Non-terminal: $0$.
  - Designed with minimal compute overhead to benchmark maximum search depth and node throughput (NPS).

---

## 3. Git Commit History (9 Commits on `main`)

```text
* d4f67a2 feat(eval): implement Raw/Simple leaf evaluator
* 4d7152d feat(engine): add static center-outward move ordering for early pruning
* 7cee825 feat(engine): define IEngine, IEvaluator, and SearchResult contracts
* 1371e38 docs: replace raw LaTeX math blocks with clean markdown in README
* 722e29c docs: add comprehensive README with research roadmap and bitboard math
* 76210f1 feat(tools): add CLI smoke test runner and board visualizer
* 9168c6a build: configure cross-platform CMake build system with C++20
* 903606b feat(core): implement 64-bit bitboard representation for Connect 4
* 2738feb chore: add .gitignore for CMake, IDEs, and build artifacts
```

---

## 4. Next Steps in the Roadmap

1. **Milestone 4:** `MinimaxEngine` (Pure sequential brute-force Minimax as golden-truth baseline).
2. **Milestone 5:** `AlphaBetaEngine` (Alpha-Beta pruning engine with move ordering).
3. **Milestone 6:** `ComplexEvaluator` (Positional pattern evaluation with bitmasks).
4. **Milestones 7–10:** Unit tests (Catch2), interactive CLI match runner, automated benchmarks, and research report.
