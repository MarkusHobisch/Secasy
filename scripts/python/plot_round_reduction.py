"""
Round Reduction Analysis – Visualization & Chart Generation

Modes:
  Single CSV:   python plot_round_reduction.py data.csv
  Compare:      python plot_round_reduction.py --compare 64:f1.csv 128:f2.csv 256:f3.csv 512:f4.csv

Produces individual charts per CSV and combined comparison charts when
multiple files are given.

Requires: matplotlib, pandas (pip install matplotlib pandas)
"""
import sys
import os

try:
    import pandas as pd
    import matplotlib
    matplotlib.use('Agg')  # non-interactive backend for file output
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    import numpy as np
except ImportError as e:
    print(f"Missing dependency: {e}")
    print("Install with: pip install matplotlib pandas numpy")
    sys.exit(1)

# ── Styling helpers ──────────────────────────────────────────────
COLORS = {64: '#1f77b4', 128: '#ff7f0e', 256: '#2ca02c', 512: '#d62728'}
MARKERS = {64: 'o', 128: 's', 256: '^', 512: 'D'}


def style_ax(ax, title, ylabel, ideal=None, warn_below=None, warn_above=None):
    """Apply consistent styling to an axis."""
    ax.set_xscale('log')
    ax.set_xlabel('Rounds (log scale)', fontsize=10)
    ax.set_ylabel(ylabel, fontsize=10)
    ax.set_title(title, fontsize=12, fontweight='bold')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
    ax.tick_params(labelsize=9)

    if ideal is not None:
        ax.axhline(y=ideal, color='green', linestyle='--', alpha=0.6, label=f'Ideal: {ideal}%')
    if warn_below is not None:
        ax.axhline(y=warn_below, color='red', linestyle=':', alpha=0.6, label=f'Warning: <{warn_below}%')
    if warn_above is not None:
        ax.axhline(y=warn_above, color='red', linestyle=':', alpha=0.6, label=f'Warning: >{warn_above}%')


# ── Single-file chart functions ──────────────────────────────────

def load_data(path):
    df = pd.read_csv(path)
    df = df.sort_values('rounds', ascending=True).reset_index(drop=True)
    return df


def plot_single(df, col, title, ylabel, filename, ideal=None, warn_below=None, warn_above=None):
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.plot(df['rounds'], df[col], 'b-o', markersize=5, linewidth=1.5, label='Measured')
    style_ax(ax, title, ylabel, ideal, warn_below, warn_above)
    ax.legend(fontsize=8, loc='best')
    plt.tight_layout()
    fig.savefig(filename, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"  Saved: {filename}")


def plot_combined(df, filename):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Secasy Round Reduction Security Analysis', fontsize=14, fontweight='bold', y=0.98)

    ax = axes[0, 0]
    ax.plot(df['rounds'], df['avalanche_pct'], 'b-o', markersize=4, linewidth=1.5, label='Measured')
    style_ax(ax, 'Avalanche Effect', 'Bit-flip %', ideal=50.0, warn_below=48.0)
    ax.legend(fontsize=8, loc='best')

    ax = axes[0, 1]
    ax.plot(df['rounds'], df['seq_corr_pct'], 'g-s', markersize=4, linewidth=1.5, label='Measured')
    style_ax(ax, 'Sequential Correlation', 'Hamming distance %', ideal=50.0, warn_below=45.0)
    ax.legend(fontsize=8, loc='best')

    ax = axes[1, 0]
    ax.plot(df['rounds'], df['min_hamming_pct'], 'r-^', markersize=4, linewidth=1.5, label='Measured')
    style_ax(ax, 'Min Pairwise Hamming Distance', 'Min distance %', warn_below=20.0)
    ax.legend(fontsize=8, loc='best')

    ax = axes[1, 1]
    ax.plot(df['rounds'], df['bit_bias_pct'], 'm-D', markersize=4, linewidth=1.5, label='Measured')
    style_ax(ax, 'Max Positional Bit Bias', 'Deviation from 50%', warn_above=10.0)
    sigma = 1.0 / (2.0 * np.sqrt(500)) * 100
    expected_max = 3.5 * sigma
    ax.axhline(y=expected_max, color='orange', linestyle='-.', alpha=0.6, label=f'Expected noise floor (~{expected_max:.1f}%)')
    ax.legend(fontsize=8, loc='best')

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(filename, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"  Saved: {filename}")


def generate_single_charts(csv_path):
    """Generate charts for a single CSV file."""
    df = load_data(csv_path)
    print(f"Loaded {len(df)} data points from {csv_path}\n")

    out_dir = os.path.dirname(csv_path) or '.'
    print("Generating charts...")
    plot_single(df, 'avalanche_pct', 'Avalanche Effect vs. Rounds',
                'Bit-flip %', os.path.join(out_dir, 'chart_avalanche.png'),
                ideal=50.0, warn_below=48.0)
    plot_single(df, 'seq_corr_pct', 'Sequential Correlation vs. Rounds',
                'Hamming distance %', os.path.join(out_dir, 'chart_seq_correlation.png'),
                ideal=50.0, warn_below=45.0)
    plot_single(df, 'min_hamming_pct', 'Min Pairwise Hamming Distance vs. Rounds',
                'Min distance %', os.path.join(out_dir, 'chart_min_hamming.png'),
                warn_below=20.0)
    plot_single(df, 'bit_bias_pct', 'Max Positional Bit Bias vs. Rounds',
                'Deviation from 50%', os.path.join(out_dir, 'chart_bit_bias.png'),
                warn_above=10.0)
    plot_combined(df, os.path.join(out_dir, 'chart_round_reduction_overview.png'))
    print("\nDone. All charts saved.")


