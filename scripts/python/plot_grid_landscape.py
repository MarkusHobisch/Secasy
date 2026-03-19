#!/usr/bin/env python3
"""
plot_grid_landscape.py — 3D landscape visualization of the Secasy grid state.

Can either read pre-existing CSV files or run Secasy directly with a string
or hex input, then plot the resulting init/processed grid states.

Usage:
    # Direct input (runs Secasy automatically):
    python plot_grid_landscape.py -s "Hello World"
    python plot_grid_landscape.py -x "0x48,0x65,0x6c,0x6c,0x6f"

    # From existing CSV files:
    python plot_grid_landscape.py grid_init.csv grid_processed.csv

    # Options:
    python plot_grid_landscape.py -s "Test" --surface
    python plot_grid_landscape.py -s "Test" -o output.png
"""

import argparse
import csv
import os
import shutil
import subprocess
import sys
import tempfile

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401 — needed for 3d projection

# 6 distinct colours for the 6 operations
COLOR_NAMES = ["ADD", "SUB", "XOR", "RLX", "RRA", "INV"]
COLOR_MAP = {
    0: "#2ecc71",  # ADD  — green  (valley / growth)
    1: "#e74c3c",  # SUB  — red    (erosion)
    2: "#3498db",  # XOR  — blue   (water / ice)
    3: "#f39c12",  # RLX  — orange (rotate-left + XOR)
    4: "#9b59b6",  # RRA  — purple (rotate-right + ADD)
    5: "#ecf0f1",  # INV  — white  (snow cap)
}


def read_grid_csv(path):
    """Parse a Secasy grid CSV into numpy arrays."""
    xs, ys, vals, pidxs, cidxs = [], [], [], [], []
    phase = "unknown"
    with open(path, "r", newline="") as f:
        for line in f:
            if line.startswith("# phase:"):
                phase = line.split(":", 1)[1].strip()
                continue
            if line.startswith("#"):
                continue
            if line.strip().startswith("x,"):
                continue  # header
            parts = line.strip().split(",")
            if len(parts) < 5:
                continue
            xs.append(int(parts[0]))
            ys.append(int(parts[1]))
            vals.append(int(parts[2]))
            pidxs.append(int(parts[3]))
            cidxs.append(int(parts[4]))

    return {
        "x": np.array(xs),
        "y": np.array(ys),
        "value": np.array(vals, dtype=np.float64),
        "primeIndex": np.array(pidxs),
        "colorIndex": np.array(cidxs),
        "phase": phase,
    }


def plot_scatter(ax, grid, title=None):
    """Coloured 3D scatter plot: x, y, z=value, color=colorIndex."""
    colors = [COLOR_MAP.get(c, "#888888") for c in grid["colorIndex"]]

    ax.scatter(
        grid["x"],
        grid["y"],
        grid["value"],
        c=colors,
        s=120,
        edgecolors="black",
        linewidths=0.4,
        depthshade=True,
        alpha=0.9,
    )

    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_zlabel("value (prime)")
    ax.set_title(title or grid["phase"])
    ax.set_xticks(range(0, 16, 2))
    ax.set_yticks(range(0, 16, 2))


def plot_surface(ax, grid, title=None):
    """Coloured 3D surface plot using the grid values."""
    X = grid["x"].reshape(16, 16)
    Y = grid["y"].reshape(16, 16)
    Z = grid["value"].reshape(16, 16)
    C = grid["colorIndex"].reshape(16, 16)

    # Build RGBA face colours from colorIndex
    norm = plt.Normalize(vmin=0, vmax=5)
    listed_cmap = mcolors.ListedColormap(
        [COLOR_MAP[i] for i in range(6)]
    )
    face_colors = listed_cmap(norm(C))

    ax.plot_surface(X, Y, Z, facecolors=face_colors, shade=True, alpha=0.85)
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_zlabel("value (prime)")
    ax.set_title(title or grid["phase"])
    ax.set_xticks(range(0, 16, 2))
    ax.set_yticks(range(0, 16, 2))


def add_legend(fig):
    """Add a colour legend for the 6 operations."""
    from matplotlib.patches import Patch

    legend_elements = [
        Patch(facecolor=COLOR_MAP[i], edgecolor="black", label=COLOR_NAMES[i])
        for i in range(6)
    ]
    fig.legend(
        handles=legend_elements,
        loc="lower center",
        ncol=6,
        fontsize=9,
        frameon=True,
        title="colorIndex (operation)",
    )


