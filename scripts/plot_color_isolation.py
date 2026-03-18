#!/usr/bin/env python3
"""
plot_color_isolation.py
=======================
Generates bar-chart histograms for the Secasy color-operation isolation
diffusion analysis and saves them to docs/de/img/ and docs/en/img/.

Hard-coded from the measured results of SecasyColorIsolation.exe
(100 messages × 32 bytes × 256 bit-flips = 25 600 samples per mode).

Usage:
    python scripts/plot_color_isolation.py
"""

import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

# ── Measured histogram data (bin = 5% wide, 20 bins covering 0–100%) ──
# Format: list of 20 counts, one per 5%-bin.
RESULTS = {
    "Baseline\n(mixed: ADD/SUB/XOR/AND/OR/INVERT)": {
        "bins": [0,0,0,0,0,0,0,0,344,11934,12984,338,0,0,0,0,0,0,0,0],
        "mean": 50.0, "std": 2.2,
    },
    "ADD only": {
        "bins": [0,0,0,0,2,8,10,5,332,11878,13062,303,0,0,0,0,0,0,0,0],
        "mean": 50.0, "std": 2.3,
    },
    "SUB only": {
        "bins": [0,0,0,0,0,0,2,2,306,11931,13048,310,1,0,0,0,0,0,0,0],
        "mean": 50.0, "std": 2.2,
    },
    "XOR only": {
        "bins": [0,1,2,14,43,61,104,144,577,11746,12488,415,5,0,0,0,0,0,0,0],
        "mean": 49.7, "std": 3.3,
    },
    "AND only": {
        "bins": [202,87,103,154,165,248,269,492,1478,10661,10978,738,25,0,0,0,0,0,0,0],
        "mean": 48.1, "std": 7.6,
    },
    "OR only": {
        "bins": [164,21,31,40,81,124,142,181,650,11674,12124,367,1,0,0,0,0,0,0,0],
        "mean": 49.1, "std": 5.8,
    },
    "INVERT only": {
        "bins": [0,0,4,26,60,98,139,180,697,11768,12278,350,0,0,0,0,0,0,0,0],
        "mean": 49.5, "std": 3.6,
    },
}

TOTAL = 25600
BIN_CENTERS = np.arange(2.5, 100, 5)  # 2.5, 7.5, …, 97.5
BIN_EDGES   = np.arange(0, 101, 5)

IDEAL_IDX   = 10   # bin 50–55%

COLOR_BAR    = "#4C72B0"
COLOR_IDEAL  = "#DD8452"
COLOR_GRID   = "#dddddd"


def verdict_color(std):
    if std <= 3.0:
        return "#2ca02c"   # green
    elif std <= 6.0:
        return "#ff7f0e"   # orange
    else:
        return "#d62728"   # red


def plot_single(label, data, ax):
    counts = np.array(data["bins"], dtype=float)
    pct    = counts / TOTAL * 100

    colors = [COLOR_IDEAL if i == IDEAL_IDX else COLOR_BAR for i in range(20)]

    ax.bar(BIN_CENTERS, pct, width=4.6, color=colors, edgecolor="white", linewidth=0.5)
    ax.axvline(50, color=COLOR_IDEAL, linewidth=1.4, linestyle="--", alpha=0.7, label="Ideal (50%)")

    ax.set_xlim(0, 100)
    ax.set_ylim(0, max(pct) * 1.30 if max(pct) > 0 else 1)
    ax.set_xlabel("Hamming Distance [% of 512 bits]", fontsize=9)
    ax.set_ylabel("Sample Fraction [%]", fontsize=9)
    ax.set_xticks(BIN_EDGES[::2])
    ax.grid(axis="y", color=COLOR_GRID, linewidth=0.6)
    ax.set_axisbelow(True)

    vc = verdict_color(data["std"])
    title = label.replace("\n", " — ")
    ax.set_title(title, fontsize=9.5, fontweight="bold", pad=6)

    stats_txt = f"μ = {data['mean']:.1f}%   σ = {data['std']:.1f}%"
    ax.text(0.97, 0.95, stats_txt,
            transform=ax.transAxes, ha="right", va="top",
            fontsize=8.5, color=vc,
            bbox=dict(boxstyle="round,pad=0.3", facecolor="white",
                      edgecolor=vc, alpha=0.85))

    ideal_patch = mpatches.Patch(color=COLOR_IDEAL, label="50% bin (ideal)")
    bar_patch   = mpatches.Patch(color=COLOR_BAR,   label="Measurement")
    ax.legend(handles=[bar_patch, ideal_patch], fontsize=7.5, loc="upper left")


