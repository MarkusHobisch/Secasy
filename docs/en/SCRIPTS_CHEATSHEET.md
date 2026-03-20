# Secasy — Scripts Cheat Sheet

> Internal quick-reference. All commands run from the **repository root** unless noted otherwise.
> Build prerequisites: `cmake -S . -B build -G "MinGW Makefiles"` → `cmake --build build --target <TARGET>`

---

## Shell Scripts — `scripts/shell/`

### `generate_hashes.sh`

Generates N random hash outputs for external statistical tools (NIST STS, Dieharder).

| Param | Default | Meaning |
|---|---|---|
| `$1` | 1 000 000 | number of hashes |
| `$2` | 64 | input length (bytes) |
| `$3` | 100 000 | number of rounds |

```bash
# 1M hashes, 64-byte inputs (defaults)
bash scripts/shell/generate_hashes.sh

# 500K hashes, 128-byte inputs, 50K rounds
bash scripts/shell/generate_hashes.sh 500000 128 50000
```

---

### `run_security_tests.sh`

Full automated security test battery — builds if necessary, runs tests, prints pass/fail per test, saves logs to `../results/`.

| Mode | Duration | What it runs |
|---|---|---|
| `quick` | ~5 min | Fast sanity checks |
| `full` | ~30–60 min | Comprehensive suite |

```bash
bash scripts/shell/run_security_tests.sh quick
bash scripts/shell/run_security_tests.sh full
```

---

### `run_extended_avalanche_wsl.sh`

Builds `SecasyAvalanche` under WSL and runs a fixed set of extended (`-X`) avalanche cases covering different message sizes, seed values, and multi-bit flip depths.

```bash
# From repo root inside WSL:
bash scripts/shell/run_extended_avalanche_wsl.sh

# Override build type or dir:
BUILD_TYPE=Debug BUILD_DIR=build-dbg bash scripts/shell/run_extended_avalanche_wsl.sh
```

Environment variables: `BUILD_TYPE` (default `Release`), `BUILD_DIR` (default `build-wsl`), `GENERATOR`, `JOBS`.

---

## Python Scripts — `scripts/python/`

All scripts write images to `docs/en/img/`. Run from the repo root.

---

### `plot_grid_landscape.py`

3-D scatter (or surface) plot of the 16×16 grid state after hashing an input.
Can run Secasy directly or read pre-existing CSV files.

```bash
# Hash a string and plot
python scripts/python/plot_grid_landscape.py -s "Hello World"

# Hash hex bytes
python scripts/python/plot_grid_landscape.py -x "0x48,0x65,0x6c,0x6c,0x6f"

# From existing build CSVs
python scripts/python/plot_grid_landscape.py build/grid_init.csv build/grid_processed.csv

# Surface mode, custom output file
python scripts/python/plot_grid_landscape.py -s "Test" --surface -o docs/en/img/my_plot.png
```

Flags: `-s STRING`, `-x HEX`, `--surface` (surface instead of scatter), `-o OUTPUT`.

---

### `plot_arx_comparison.py`

Before/after bar charts comparing the AND/OR vs ARX (Rotate+XOR/ADD) Processing Phase.
Reads `build/grid_processed.csv` for the "after" data; "before" data is hard-coded from the pre-migration measurement.

```bash
# Build Secasy first, then run it to produce grid_processed.csv:
./build/Secasy.exe -s "ALGORITHM.md input"
python scripts/python/plot_arx_comparison.py
# → docs/en/img/arx_migration_comparison.png
```

---

### `plot_round_reduction.py`

Reads round-reduction CSV files and produces metric charts (Hamming distance, entropy, SAC rate) per output bit-width, plus a combined comparison across bit-widths.

```bash
# Single file
python scripts/python/plot_round_reduction.py build/round_reduction_512bit.csv

# Compare all four bit-widths
python scripts/python/plot_round_reduction.py --compare \
  64:build/round_reduction_64bit.csv \
  128:build/round_reduction_128bit.csv \
  256:build/round_reduction_256bit.csv \
  512:build/round_reduction_512bit.csv
# → docs/en/img/round_reduction_*.png
```

---

### `plot_field_size_sweep.py`

Reads `build/field_size_results.csv` (produced by `SecasyFieldSizeSweep`) and generates:
- Hamming-distance histograms per field size (4×4 … 64×64)
- µ ± σ summary + nibble-symmetry-bias bar chart

```bash
cmake --build build --target SecasyFieldSizeSweep
./build/SecasyFieldSizeSweep.exe
python scripts/python/plot_field_size_sweep.py
# → docs/en/img/field_size_histograms.png
# → docs/en/img/field_size_summary.png
```

---

### `plot_cell_divergence_multi.py`

Reads `build/cell_div_flip_byte{0,1,32,64,127}.csv` and produces:
- One individual plot per flip position (5 images)
- One combined overlay of all 5 curves

