#!/usr/bin/env python3
"""
plot_field_size_sweep.py
========================
Reads  build/field_size_results.csv  (produced by SecasyFieldSizeSweep)
and generates two chart files:

  docs/en/img/field_size_histograms.png
      — 5 Hamming-distance histograms, one per field size

  docs/en/img/field_size_summary.png
      — µ ± σ bar chart  +  nibble-symmetry-bias bar chart

Usage (from repo root):
    cd build && cmake --build . --target SecasyFieldSizeSweep
    ./SecasyFieldSizeSweep          # or SecasyFieldSizeSweep.exe on Windows
    cd ..
    python scripts/plot_field_size_sweep.py
"""

import os
import sys
import csv
import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np

# ── Per-field-size colour scheme ─────────────────────────────────────
FS_COLORS = {
    4:  "#e41a1c",   # red    — smallest grid
    8:  "#ff7f00",   # orange
    16: "#4daf4a",   # green  — baseline (16×16)
    32: "#377eb8",   # blue
    64: "#984ea3",   # purple — largest grid
}

COLOR_IDEAL = "#DD8452"
COLOR_GRID  = "#dddddd"

BIN_CENTERS = np.arange(2.5, 100.0, 5.0)   # 2.5, 7.5, …, 97.5
BIN_EDGES   = np.arange(0, 101, 5)
IDEAL_BIN   = 10   # 0-indexed bin covering 50–55 %


def verdict_color(std):
    """Traffic-light colour based on standard deviation."""
    if std <= 2.5:
        return "#2ca02c"   # green  — strong diffusion
    elif std <= 4.5:
        return "#ff7f0e"   # orange — moderate diffusion
    else:
        return "#d62728"   # red    — degraded diffusion


# ── Read CSV produced by SecasyFieldSizeSweep ────────────────────────

def read_csv(path):
    results = []
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            fs = int(row["fieldsize"])
            results.append({
                "fs":       fs,
                "cells":    int(row["cells"]),
                "mean":     float(row["mean"]),
                "std":      float(row["std"]),
                "min":      float(row["min"]),
                "max":      float(row["max"]),
                "nib_bias": float(row["nib_bias_pct"]),
                "bins":     [int(row[f"bin{i}"]) for i in range(20)],
                "total":    sum(int(row[f"bin{i}"]) for i in range(20)),
            })
    return sorted(results, key=lambda r: r["fs"])


# ── Figure 1: histogram grid ─────────────────────────────────────────

def _plot_single_hist(r, ax):
    counts = np.array(r["bins"], dtype=float)
    total  = r["total"] if r["total"] > 0 else 1
    pct    = counts / total * 100.0

    clr    = FS_COLORS.get(r["fs"], "#888888")
    colors = [COLOR_IDEAL if i == IDEAL_BIN else clr for i in range(20)]

    ax.bar(BIN_CENTERS, pct, width=4.6, color=colors,
           edgecolor="white", linewidth=0.5)
    ax.axvline(50, color=COLOR_IDEAL, linewidth=1.4,
               linestyle="--", alpha=0.7, label="Ideal (50 %)")

    ax.set_xlim(0, 100)
    ax.set_ylim(0, max(pct) * 1.38 if max(pct) > 0 else 1)
    ax.set_xlabel("Hamming Distance [% of 512 bits]", fontsize=9)
    ax.set_ylabel("Sample Fraction [%]", fontsize=9)
    ax.set_xticks(BIN_EDGES[::2])
    ax.grid(axis="y", color=COLOR_GRID, linewidth=0.6)
    ax.set_axisbelow(True)

    is_baseline = (r["fs"] == 16)
    title_suffix = "  ★ baseline" if is_baseline else ""
    ax.set_title(
        f"{r['fs']}×{r['fs']} grid  ({r['cells']} cells){title_suffix}",
        fontsize=9.5, fontweight="bold", pad=6
    )

    vc = verdict_color(r["std"])
    ax.text(
        0.97, 0.95,
        f"µ = {r['mean']:.1f}%   σ = {r['std']:.1f}%",
        transform=ax.transAxes, ha="right", va="top",
        fontsize=8.5, color=vc,
        bbox=dict(boxstyle="round,pad=0.3", facecolor="white",
                  edgecolor=vc, alpha=0.85)
    )

    ideal_patch = mpatches.Patch(color=COLOR_IDEAL, label="50% bin (ideal)")
    bar_patch   = mpatches.Patch(color=clr,         label="Measurement")
    ax.legend(handles=[bar_patch, ideal_patch], fontsize=7, loc="upper left")


def make_histogram_figure(results):
    n     = len(results)
    ncols = 3
    nrows = math.ceil(n / ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(15, 4.8 * nrows))
    axes = axes.flatten()

    for idx, r in enumerate(results):
        _plot_single_hist(r, axes[idx])

    for j in range(n, len(axes)):
        axes[j].set_visible(False)

    fig.suptitle(
        "Secasy — Diffusion Quality across Field Sizes\n"
        "(Hamming distance per single-bit flip, N ≈ 25 600 samples per size)",
        fontsize=11, fontweight="bold", y=1.01
    )
    fig.tight_layout()
    return fig


# ── Figure 2: summary — µ ± σ  +  nibble symmetry bias ──────────────

