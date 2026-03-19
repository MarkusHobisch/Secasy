#!/usr/bin/env python3
"""
plot_cell_divergence_seeds.py
=============================
Overlays the cell-divergence curves from multiple RNG seeds (all with
flip at byte 0) to demonstrate seed-independence of the results.

Reads: build/cell_div_seed{1..5}.csv
Output: docs/en/img/cell_divergence_seeds.png

Usage:
    python scripts/python/plot_cell_divergence_seeds.py
"""

import os
import csv
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

TOTAL_CELLS = 256
BUILD_DIR   = os.path.join(os.path.dirname(__file__), "..", "build")
IMG_DIR     = os.path.join(os.path.dirname(__file__), "..", "docs", "en", "img")

SEEDS = [
    {"file": "cell_div_seed1.csv", "seed": "0xDEADBEEFCAFE1234", "color": "#4C72B0"},
    {"file": "cell_div_seed2.csv", "seed": "0x123456789ABCDEF0", "color": "#DD8452"},
    {"file": "cell_div_seed3.csv", "seed": "0xAAAAAAAAAAAAAAAA", "color": "#55A868"},
    {"file": "cell_div_seed4.csv", "seed": "0x5555555555555555", "color": "#C44E52"},
    {"file": "cell_div_seed5.csv", "seed": "0xFEDCBA9876543210", "color": "#8172B3"},
]


def read_csv(path):
    byte_idx, mean, mins, maxs, stddev = [], [], [], [], []
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            byte_idx.append(int(row["byte_index"]))
            mean.append(float(row["mean_diff_cells"]))
            mins.append(int(row["min"]))
            maxs.append(int(row["max"]))
            stddev.append(float(row["stddev"]))
    return (np.array(byte_idx), np.array(mean),
            np.array(mins, dtype=float), np.array(maxs, dtype=float),
            np.array(stddev))


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    fig, ax = plt.subplots(figsize=(12, 6.5))

    all_means = []
    for s in SEEDS:
        csv_path = os.path.join(BUILD_DIR, s["file"])
        if not os.path.exists(csv_path):
            print(f"WARNING: {csv_path} not found, skipping")
            continue
        bi, mu, lo, hi, sd = read_csv(csv_path)
        all_means.append(mu)
        ax.plot(bi, mu, "-", color=s["color"], linewidth=1.8, alpha=0.8,
                label=f"Seed {s['seed']}")

    # Compute and plot grand mean across all seeds
    if all_means:
        grand = np.mean(all_means, axis=0)
        ax.plot(bi, grand, "k--", linewidth=2.5, label="Grand mean (5 seeds)")

    ax.axhline(TOTAL_CELLS, color="#999999", linestyle="--", linewidth=1,
               label=f"Max ({TOTAL_CELLS} cells)")

    ax.set_xlabel("Input byte position", fontsize=12)
    ax.set_ylabel("Number of differing cells (of 256)", fontsize=12)
    ax.set_title("Cross-Seed Reproducibility: HDC(n) with Bit Flip at Byte 0\n"
                 "(N = 200 trials per seed, 5 independent seeds, 1,000 total pairs)",
                 fontsize=12, fontweight="bold")
    ax.set_xlim(0.5, 128.5)
    ax.set_ylim(0, TOTAL_CELLS + 20)
    ax.legend(loc="lower right", fontsize=9, framealpha=0.9)
    ax.grid(True, alpha=0.3)

    # Annotate spread at key points
    for idx_byte in [0, 86]:  # byte 1 and byte 87 (0-indexed in array)
        vals = [m[idx_byte] for m in all_means]
        lo_v, hi_v = min(vals), max(vals)
        mid = np.mean(vals)
        spread = hi_v - lo_v
        bx = bi[idx_byte]
        ax.annotate(f"Spread: {spread:.2f}\n({lo_v:.1f}\u2013{hi_v:.1f})",
                    (bx, mid),
                    textcoords="offset points",
                    xytext=(20, 15 if idx_byte == 0 else -25),
                    fontsize=9, fontweight="bold", color="#333333",
                    arrowprops=dict(arrowstyle="->", color="#333333", lw=1))

    fig.tight_layout()
    out = os.path.join(IMG_DIR, "cell_divergence_seeds.png")
    fig.savefig(out, dpi=180)
    print(f"Saved: {out}")

    # Print summary table
    print("\nCross-seed summary (flip at byte 0, N=200 each):")
    print(f"{'Seed':<26} {'HDC(1)':>8} {'HDC(87)':>8} {'HDC(128)':>8}")
    for s, mu in zip(SEEDS, all_means):
        print(f"{s['seed']:<26} {mu[0]:>8.2f} {mu[86]:>8.2f} {mu[127]:>8.2f}")
    grand = np.mean(all_means, axis=0)
    std_across = np.std(all_means, axis=0)
    print(f"{'Grand mean':<26} {grand[0]:>8.2f} {grand[86]:>8.2f} {grand[127]:>8.2f}")
    print(f"{'Std across seeds':<26} {std_across[0]:>8.2f} {std_across[86]:>8.2f} {std_across[127]:>8.2f}")


if __name__ == "__main__":
    main()
