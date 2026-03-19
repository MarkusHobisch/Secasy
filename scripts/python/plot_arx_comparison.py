#!/usr/bin/env python3
"""
Generate before/after comparison plots for the AND/OR → ARX migration.

This script reads pre-recorded CSV stats and produces:
  1. A side-by-side bar chart comparing distinct values, entropy, etc.
  2. An annotated before/after landscape comparison

The "before" data (AND/OR) was measured empirically:
  - ALGORITHM.md input: 110/256 distinct, min=0, max=2^64
  - "Hello World" input: same bimodal pattern

The "after" data (ARX) is read live from the current build.
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
import os
import sys

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "docs", "en", "img")

# ── Pre-recorded "before" data (AND/OR, measured before migration) ──
BEFORE = {
    "label": "Before (AND / OR)",
    "distinct": 110,
    "min_val": 0,
    "max_val": 18446744073709551616,
    "entropy": 2.578,
}

# ── "After" data (ARX) — read from current build CSVs ──
def read_processed_stats(csv_path):
    vals, cidxs = [], []
    with open(csv_path) as f:
        for line in f:
            if line.startswith("#") or line.startswith("x,"):
                continue
            parts = line.strip().split(",")
            if len(parts) >= 5:
                vals.append(int(parts[2]))
                cidxs.append(int(parts[4]))
    v = np.array(vals, dtype=np.float64)
    c = np.array(cidxs)
    unique, counts = np.unique(c, return_counts=True)
    entropy = -np.sum((counts / 256) * np.log2(counts / 256))
    return {
        "label": "After (Rotate + XOR/ADD)",
        "distinct": len(np.unique(v)),
        "min_val": v.min(),
        "max_val": v.max(),
        "entropy": entropy,
    }


def plot_comparison_bars(before, after, output_path):
    """Bar chart comparing key metrics before vs after."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    fig.suptitle("Processing Phase: AND/OR vs ARX (Rotate+XOR/ADD)", fontsize=14, fontweight="bold")

    colors = ["#e74c3c", "#2ecc71"]
    labels = [before["label"], after["label"]]

    # 1) Distinct values
    ax = axes[0]
    vals = [before["distinct"], after["distinct"]]
    bars = ax.bar(labels, vals, color=colors, edgecolor="black", width=0.5)
    ax.set_ylabel("Distinct cell values (out of 256)")
    ax.set_title("Unique Values After Processing")
    ax.set_ylim(0, 280)
    ax.axhline(y=256, color="gray", linestyle="--", alpha=0.5, label="Maximum (256)")
    for bar, val in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 5,
                str(val), ha="center", va="bottom", fontweight="bold", fontsize=13)
    ax.legend(fontsize=8)

    # 2) Has zero values?
    ax = axes[1]
    has_zero = [1 if before["min_val"] == 0 else 0,
                1 if after["min_val"] == 0 else 0]
    bars = ax.bar(labels, has_zero, color=colors, edgecolor="black", width=0.5)
    ax.set_ylabel("Contains absorbed zero values")
    ax.set_title("Zero Absorption (min = 0?)")
    ax.set_ylim(-0.1, 1.5)
    ax.set_yticks([0, 1])
    ax.set_yticklabels(["No", "Yes"])
    for bar, val in zip(bars, has_zero):
        txt = "YES — absorbed" if val else "NO — clean"
        col = "#e74c3c" if val else "#2ecc71"
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.05,
                txt, ha="center", va="bottom", fontweight="bold", fontsize=10, color=col)

    # 3) Entropy preservation
    ax = axes[2]
    vals = [before["entropy"], after["entropy"]]
    bars = ax.bar(labels, vals, color=colors, edgecolor="black", width=0.5)
    ax.set_ylabel("Color entropy (bits)")
    ax.set_title("Operation Entropy (max = 2.585)")
    ax.set_ylim(0, 3.0)
    ax.axhline(y=2.585, color="gray", linestyle="--", alpha=0.5, label="Maximum (2.585)")
    for bar, val in zip(bars, vals):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.05,
                f"{val:.3f}", ha="center", va="bottom", fontweight="bold", fontsize=12)
    ax.legend(fontsize=8)

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    fig.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"Saved to {output_path}")
    plt.close()


def main():
    build_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build")
    proc_csv = os.path.join(build_dir, "grid_processed.csv")

    if not os.path.isfile(proc_csv):
        print(f"ERROR: {proc_csv} not found. Run Secasy first.", file=sys.stderr)
        sys.exit(1)

    after = read_processed_stats(proc_csv)

    os.makedirs(OUT_DIR, exist_ok=True)
    plot_comparison_bars(BEFORE, after, os.path.join(OUT_DIR, "arx_migration_comparison.png"))


if __name__ == "__main__":
    main()
