#!/usr/bin/env python3
"""
minimax-parallel-benchmark: Phase 1 Performance Plot Generator

Reads benchmark CSV output (e.g. benchmark_phase1_results.csv) and generates
publication-quality charts:
1. Nodes Explored vs Depth (Log Scale) - Demonstrating Alpha-Beta pruning collapse.
2. Effective Branching Factor (b*) vs Depth - Convergence toward theoretical optimum.
3. Search Throughput (NPS) - Simple vs Complex Evaluator trade-offs.
4. Composite Performance Dashboard (3-in-1 figure).

Author: Alejandro de la Vega
"""

import argparse
import sys
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# Modern clean aesthetic styling
plt.style.use("seaborn-v0_8-whitegrid" if "seaborn-v0_8-whitegrid" in plt.style.available else "default")
plt.rcParams.update({
    "font.family": "sans-serif",
    "font.size": 11,
    "axes.titlesize": 13,
    "axes.labelsize": 12,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "legend.fontsize": 10,
    "figure.titlesize": 15,
    "lines.linewidth": 2.2,
    "lines.markersize": 7,
    "grid.alpha": 0.4,
})

COLORS = {
    "Minimax (Simple)": "#d9534f",     # Red
    "AlphaBeta (Simple)": "#0275d8",   # Blue
    "AlphaBeta (Complex)": "#5cb85c",  # Green
}

MARKERS = {
    "Minimax (Simple)": "s",
    "AlphaBeta (Simple)": "o",
    "AlphaBeta (Complex)": "^",
}


def load_and_clean_data(csv_path: Path) -> pd.DataFrame:
    """Load benchmark CSV and prepare series labels."""
    if not csv_path.exists():
        raise FileNotFoundError(f"Benchmark CSV not found: {csv_path}")
    
    df = pd.read_csv(csv_path)
    # Create unified series label
    df["series"] = df["engine"] + " (" + df["evaluator"] + ")"
    return df


def plot_nodes_explored(df: pd.DataFrame, outdir: Path):
    """Plot nodes explored vs depth on a logarithmic scale for the Opening position."""
    fig, ax = plt.subplots(figsize=(8, 5.5), dpi=300)
    
    sub_df = df[df["position"].str.startswith("Opening")]
    
    for series_name, group in sub_df.groupby("series"):
        color = COLORS.get(series_name, "#333333")
        marker = MARKERS.get(series_name, "o")
        ax.plot(group["depth"], group["nodes"], label=series_name,
                color=color, marker=marker)
    
    ax.set_yscale("log")
    ax.set_xlabel("Search Depth (plies)")
    ax.set_ylabel("Nodes Explored (Log Scale)")
    ax.set_title("Search Space Reduction: Minimax vs. Alpha-Beta Pruning\n(Connect 4 - Opening Position)")
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax.yaxis.set_major_formatter(ticker.LogFormatterSciNotation())
    ax.grid(True, which="both", ls="--", alpha=0.3)
    ax.legend(frameon=True, facecolor="white", framealpha=0.9)
    
    # Annotate pruning ratio at depth 6
    d6 = sub_df[sub_df["depth"] == 6]
    mm_nodes = d6[d6["series"] == "Minimax (Simple)"]["nodes"].values
    ab_nodes = d6[d6["series"] == "AlphaBeta (Simple)"]["nodes"].values
    if len(mm_nodes) > 0 and len(ab_nodes) > 0:
        ratio = (1.0 - (ab_nodes[0] / mm_nodes[0])) * 100.0
        ax.annotate(
            f"Depth 6 Pruning:\n{ratio:.1f}% reduction\n({mm_nodes[0]:,} vs {ab_nodes[0]:,} nodes)",
            xy=(6, ab_nodes[0]),
            xytext=(6.2, ab_nodes[0] * 5),
            arrowprops=dict(facecolor="#333333", shrink=0.08, width=1, headwidth=6),
            fontsize=9,
            bbox=dict(boxstyle="round,pad=0.4", fc="#fff3cd", ec="#ffeeba", alpha=0.9),
        )

    plt.tight_layout()
    output_path = outdir / "nodes_explored_log_scale.png"
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[OK] Generated: {output_path}")


def plot_effective_branching_factor(df: pd.DataFrame, outdir: Path):
    """Plot effective branching factor b* vs depth."""
    fig, ax = plt.subplots(figsize=(8, 5.5), dpi=300)
    
    sub_df = df[df["position"].str.startswith("Opening")]
    
    for series_name, group in sub_df.groupby("series"):
        color = COLORS.get(series_name, "#333333")
        marker = MARKERS.get(series_name, "o")
        ax.plot(group["depth"], group["effective_branching_factor"], label=series_name,
                color=color, marker=marker)
    
    # Add theoretical limits
    ax.axhline(y=7.0, color="#d9534f", linestyle=":", linewidth=1.5,
               label=r"Full Branching ($b = 7.0$)")
    ax.axhline(y=2.646, color="#0275d8", linestyle="--", linewidth=1.5,
               label=r"Theoretical Optimum ($b^* = \sqrt{7} \approx 2.65$)")
    
    ax.set_xlabel("Search Depth (plies)")
    ax.set_ylabel(r"Effective Branching Factor ($b^* = \sqrt[d]{N}$)")
    ax.set_title("Effective Branching Factor ($b^*$) vs. Search Depth\n(Opening Position)")
    ax.set_ylim(2.0, 8.0)
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax.legend(frameon=True, facecolor="white", framealpha=0.9, loc="upper right")
    ax.grid(True, ls="--", alpha=0.4)
    
    plt.tight_layout()
    output_path = outdir / "effective_branching_factor.png"
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[OK] Generated: {output_path}")


