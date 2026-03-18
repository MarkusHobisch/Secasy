#!/usr/bin/env python3
"""
plot_cell_divergence_multi.py
=============================
Generates:
  1. One individual plot per flip-byte experiment (5 images).
  2. One combined comparison overlay with all 5 curves.

Reads CSV files from the build directory.

Output: docs/en/img/cell_divergence_flip_byte{N}.png   (individual)
        docs/en/img/cell_divergence_comparison.png      (overlay)

Usage:
    python scripts/plot_cell_divergence_multi.py
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

EXPERIMENTS = [
    {"file": "cell_div_flip_byte0.csv",   "flip_byte": 0,   "color": "#4C72B0", "label": "Flip at byte 0"},
    {"file": "cell_div_flip_byte1.csv",   "flip_byte": 1,   "color": "#DD8452", "label": "Flip at byte 1"},
    {"file": "cell_div_flip_byte32.csv",  "flip_byte": 32,  "color": "#55A868", "label": "Flip at byte 32"},
    {"file": "cell_div_flip_byte64.csv",  "flip_byte": 64,  "color": "#C44E52", "label": "Flip at byte 64"},
    {"file": "cell_div_flip_byte127.csv", "flip_byte": 127, "color": "#8172B3", "label": "Flip at byte 127"},
]


def read_csv(path):
    """Return arrays: byte_idx, mean, mins, maxs, stddev."""
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


def plot_individual(bi, mu, sd, lo, hi, exp):
    """Generate a single-experiment plot (same style as original)."""
    fig, ax = plt.subplots(figsize=(10, 5.5))

    ax.fill_between(bi, lo, hi, alpha=0.12, color=exp["color"], label="Min\u2013Max range")
    ax.fill_between(bi, mu - sd, mu + sd, alpha=0.30, color=exp["color"], label="Mean \u00b1 1\u03c3")
    ax.plot(bi, mu, "-o", color=exp["color"], markersize=3, linewidth=2, label="Mean diff. cells")
    ax.axhline(TOTAL_CELLS, color="#999999", linestyle="--", linewidth=1, label=f"Max ({TOTAL_CELLS} cells)")

    flip = exp["flip_byte"]
    ax.axvline(flip + 1, color="#CC0000", linestyle=":", linewidth=1.2,
               label=f"Flip position (byte {flip})")

    ax.set_xlabel("Input byte position", fontsize=12)
    ax.set_ylabel("Number of differing cells (of 256)", fontsize=12)
    ax.set_title(f"Cell Divergence Growth \u2014 Single-Bit Flip at Byte {flip}\n"
                 f"(N = 200 trials, 128-byte messages, seed 0xDEADBEEFCAFE1234)",
                 fontsize=12, fontweight="bold")
    ax.set_xlim(0.5, 128.5)
    ax.set_ylim(0, TOTAL_CELLS + 20)
    ax.legend(loc="lower right" if flip < 64 else "upper left", fontsize=9)
    ax.grid(True, alpha=0.3)

    # Annotate first nonzero value
    for i, m in enumerate(mu):
        if m > 0:
            ax.annotate(f"{m:.1f}", (bi[i], mu[i]),
                        textcoords="offset points", xytext=(12, -8),
                        fontsize=9, color=exp["color"], fontweight="bold")
            break

    # Annotate final value
    ax.annotate(f"{mu[-1]:.0f}/{TOTAL_CELLS}\n({mu[-1]/TOTAL_CELLS*100:.1f}%)",
                (bi[-1], mu[-1]),
                textcoords="offset points", xytext=(8, -20),
                fontsize=9, color=exp["color"], fontweight="bold")

    # 90% line if reached
    for i, m in enumerate(mu):
        if m >= 0.9 * TOTAL_CELLS:
            ax.annotate(f"90% @ byte {bi[i]}",
                        (bi[i], mu[i]),
                        textcoords="offset points", xytext=(-70, 12),
                        fontsize=9, color=exp["color"], fontweight="bold",
                        arrowprops=dict(arrowstyle="->", color=exp["color"], lw=1))
            break

    fig.tight_layout()
    out = os.path.join(IMG_DIR, f"cell_divergence_flip_byte{flip}.png")
    fig.savefig(out, dpi=180)
    print(f"Saved: {out}")
    plt.close(fig)


def plot_comparison(all_data):
    """Overlay all 5 experiments in one plot."""
    fig, ax = plt.subplots(figsize=(12, 6.5))

    for exp, (bi, mu, sd, lo, hi) in all_data:
        ax.fill_between(bi, mu - sd, mu + sd, alpha=0.15, color=exp["color"])
        ax.plot(bi, mu, "-", color=exp["color"], linewidth=2.2, label=exp["label"])

    ax.axhline(TOTAL_CELLS, color="#999999", linestyle="--", linewidth=1, label=f"Max ({TOTAL_CELLS} cells)")

    # Mark each flip position with a small vertical tick
    for exp, _ in all_data:
        fx = exp["flip_byte"] + 1
        ax.plot(fx, 0, marker="^", color=exp["color"], markersize=8, zorder=5)

    ax.set_xlabel("Input byte position", fontsize=12)
    ax.set_ylabel("Number of differing cells (of 256)", fontsize=12)
    ax.set_title("Cell Divergence Comparison: Bit Flip at Different Byte Positions\n"
                 "(N = 200 trials each, 128-byte messages, seed 0xDEADBEEFCAFE1234)",
                 fontsize=12, fontweight="bold")
    ax.set_xlim(0.5, 128.5)
    ax.set_ylim(-5, TOTAL_CELLS + 20)
    ax.legend(loc="center right", fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.3)

    # Annotate final HDC values
    for exp, (bi, mu, sd, lo, hi) in all_data:
        final = mu[-1]
        yoff = {"0": -18, "1": -6, "32": 6, "64": 14, "127": -18}
        yo = yoff.get(str(exp["flip_byte"]), 0)
        ax.annotate(f"{final:.0f} ({final/TOTAL_CELLS*100:.0f}%)",
                    (bi[-1], final),
                    textcoords="offset points", xytext=(-55, yo),
                    fontsize=9, color=exp["color"], fontweight="bold")

    fig.tight_layout()
    out = os.path.join(IMG_DIR, "cell_divergence_comparison.png")
    fig.savefig(out, dpi=180)
    print(f"Saved: {out}")
    plt.close(fig)


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    all_data = []

    for exp in EXPERIMENTS:
        csv_path = os.path.join(BUILD_DIR, exp["file"])
        if not os.path.exists(csv_path):
            print(f"WARNING: {csv_path} not found, skipping")
            continue
        bi, mu, lo, hi, sd = read_csv(csv_path)
        plot_individual(bi, mu, sd, lo, hi, exp)
        all_data.append((exp, (bi, mu, sd, lo, hi)))

    if all_data:
        plot_comparison(all_data)


if __name__ == "__main__":
    main()