```bash
cmake --build build --target SecasyCellDivergence && ./build/SecasyCellDivergence.exe
python scripts/python/plot_cell_divergence_multi.py
# → docs/en/img/cell_divergence_flip_byte*.png
# → docs/en/img/cell_divergence_comparison.png
```

---

### `plot_cell_divergence_seeds.py`

Same as above but overlays curves from 5 different RNG seeds (all flipping at byte 0) to demonstrate seed-independence.

```bash
# Requires build/cell_div_seed{1..5}.csv (produced by SecasyCellDivergence)
python scripts/python/plot_cell_divergence_seeds.py
# → docs/en/img/cell_divergence_seeds.png
```

---

### `plot_color_isolation.py`

Bar-chart histograms for the colour-operation isolation diffusion analysis.
Data is **hard-coded** from a measured run of `SecasyColorIsolation` — no CSV input needed.

```bash
python scripts/python/plot_color_isolation.py
# → docs/en/img/color_isolation_*.png
```

---

### `validate_sac.py`

Reads a SAC matrix CSV and checks what fraction of cells fall in `[0.48, 0.52]`.
Exit code 0 = pass, 1 = fail.

```bash
python scripts/python/validate_sac.py build/sac_production.csv
python scripts/python/validate_sac.py build/sac_test.csv --threshold 0.90 --lower 0.47 --upper 0.53
```

Flags: `--threshold` (default 0.95), `--lower` (default 0.48), `--upper` (default 0.52).

---

### `validate_collisions.py`

Parses a collision-test log and checks each truncation level against the birthday bound (Poisson ± Nσ).
Exit code 0 = pass, 1 = fail.

```bash
python scripts/python/validate_collisions.py build/coll_out.txt
python scripts/python/validate_collisions.py build/coll_out.txt --sigma 2.0
```

Flags: `--sigma` (default 3.0).

---

### `scan_reduced_params.py`

Sweeps reduced round counts, prime-index values, and output bit-widths.
For each combination it runs `SecasyDifferential` and `SecasyExtendedSecurity`, flags weak configurations, and optionally confirms failures with higher trial counts.

```bash
python scripts/python/scan_reduced_params.py

# Custom sweep
python scripts/python/scan_reduced_params.py \
  --rounds 50,100,500 \
  --nbits 64,128,256 \
  --prime-index 100,200 \
  --trials 300 \
  --report-dir build/scan_report
```

Key flags:

| Flag | Default | Meaning |
|---|---|---|
| `--build` | `build` | directory with compiled executables |
| `--rounds` | `50,100,200,500` | round counts to test |
| `--nbits` | `64,128,256` | output bit-widths |
| `--prime-index` | `100,200` | `maximumPrimeIndex` values |
| `--trials` | `200` | extended-security trial count |
| `--confirm-trials` | `500` | trials for confirming a failure |
| `--no-confirm` | off | skip confirmation loop |
| `--report-dir` | *(none)* | save flagged config outputs here |

---

## C Scripts — `scripts/c/`

These are standalone attack-simulation programs compiled separately (not in the main CMake tree).

### `collision_finder.c`

Birthday-attack and algebraic collision search. Hashes 1 000 000 random 64-byte inputs, stores them in a hash table, and reports any exact collisions or near-collisions.

```bash
gcc -O2 -Iinclude scripts/c/collision_finder.c src/InitializationPhase.c \
    src/ProcessingPhase.c src/Calculations.c src/util.c src/SieveOfEratosthenes.c \
    -o build/collision_finder
./build/collision_finder
```

---

### `targeted_attack.c`

Targeted structural probes: tests zero-byte sequences, single-byte patterns, padding sensitivity, and byte-swap invariance to surface potential fixed points or weak input classes.

```bash
gcc -O2 -Iinclude scripts/c/targeted_attack.c src/InitializationPhase.c \
    src/ProcessingPhase.c src/Calculations.c src/util.c src/SieveOfEratosthenes.c \
    -o build/targeted_attack
./build/targeted_attack
```

---

### `avalanche.c`  *(alias — see note)*

Identical source to `tests/avalanche/avalanche.c`. The CMake target `SecasyAvalanche` is the canonical build; this copy exists for quick manual compilation without CMake.

```bash
# Canonical (recommended):
cmake --build build --target SecasyAvalanche
./build/SecasyAvalanche.exe -X -m 50 -l 64 -r 100000

# Manual compile:
gcc -O2 -Iinclude scripts/c/avalanche.c src/*.c -lm -o build/avalanche_manual
```

Key flags for the avalanche tool:

| Flag | Default | Meaning |
|---|---|---|
| `-m N` | 50 | base messages |
| `-l N` | 64 | message length in bytes |
| `-B N` | 64 | bit-flips sampled per message (0 = all) |
| `-r N` | DEFAULT | number of rounds |
| `-n N` | DEFAULT | output bit-width |
| `-s SEED` | time-based | RNG seed |
| `-X` | off | extended analysis (bit bias, multi-bit) |
| `-H` | off | print Hamming-distance histogram |
| `-q` | off | quiet (stats only) |