def plot_throughput_nps(df: pd.DataFrame, outdir: Path):
    """Plot search throughput (NPS in Millions) for Simple vs Complex evaluators."""
    fig, ax = plt.subplots(figsize=(8, 5.5), dpi=300)
    
    # Filter for AlphaBeta at depth >= 4 where timing is statistically stable
    ab_df = df[(df["engine"] == "AlphaBeta") & (df["depth"] >= 4) & (df["position"].str.startswith("Opening"))]
    
    for series_name, group in ab_df.groupby("series"):
        color = COLORS.get(series_name, "#333333")
        marker = MARKERS.get(series_name, "o")
        nps_millions = group["nps"] / 1_000_000.0
        ax.plot(group["depth"], nps_millions, label=series_name,
                color=color, marker=marker)
    
    ax.set_xlabel("Search Depth (plies)")
    ax.set_ylabel("Throughput (Million Nodes / Second)")
    ax.set_title("Search Throughput (NPS): Raw Depth vs. Heavy Heuristics\n(AlphaBeta Search)")
    ax.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax.legend(frameon=True, facecolor="white", framealpha=0.9)
    ax.grid(True, ls="--", alpha=0.4)
    
    plt.tight_layout()
    output_path = outdir / "throughput_nps.png"
    plt.savefig(output_path, dpi=300)
    plt.close()
    print(f"[OK] Generated: {output_path}")


def plot_composite_dashboard(df: pd.DataFrame, outdir: Path):
    """Generate a combined 3-panel horizontal publication dashboard."""
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 5.2), dpi=300)
    
    sub_df = df[df["position"].str.startswith("Opening")]
    
    # 1. Nodes Explored
    for series_name, group in sub_df.groupby("series"):
        ax1.plot(group["depth"], group["nodes"], label=series_name,
                 color=COLORS.get(series_name, "#333333"),
                 marker=MARKERS.get(series_name, "o"))
    ax1.set_yscale("log")
    ax1.set_xlabel("Depth (plies)")
    ax1.set_ylabel("Nodes Explored (Log)")
    ax1.set_title("1. Search Effort (Pruning)")
    ax1.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax1.legend(loc="upper left", fontsize=8.5)
    ax1.grid(True, which="both", ls="--", alpha=0.3)
    
    # 2. Effective Branching Factor
    for series_name, group in sub_df.groupby("series"):
        ax2.plot(group["depth"], group["effective_branching_factor"], label=series_name,
                 color=COLORS.get(series_name, "#333333"),
                 marker=MARKERS.get(series_name, "o"))
    ax2.axhline(y=7.0, color="#d9534f", linestyle=":", linewidth=1.3, label=r"Full $b=7.0$")
    ax2.axhline(y=2.646, color="#0275d8", linestyle="--", linewidth=1.3, label=r"Optimal $b^* \approx 2.65$")
    ax2.set_xlabel("Depth (plies)")
    ax2.set_ylabel(r"Effective Branching $b^*$")
    ax2.set_title(r"2. Branching Collapse ($b^* \to 2.9$)")
    ax2.set_ylim(2.0, 8.0)
    ax2.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax2.legend(loc="upper right", fontsize=8.5)
    ax2.grid(True, ls="--", alpha=0.3)
    
    # 3. Search Throughput (NPS)
    ab_df = sub_df[(sub_df["engine"] == "AlphaBeta") & (sub_df["depth"] >= 4)]
    for series_name, group in ab_df.groupby("series"):
        ax3.plot(group["depth"], group["nps"] / 1_000_000.0, label=series_name,
                 color=COLORS.get(series_name, "#333333"),
                 marker=MARKERS.get(series_name, "o"))
    ax3.set_xlabel("Depth (plies)")
    ax3.set_ylabel("Throughput (Million NPS)")
    ax3.set_title("3. Evaluator Throughput")
    ax3.xaxis.set_major_locator(ticker.MaxNLocator(integer=True))
    ax3.legend(loc="upper right", fontsize=8.5)
    ax3.grid(True, ls="--", alpha=0.3)
    
    plt.suptitle("Phase 1 Benchmark Suite: Sequential Minimax vs. Alpha-Beta Evaluation Metrics", y=1.02)
    plt.tight_layout()
    output_path = outdir / "phase1_benchmark_dashboard.png"
    plt.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"[OK] Generated: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate benchmark plots for Phase 1 results.")
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path("benchmark_phase1_results.csv"),
        help="Path to benchmark_phase1_results.csv",
    )
    parser.add_argument(
        "--outdir",
        type=Path,
        default=Path("docs/assets/plots"),
        help="Target output directory for images (default: docs/assets/plots)",
    )
    args = parser.parse_args()

    args.outdir.mkdir(parents=True, exist_ok=True)
    df = load_and_clean_data(args.csv)

    print(f"Loaded {len(df)} benchmark records from {args.csv}")
    print(f"Generating charts to: {args.outdir}")

    plot_nodes_explored(df, args.outdir)
    plot_effective_branching_factor(df, args.outdir)
    plot_throughput_nps(df, args.outdir)
    plot_composite_dashboard(df, args.outdir)
    print("All benchmark charts successfully generated.")


if __name__ == "__main__":
    main()