def make_overview():
    """One figure with all 7 subplots."""
    fig, axes = plt.subplots(2, 4, figsize=(16, 7))
    axes = axes.flatten()

    for idx, (label, data) in enumerate(RESULTS.items()):
        plot_single(label, data, axes[idx])

    # hide the unused 8th subplot
    axes[7].set_visible(False)

    fig.suptitle(
        "Secasy — Diffusion Quality under Isolated Colour Operations\n"
        "(Hamming distance per single-bit flip, N = 25 600 samples per mode)",
        fontsize=11, fontweight="bold", y=1.01
    )
    fig.tight_layout()
    return fig


def make_summary_bar():
    """Summary chart: mean ± stddev for all 7 modes."""
    labels = [l.replace("\n", "\n") for l in RESULTS.keys()]
    means  = [d["mean"] for d in RESULTS.values()]
    stds   = [d["std"]  for d in RESULTS.values()]
    colors = [verdict_color(s) for s in stds]

    x = np.arange(len(labels))

    fig, ax = plt.subplots(figsize=(11, 5))
    bars = ax.bar(x, means, yerr=stds, color=colors, edgecolor="white",
                  capsize=5, ecolor="black", alpha=0.85)

    ax.axhline(50, color=COLOR_IDEAL, linewidth=1.6, linestyle="--", label="Ideal (50%)")
    ax.axhspan(45, 55, color=COLOR_IDEAL, alpha=0.10, label="Ideal range ±5%")

    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=8.5)
    ax.set_ylabel("Mean Hamming Distance [%]", fontsize=10)
    ax.set_ylim(0, 65)
    ax.set_title(
        "Secasy — Mean Diffusion by Colour Operation (μ ± σ)",
        fontsize=11, fontweight="bold"
    )
    ax.grid(axis="y", color=COLOR_GRID, linewidth=0.6)
    ax.set_axisbelow(True)

    green  = mpatches.Patch(color="#2ca02c", label="Strong diffusion (σ ≤ 3%)")
    orange = mpatches.Patch(color="#ff7f0e", label="Moderate diffusion (σ 3–6%)")
    red    = mpatches.Patch(color="#d62728", label="Degraded diffusion (σ > 6%)")
    ideal_line = plt.Line2D([0],[0], color=COLOR_IDEAL, linewidth=1.6,
                             linestyle="--", label="Ideal (50%)")
    ax.legend(handles=[green, orange, red, ideal_line], fontsize=8.5, loc="upper right")

    fig.tight_layout()
    return fig


def save(fig, paths, name):
    for p in paths:
        os.makedirs(p, exist_ok=True)
        out = os.path.join(p, name)
        fig.savefig(out, dpi=150, bbox_inches="tight")
        print(f"  Saved: {out}")


if __name__ == "__main__":
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    img_paths = [
        os.path.join(base, "docs", "en", "img"),
    ]

    print("Generating color operation isolation charts…")

    fig_ov = make_overview()
    save(fig_ov, img_paths, "color_isolation_histograms.png")
    plt.close(fig_ov)

    fig_sum = make_summary_bar()
    save(fig_sum, img_paths, "color_isolation_summary.png")
    plt.close(fig_sum)

    print("Done.")