def make_summary_figure(results):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.5))

    labels   = [f"{r['fs']}×{r['fs']}" for r in results]
    means    = [r["mean"]    for r in results]
    stds     = [r["std"]     for r in results]
    biases   = [r["nib_bias"] for r in results]
    bar_clrs = [FS_COLORS.get(r["fs"], "#888") for r in results]
    vcolors  = [verdict_color(r["std"]) for r in results]
    x        = np.arange(len(labels))

    # ── Left subplot: µ ± σ ─────────────────────────────────────────
    bars = ax1.bar(x, means, yerr=stds,
                   color=bar_clrs, edgecolor="white",
                   capsize=7, ecolor="#333333",
                   alpha=0.88, width=0.58)

    ax1.axhline(50.0, color=COLOR_IDEAL, linewidth=1.8,
                linestyle="--", label="Ideal (50 %)")
    ax1.axhspan(45.0, 55.0, color=COLOR_IDEAL, alpha=0.09,
                label="Ideal ±5 % band")

    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, fontsize=9.5)
    ax1.set_ylabel("Mean Hamming Distance [%]", fontsize=10)
    ax1.set_ylim(0.0, 72.0)
    ax1.set_title("Mean Diffusion µ ± σ by Field Size",
                  fontsize=10.5, fontweight="bold")
    ax1.grid(axis="y", color=COLOR_GRID, linewidth=0.6)
    ax1.set_axisbelow(True)

    # annotate σ above each bar
    for bar, std, vc in zip(bars, stds, vcolors):
        ax1.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height() + std + 0.9,
            f"σ={std:.1f}%",
            ha="center", va="bottom", fontsize=8.0,
            color=vc, fontweight="bold"
        )

    # mark baseline (16×16)
    for i, r in enumerate(results):
        if r["fs"] == 16:
            ax1.text(
                x[i], 1.5, "baseline",
                ha="center", va="bottom", fontsize=7.5,
                color="white", fontweight="bold",
                bbox=dict(boxstyle="round,pad=0.2",
                          facecolor=FS_COLORS[16], edgecolor="none")
            )

    green_p  = mpatches.Patch(color="#2ca02c", label="Strong diffusion (σ ≤ 2.5 %)")
    orange_p = mpatches.Patch(color="#ff7f0e", label="Moderate diffusion (σ ≤ 4.5 %)")
    red_p    = mpatches.Patch(color="#d62728", label="Degraded diffusion (σ > 4.5 %)")
    ideal_l  = plt.Line2D([0], [0], color=COLOR_IDEAL, linewidth=1.8,
                           linestyle="--", label="Ideal (50 %)")
    ax1.legend(handles=[green_p, orange_p, red_p, ideal_l],
               fontsize=8, loc="upper right")

    # ── Right subplot: nibble symmetry bias ─────────────────────────
    bias_colors = [verdict_color(b) for b in biases]
    bars2 = ax2.bar(x, biases,
                    color=bias_colors, edgecolor="white",
                    alpha=0.88, width=0.58)

    ax2.axhline(0.0, color="black", linewidth=0.8)
    ax2.set_xticks(x)
    ax2.set_xticklabels(labels, fontsize=9.5)
    ax2.set_ylabel(
        "Max nibble flip-rate deviation from 50 % [pp]",
        fontsize=9.5
    )
    ax2.set_title(
        "Output Symmetry — Nibble Bias per Field Size",
        fontsize=10.5, fontweight="bold"
    )
    ax2.grid(axis="y", color=COLOR_GRID, linewidth=0.6)
    ax2.set_axisbelow(True)
    ax2.annotate(
        "Lower = more uniform output sensitivity",
        xy=(0.98, 0.97), xycoords="axes fraction",
        ha="right", va="top", fontsize=8.5,
        style="italic", color="#555555"
    )

    # value labels on bars
    for bar, val in zip(bars2, biases):
        ax2.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height() + 0.02,
            f"{val:.2f}",
            ha="center", va="bottom", fontsize=8.0, fontweight="bold"
        )

    fig.suptitle(
        "Secasy — Field-Size Sweep: Diffusion Quality & Output Symmetry",
        fontsize=12, fontweight="bold"
    )
    fig.tight_layout()
    return fig


# ── Save helper ──────────────────────────────────────────────────────

def save(fig, paths, name):
    for p in paths:
        os.makedirs(p, exist_ok=True)
        out = os.path.join(p, name)
        fig.savefig(out, dpi=150, bbox_inches="tight")
        print(f"  Saved: {out}")


# ── Entry point ──────────────────────────────────────────────────────

def main():
    base     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    csv_path = os.path.join(base, "build", "field_size_results.csv")

    if not os.path.isfile(csv_path):
        print(f"ERROR: CSV not found at:\n  {csv_path}\n")
        print("Run SecasyFieldSizeSweep first:")
        print("  cd build")
        print("  cmake --build . --target SecasyFieldSizeSweep")
        print("  ./SecasyFieldSizeSweep   (or SecasyFieldSizeSweep.exe)")
        sys.exit(1)

    print(f"Reading: {csv_path}")
    results = read_csv(csv_path)
    print(f"  Field sizes found: {[r['fs'] for r in results]}")

    img_paths = [
        os.path.join(base, "docs", "en", "img"),
    ]

    print("Generating histogram grid...")
    fig1 = make_histogram_figure(results)
    save(fig1, img_paths, "field_size_histograms.png")
    plt.close(fig1)

    print("Generating summary chart...")
    fig2 = make_summary_figure(results)
    save(fig2, img_paths, "field_size_summary.png")
    plt.close(fig2)

    print("Done.")


if __name__ == "__main__":
    main()