# ── Comparison chart functions ───────────────────────────────────

def plot_compare_metric(datasets, col, title, ylabel, filename,
                        ideal=None, warn_below=None, warn_above=None):
    """Plot a single metric across multiple hash sizes."""
    fig, ax = plt.subplots(figsize=(11, 5.5))

    for bits, df in sorted(datasets.items()):
        c = COLORS.get(bits, 'gray')
        m = MARKERS.get(bits, 'x')
        ax.plot(df['rounds'], df[col], f'-{m}', color=c, markersize=5,
                linewidth=1.5, label=f'{bits}-bit')

    style_ax(ax, title, ylabel, ideal, warn_below, warn_above)
    ax.legend(fontsize=9, loc='best')
    plt.tight_layout()
    fig.savefig(filename, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"  Saved: {filename}")


def plot_compare_overview(datasets, filename):
    """2x2 comparison overview."""
    fig, axes = plt.subplots(2, 2, figsize=(15, 11))
    fig.suptitle('Secasy Round Reduction — Hash Size Comparison',
                 fontsize=14, fontweight='bold', y=0.98)

    metrics = [
        (axes[0, 0], 'avalanche_pct', 'Avalanche Effect', 'Bit-flip %',
         dict(ideal=50.0, warn_below=48.0)),
        (axes[0, 1], 'seq_corr_pct', 'Sequential Correlation', 'Hamming distance %',
         dict(ideal=50.0, warn_below=45.0)),
        (axes[1, 0], 'min_hamming_pct', 'Min Pairwise Hamming Distance', 'Min distance %',
         dict(warn_below=20.0)),
        (axes[1, 1], 'bit_bias_pct', 'Max Positional Bit Bias', 'Deviation from 50%',
         dict(warn_above=10.0)),
    ]

    for ax, col, title, ylabel, kwargs in metrics:
        for bits, df in sorted(datasets.items()):
            c = COLORS.get(bits, 'gray')
            m = MARKERS.get(bits, 'x')
            ax.plot(df['rounds'], df[col], f'-{m}', color=c, markersize=4,
                    linewidth=1.5, label=f'{bits}-bit')
        style_ax(ax, title, ylabel, **kwargs)
        ax.legend(fontsize=8, loc='best')

    # Noise floor on bit bias
    sigma = 1.0 / (2.0 * np.sqrt(500)) * 100
    expected_max = 3.5 * sigma
    axes[1, 1].axhline(y=expected_max, color='orange', linestyle='-.',
                        alpha=0.6, label=f'Noise floor (~{expected_max:.1f}%)')
    axes[1, 1].legend(fontsize=8, loc='best')

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(filename, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"  Saved: {filename}")


def generate_compare_charts(label_paths, out_dir):
    """Generate comparison charts from multiple label:csv pairs."""
    datasets = {}
    for lp in label_paths:
        if ':' not in lp:
            print(f"Expected format 'BITS:path.csv', got: {lp}")
            sys.exit(1)
        label, path = lp.split(':', 1)
        bits = int(label)
        if not os.path.exists(path):
            print(f"CSV not found: {path}")
            sys.exit(1)
        datasets[bits] = load_data(path)
        print(f"Loaded {len(datasets[bits])} data points for {bits}-bit from {path}")

    print(f"\nGenerating comparison charts for {sorted(datasets.keys())} ...")

    plot_compare_metric(datasets, 'avalanche_pct',
                        'Avalanche Effect — Hash Size Comparison', 'Bit-flip %',
                        os.path.join(out_dir, 'compare_avalanche.png'),
                        ideal=50.0, warn_below=48.0)

    plot_compare_metric(datasets, 'seq_corr_pct',
                        'Sequential Correlation — Hash Size Comparison', 'Hamming distance %',
                        os.path.join(out_dir, 'compare_seq_correlation.png'),
                        ideal=50.0, warn_below=45.0)

    plot_compare_metric(datasets, 'min_hamming_pct',
                        'Min Pairwise Hamming — Hash Size Comparison', 'Min distance %',
                        os.path.join(out_dir, 'compare_min_hamming.png'),
                        warn_below=20.0)

    plot_compare_metric(datasets, 'bit_bias_pct',
                        'Max Bit Bias — Hash Size Comparison', 'Deviation from 50%',
                        os.path.join(out_dir, 'compare_bit_bias.png'),
                        warn_above=10.0)

    plot_compare_overview(datasets,
                          os.path.join(out_dir, 'compare_round_reduction_overview.png'))

    print("\nDone. All comparison charts saved.")


# ── Entry point ──────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print("Usage:")
        print("  Single:  python plot_round_reduction.py data.csv")
        print("  Compare: python plot_round_reduction.py --compare 64:f1.csv 128:f2.csv ...")
        sys.exit(1)

    if sys.argv[1] == '--compare':
        if len(sys.argv) < 3:
            print("Need at least one BITS:path.csv argument")
            sys.exit(1)
        # Use directory of first CSV as output dir
        first_path = sys.argv[2].split(':', 1)[1]
        out_dir = os.path.dirname(first_path) or '.'
        generate_compare_charts(sys.argv[2:], out_dir)
    else:
        generate_single_charts(sys.argv[1])


if __name__ == '__main__':
    main()