def find_secasy_exe():
    """Locate the Secasy executable relative to this script."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    candidates = [
        os.path.join(project_root, "build", "Secasy.exe"),
        os.path.join(project_root, "build", "Secasy"),
        os.path.join(project_root, "cmake-build-debug", "Secasy.exe"),
        os.path.join(project_root, "cmake-build-debug", "Secasy"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return shutil.which("Secasy") or shutil.which("Secasy.exe")


def run_secasy(input_string=None, hex_input=None, file_input=None, work_dir=None):
    """Run Secasy with the given input and return paths to the two CSVs."""
    exe = find_secasy_exe()
    if not exe:
        print("ERROR: Could not find Secasy executable.", file=sys.stderr)
        print("       Build the project first (cmake --build build)", file=sys.stderr)
        sys.exit(1)

    if work_dir is None:
        work_dir = tempfile.mkdtemp(prefix="secasy_grid_")

    cmd = [exe, "-d"]
    if input_string is not None:
        cmd += ["-s", input_string]
    elif hex_input is not None:
        cmd += ["-x", hex_input]
    elif file_input is not None:
        abs_path = os.path.abspath(file_input)
        if not os.path.isfile(abs_path):
            print(f"ERROR: Input file not found: {abs_path}", file=sys.stderr)
            sys.exit(1)
        cmd += ["-f", abs_path]
    else:
        print("ERROR: No input provided.", file=sys.stderr)
        sys.exit(1)

    result = subprocess.run(cmd, cwd=work_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: Secasy failed (exit {result.returncode}):", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)

    init_csv = os.path.join(work_dir, "grid_init.csv")
    proc_csv = os.path.join(work_dir, "grid_processed.csv")

    found = []
    if os.path.isfile(init_csv):
        found.append(init_csv)
    if os.path.isfile(proc_csv):
        found.append(proc_csv)

    if not found:
        print("ERROR: Secasy ran but no grid CSVs were created.", file=sys.stderr)
        print("Secasy output:", result.stdout, file=sys.stderr)
        sys.exit(1)

    return found


def make_title(input_string=None, hex_input=None, file_input=None):
    """Build a descriptive title from the input."""
    if input_string is not None:
        label = input_string if len(input_string) <= 40 else input_string[:37] + "..."
        return rf'Secasy Grid — input: "{label}"'
    if hex_input is not None:
        label = hex_input if len(hex_input) <= 40 else hex_input[:37] + "..."
        return rf"Secasy Grid — input: [{label}]"
    if file_input is not None:
        label = os.path.basename(file_input)
        return rf"Secasy Grid — file: {label}"
    return r"Secasy Grid as $f(x, y, z{=}\mathrm{prime}, c{=}\mathrm{color})$"


def main():
    parser = argparse.ArgumentParser(
        description="3D landscape of Secasy grid state  f(x, y, z=prime, c=color)"
    )

    input_group = parser.add_argument_group("input (choose one)")
    input_group.add_argument(
        "-s", "--string", default=None,
        help='Hash a string, e.g. -s "Hello World"',
    )
    input_group.add_argument(
        "-x", "--hex", default=None,
        help='Hash hex bytes, e.g. -x "0x48,0x65,0x6c,0x6c,0x6f"',
    )
    input_group.add_argument(
        "-f", "--file", default=None,
        help='Hash a file, e.g. -f path/to/input.txt',
    )
    input_group.add_argument(
        "csv_files", nargs="*", default=[],
        help="Pre-existing grid CSV files (alternative to -s/-x)",
    )

    parser.add_argument(
        "--surface", action="store_true", help="Use surface plot instead of scatter"
    )
    parser.add_argument(
        "-o", "--output", default=None, help="Save figure to file instead of showing"
    )
    args = parser.parse_args()

    # Determine CSV files: either run Secasy or use provided files
    if args.string is not None or args.hex is not None or args.file is not None:
        csv_files = run_secasy(
            input_string=args.string, hex_input=args.hex, file_input=args.file
        )
        title = make_title(
            input_string=args.string, hex_input=args.hex, file_input=args.file
        )
    elif args.csv_files:
        csv_files = args.csv_files
        title = r"Secasy Grid as $f(x, y, z{=}\mathrm{prime}, c{=}\mathrm{color})$"
    else:
        parser.error("Provide input via -s, -x, -f, or pass CSV file(s) as arguments")
        return

    n = len(csv_files)
    fig = plt.figure(figsize=(8 * n, 7))
    fig.suptitle(title, fontsize=14)

    plot_fn = plot_surface if args.surface else plot_scatter

    for idx, path in enumerate(csv_files):
        grid = read_grid_csv(path)
        ax = fig.add_subplot(1, n, idx + 1, projection="3d")
        plot_fn(ax, grid)

    add_legend(fig)
    plt.tight_layout(rect=[0, 0.06, 1, 0.95])

    if args.output:
        out_dir = os.path.dirname(args.output)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        fig.savefig(args.output, dpi=150, bbox_inches="tight")
        print(f"Saved to {args.output}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
