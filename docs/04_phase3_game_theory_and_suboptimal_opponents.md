# 🧠 Phase 3: Game Theory & Exploitative Search against Suboptimal Adversaries

**Document Code**: `DOC-04-PHASE3`  
**Author**: Alejandro de la Vega Zuluaga (`a-delavega`)  
**Status**: In Progress (Subphases 3.1, 3.2, 3.3)  

---

## 1. Abstract & Mathematical Motivation

Classical zero-sum game tree search algorithms (**Minimax** and **Alpha-Beta**) are founded on the strict assumption of **bilateral perfection**:
$$\forall n \in \text{Opponent Nodes}, \quad V(n) = \min_{m \in \text{Moves}(n)} V(m)$$

Under the von Neumann Minimax Theorem and Nash Equilibrium, this strategy is **unexploitable** and guarantees the game-theoretic value $V^*$ of the game. If Connect 4 is played from the root by two perfect rational players, Player 1 (first player) possesses a forced theoretical win with perfect play.

### The Minimax Fallacy Against Flawed Adversaries

In competitive real-world settings, human opponents and heuristic AI agents deviate systematically from game-theoretic optimality:
1. **Bounded Rationality & Horizon Effects**: Calculating 4 plies ahead instead of 12.
2. **Greedy Heuristic Biases**: Maximizing immediate 1-ply board score without detecting multi-ply counterattacks.
3. **Tactical Blindness to Forks**: Overlooking simultaneous dual threats (e.g. two concurrent 3-in-a-row connections with an open ends).

When facing a flawed adversary, **pure Minimax is overly conservative (passive)**. Consider a fork in the game tree:
* **Branch A (Minimax Safe Path)**: A dull symmetrical variation leading deterministically to a theoretical draw ($V = 0$).
* **Branch B (Tactical Trap)**: An aggressive move that creates a lethal trap:
  - If the opponent finds the *unique saving move* $m_1$, the position yields $V(m_1) = -10$ (a negligible tactical disadvantage).
  - If the opponent plays *any of the other 6 legal moves* $\{m_2, \dots, m_7\}$, they blunder immediately into a forced 4-in-a-row loss ($V = +100\,000$).

Under standard Minimax:
$$V(B) = \min(-10, +100\,000, \dots) = -10 < V(A) = 0$$
Minimax unconditionally selects **Branch A**, discarding the trap because in the worst-case scenario the opponent plays like a supercomputer.

However, if the opponent blunders with probability $P(\text{error}) = 80\%$:
$$\mathbb{E}[V(B)] = (0.20 \times -10) + (0.80 \times 100\,000) = +79\,998 \gg 0$$

**Opponent-Exploitative Minimax** bridges this gap by maintaining strict minimax safety guards while actively steering play toward high-entropy positions where the opponent's probability of failure is maximized.

---

## 2. Stochastic Opponent Decision Modeling

We model the opponent's move selection via parameterized probability distributions:

### 2.1. Boltzmann / Softmax Rationality Model

Given legal moves $m \in M(n)$ and evaluated scores $S(m)$:
$$P(m) = \frac{\exp\left(\frac{S(m) - \max_k S(k)}{T}\right)}{\sum_{j \in M(n)} \exp\left(\frac{S(j) - \max_k S(k)}{T}\right)}$$
* High Temperature ($T \to \infty$): Decouples into uniform exploration (Random agent).
* Low Temperature ($T \to 0$): Collapses into standard $\arg\max$ selection (Optimal Minimax).

### 2.2. $\epsilon$-Greedy Error Distribution

With probability $1 - \epsilon$, the opponent selects the optimal defense:
$$P(m^*) = 1 - \epsilon, \quad m^* = \arg\max_{m} S(m)$$
With probability $\epsilon$, the opponent distributes moves across suboptimal alternatives:
$$\forall m \neq m^*, \quad P(m) = \frac{\epsilon}{|M(n)| - 1}$$

---

## 3. The Exploitative Utility Formulation

