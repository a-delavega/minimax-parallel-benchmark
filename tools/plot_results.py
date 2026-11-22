#!/usr/bin/env python3
"""
tools/plot_results.py - Automated Benchmark Plotting Suite
Generates publication-quality charts from minimax-parallel-benchmark CSV telemetry.

Outputs:
  - blitz_depth_vs_budget.png : Search Horizon achieved within strict Blitz deadlines
  - blitz_deadline_variance.png: Adherence to time budgets (Delta ms vs Target ms)
  - tt_hit_rate_vs_memory.png   : Transposition Table cache hit % across memory footprints
  - speedup_scalability.png     : Lazy SMP multi-threaded speedup scaling
"""

import os
import sys
import csv
import math

try:
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
except ImportError:
    print("[ERROR] matplotlib is required to run plot_results.py.")
    print("        Install with: pip install matplotlib")
    sys.exit(1)


def set_plot_style():
    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    plt.rcParams.update({
        'font.family': 'sans-serif',
        'font.size': 11,
        'axes.labelsize': 12,
        'axes.titlesize': 13,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.titlesize': 14,
        'figure.dpi': 300,
        'savefig.dpi': 300,
        'lines.linewidth': 2.0,
        'lines.markersize': 7
    })


def plot_blitz_benchmarks(csv_path="blitz_results.csv", output_dir="docs/assets"):
    os.makedirs(output_dir, exist_ok=True)
    if not os.path.exists(csv_path):
        print(f"[WARN] {csv_path} not found. Skipping blitz plots.")
        return

    data = {}
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            eng = f"{row['Engine']} ({row['Threads']}T)"
            b_ms = int(row['Budget_ms'])
            el_ms = float(row['Elapsed_ms'])
            dev_ms = float(row['Deviation_ms'])
            depth = int(row['DepthReached'])
            nps = float(row['NPS'])

            if eng not in data:
                data[eng] = {"budgets": [], "elapsed": [], "deviation": [], "depth": [], "nps": []}
            data[eng]["budgets"].append(b_ms)
            data[eng]["elapsed"].append(el_ms)
            data[eng]["deviation"].append(dev_ms)
            data[eng]["depth"].append(depth)
            data[eng]["nps"].append(nps)

    # 1. Depth vs Budget Plot
    fig, ax = plt.subplots(figsize=(8, 5))
    colors = {"AlphaBeta (1T)": "#1f77b4", "LazySMP (2T)": "#ff7f0e", "LazySMP (4T)": "#2ca02c", "LazySMP (8T)": "#d62728"}

    for eng, vals in data.items():
        c = colors.get(eng, None)
        ax.plot(vals["budgets"], vals["depth"], marker='o', label=eng, color=c)

    ax.set_title("Blitz Search Horizon: Reached Depth vs Time Budget")
    ax.set_xlabel("Time Budget (ms)")
    ax.set_ylabel("Depth Horizon (Plies)")
    ax.set_xscale('log')
    ax.set_xticks([10, 25, 50, 100, 250, 500])
    ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
    ax.set_ylim(bottom=0)
    ax.legend(loc="lower right")
    ax.grid(True, linestyle="--", alpha=0.6)

    fig.tight_layout()
    p1 = os.path.join(output_dir, "blitz_depth_vs_budget.png")
    fig.savefig(p1)
    plt.close(fig)
    print(f"[SAVED] {p1}")

    # 2. Deadline Adherence Plot (Elapsed vs Target)
    fig, ax = plt.subplots(figsize=(8, 5))
    budgets = [10, 25, 50, 100, 250, 500]
    ax.plot(budgets, budgets, 'k--', label="Exact Budget Limit", alpha=0.7)

    for eng, vals in data.items():
        c = colors.get(eng, None)
        ax.plot(vals["budgets"], vals["elapsed"], marker='s', label=f"{eng} Elapsed", color=c)

    ax.set_title("Real-Time Deadline Adherence (Elapsed vs Budget Limit)")
    ax.set_xlabel("Target Budget (ms)")
    ax.set_ylabel("Actual Elapsed Time (ms)")
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xticks(budgets)
    ax.set_yticks(budgets)
    ax.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
    ax.get_yaxis().set_major_formatter(ticker.ScalarFormatter())
    ax.legend(loc="upper left")
    ax.grid(True, which="both", linestyle="--", alpha=0.5)

    fig.tight_layout()
    p2 = os.path.join(output_dir, "blitz_deadline_variance.png")
    fig.savefig(p2)
    plt.close(fig)
    print(f"[SAVED] {p2}")


def main():
    set_plot_style()
    plot_blitz_benchmarks("blitz_results.csv", "docs/assets")
    print("[OK] Plot generation complete.")


if __name__ == "__main__":
    main()