The exploitative node evaluation $V_{\text{exploit}}(n)$ balances minimax robustness with probabilistic payoff:
$$V_{\text{exploit}}(n) = (1 - \lambda) V_{\text{minimax}}(n) + \lambda \mathbb{E}_{m \sim P_{\text{opp}}}[V(m)] + \Delta_{\text{trap}}(n)$$
Where:
* $\lambda \in [0.0, 1.0]$: Exploitation weight ($\lambda = 0$ recovers pure Alpha-Beta).
* $\Delta_{\text{trap}}(n)$: Tactical trap bonus proportional to the **Induced Error Factor** ($k$ losing alternatives vs 1 saving path).

### Zero-Regret Minimax Safety Invariant

To guarantee that exploitative optimism never commits suicide:
1. If $V_{\text{minimax}}(n) \le -\text{MATE\_THRESHOLD}$, the engine refuses the move regardless of trap payoff.
2. If a provable forced mate exists ($V(n) \ge \text{MATE\_THRESHOLD}$), the forced win is executed immediately.

---

## 4. Bayesian Belief Updating Over Adversarial Hypotheses

Rather than assuming fixed opponent parameters, the `BayesianOpponentModel` maintains a discrete probability distribution over behavioral hypotheses:
$$\Theta = \{\text{OptimalMinimax}, \text{Greedy}, \text{ShallowHeuristic}, \text{HumanNovice}, \text{Random}\}$$

Starting from a uniform prior $P(\theta_k) = 1 / |\Theta|$, each observed move $m_{\text{obs}}$ triggers a Bayesian belief update:
$$P(\theta_k \mid m_{\text{obs}}, B) = \frac{P(m_{\text{obs}} \mid \theta_k, B) P(\theta_k)}{\sum_j P(m_{\text{obs}} \mid \theta_j, B) P(\theta_j)}$$

### Predictive Marginalization

Future opponent actions are forecasted by marginalizing across the hypothesis space:
$$P(m \mid B) = \sum_{k} P(m \mid \theta_k, B) P(\theta_k)$$

This marginal probability directly feeds the chance nodes of the `ExpectiminimaxEngine`.

---

## 5. Expectiminimax Game-Tree Search

The `ExpectiminimaxEngine` evaluates game trees containing two distinct node types:
1. **Decision Nodes (Our Turn)**: Maximize deterministic payoff:
   $$V(n) = \max_{m \in M(n)} V(m)$$
2. **Chance Nodes (Opponent Turn)**: Compute the expected utility over the opponent's predictive distribution:
   $$V(n) = \sum_{m \in M(n)} P(m \mid B) V(m)$$

---

## 6. Empirical Round-Robin Tournament Results

Using `TournamentHarness` (`mmb_tournament`), 6 competitive and heuristic engines were evaluated in a balanced round-robin tournament (alternating colors):

| Rank | Engine | Played | Wins | Draws | Losses | Points | Win Rate % | Strategy Profile |
| :---: | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **1** | **Expectiminimax (d=4)** | 20 | 14 | 6 | 0 | **17.0** | **70.0%** | Probabilistic Chance Nodes + Bayesian Beliefs |
| **2** | **Alpha-Beta (d=6)** | 20 | 9 | 8 | 3 | **13.0** | **45.0%** | Classical Rational Minimax Baseline |
| **3** | **ExploitEngine (d=6)** | 20 | 8 | 8 | 4 | **12.0** | **40.0%** | Tactical Trap Maximization + Minimax Guard |
| **4** | **Lazy SMP (4T, d=6)** | 20 | 7 | 7 | 6 | **10.5** | **35.0%** | Multi-threaded Asymmetric Search |
| **5** | **Greedy (1-Ply)** | 20 | 5 | 5 | 10 | **7.5** | **25.0%** | 1-Ply Immediate Heuristic Maximizer |
| **6** | **Random Agent** | 20 | 0 | 0 | 20 | **0.0** | **0.0%** | Uniform Random Baseline (Zero Skill) |

### Key Empirical Findings:
1. **Dominance Over Flawed Agents**: Rational and exploitative engines achieved a **100% win rate** against Random agents and outperformed Greedy 1-ply agents decisively.
2. **Expectiminimax Efficacy**: By incorporating chance node probabilities, `ExpectiminimaxEngine` converted high-entropy endgame positions into wins against suboptimal play faster than pure minimax.
