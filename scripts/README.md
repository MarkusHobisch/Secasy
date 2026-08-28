# Security Testing & Visualisation Scripts

This directory contains automation scripts for the Secasy security analysis plan,
organised by language:

| Directory  | Language   | Contents                                          |
|------------|------------|---------------------------------------------------|
| `python/`  | Python 3   | Plot generators, parameter scanners, validators   |
| `shell/`   | Bash       | Hash generation, test-battery orchestration        |
| `c/`       | C11        | Standalone attack simulations (compiled manually)  |

## Available Scripts

### Shell Scripts (`shell/`)

#### `generate_hashes.sh`

Generate random hashes for statistical testing (NIST STS, Dieharder, etc.).

**Usage:**

```bash
./generate_hashes.sh [count] [input_length] [rounds]
```

**Examples:**

```bash
# Generate 1M hashes with default parameters
./generate_hashes.sh 1000000

# Custom: 500K hashes, 128-byte inputs, 50K rounds
./generate_hashes.sh 500000 128 50000
```

### Python Validation Scripts (`python/`)

#### `phase2_reconvergence_solver.py`

Build and validate an exact bounded model of the Phase-2 cursor walk, then
search for two different messages with identical cell values, color schedule,
prime indices, and final cursor. Additional modes synthesize equal schedules,
trace differentials through Phase 3/4, scan structured families and suffixes,
and validate the exact Phase-3 inverse.

```powershell
python -m pip install z3-solver --target build/python-deps
cmake --build build --target SecasyPhase2Collision
python scripts/python/phase2_reconvergence_solver.py validate `
  --oracle build/SecasyPhase2Collision.exe --trials 200 --max-bytes 64
python scripts/python/phase2_reconvergence_solver.py search `
  --bytes-a 3 --bytes-b 3 --first-difference 11 `
  --oracle build/SecasyPhase2Collision.exe `
  --result build/phase2-search-3x3-fd11.json
python scripts/python/phase2_reconvergence_solver.py search `
  --mode equal-schedule --bytes-a 3 --bytes-b 3 `
  --oracle build/SecasyPhase2Collision.exe
python scripts/python/phase2_reconvergence_solver.py analyze-pair `
  --message-a 42f360 --message-b c2f1c0 `
  --oracle build/SecasyPhase2Collision.exe
python scripts/python/phase2_reconvergence_solver.py validate-inverse `
  --oracle build/SecasyPhase2Collision.exe
python scripts/python/phase2_reconvergence_solver.py phase4-collision `
  --message 42f360 --tail-cells 16 `
  --oracle build/SecasyPhase2Collision.exe `
  --result build/phase4-collision-42f360-tail16.json
python scripts/python/phase2_reconvergence_solver.py phase4-tail-collision `
  --message 42f360 --tail-cells 10 --chunk-bits 1 --timeout-ms 300000 `
  --oracle build/SecasyPhase2Collision.exe `
  --result build/phase4-tail-collision-42f360-tail10.json
python scripts/python/phase2_reconvergence_solver.py phase4-prime-tail-collision `
  --message 42f360 --tail-cells 10 --prime-limit 16 --chunk-bits 1 `
  --oracle build/SecasyPhase2Collision.exe `
  --result build/phase4-local-prime-tail.json
python scripts/python/phase2_reconvergence_solver.py phase2-target-reachability `
  --message 42f360 --target-result build/phase4-local-prime-tail.json `
  --side left --bytes 12 --oracle build/SecasyPhase2Collision.exe
```

`SAT` is independently replayed through the C oracle. `UNSAT` applies only to
the exact parameter set in the JSON result; `unknown` and timeout are reported
as inconclusive.

#### `up_rotor_cycle_search.py`

Search the exact all-zero Phase-2 rotor with constant-memory Brent cycle
detection. For the default production parameters it finds the zero-preperiod
cycle of 2,131,224 bytes and can write collision files that are verified by the
production executable.

```powershell
python scripts/python/up_rotor_cycle_search.py `
  --write-dir build/collision-proof `
  --oracle build/Secasy.exe `
  --result build/collision-proof/result.json
python scripts/python/up_rotor_cycle_search.py `
  --same-length-byte-hex aa `
  --write-dir build/collision-proof-equal-length `
  --oracle build/Secasy.exe `
  --result build/collision-proof-equal-length/result.json
```

The first command verifies the cross-length collision
`00^2,131,224` / `00^4,262,448`. The second verifies the equal-length
collision `00^2,131,224` / `aa^2,131,224`.

#### `secasy_second_preimage.py`

Create `m2 = neutral-prefix || m` for the current vulnerable Secasy
configuration and verify the full 512-bit collision with the production
executable. The script accepts files, UTF-8 text, and hexadecimal bytes. It
writes the output atomically and refuses to overwrite files unless `--force`
is supplied.

```powershell
cmake --build build --target Secasy
python scripts/python/secasy_second_preimage.py `
  --text "Hallo Secasy" `
  --output build/hallo-second-preimage.bin
python scripts/python/secasy_second_preimage.py `
  --file path/to/message.bin `
  --output build/message-second-preimage.bin `
  --result build/message-second-preimage.json
python scripts/python/secasy_second_preimage.py `
  --hex "00 ff 41 42" `
  --prefix-byte aa `
  --output build/hex-second-preimage.bin
```

This is a regression and research demonstrator for the current
`FIELD_SIZE=16`, six-colour, 88,801-prime configuration. It is not a generic
collision attack against standard hash functions. Prepending bytes can also
invalidate a structured file format even though its Secasy digest is unchanged.

#### `validate_sac.py`

Validate SAC (Strict Avalanche Criterion) matrix results.

**Usage:**

```bash
python scripts/python/validate_sac.py <sac_matrix.csv> [--threshold 0.95]
```

**Examples:**

```bash
# Default: require 95% of cells in [0.48, 0.52]
python scripts/python/validate_sac.py ../results/sac_matrix_64B.csv

# Custom threshold and bounds
python scripts/python/validate_sac.py ../results/sac_matrix.csv --threshold 0.90 --lower 0.47 --upper 0.53
```

**Output:**

- Matrix statistics (mean, median, std dev, min, max)
- Acceptance rate
- Pass/fail decision
- Exit code: 0 (pass) or 1 (fail)

#### `validate_collisions.py`

Validate collision test results against birthday bounds.

**Usage:**

```bash
python scripts/python/validate_collisions.py <collision_log.txt> [--sigma 3.0]
```

**Examples:**

```bash
# Default: ±3σ acceptance band
python scripts/python/validate_collisions.py ../results/collision_sweep.log

# Stricter: ±2σ
python scripts/python/validate_collisions.py ../results/collision_24bit.log --sigma 2.0
```

**Output:**

- Per-truncation-level validation
- Deviation in σ units
- Overall pass/fail
- Exit code: 0 (pass) or 1 (fail)

#### `run_security_tests.sh`

Run comprehensive test battery with automated validation.

**Usage:**

```bash
./run_security_tests.sh [quick|full]
```

**Modes:**

- `quick`: Fast sanity checks (~5 minutes)
- `full`: Comprehensive tests (~30-60 minutes)

**Examples:**

```bash
# Quick smoke test
./run_security_tests.sh quick

# Full battery
./run_security_tests.sh full
```

**Output:**

- Real-time test execution
- Pass/fail indicators
- Summary statistics
- Logs saved to `../results/`

## Integration with CI/CD

These scripts are designed to integrate with GitHub Actions or other CI systems.

**Example `.github/workflows/security.yml`:**

```yaml
name: Security Tests

on: [push, pull_request]

jobs:
  quick-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: cmake -S . -B build && cmake --build build
      - name: Run Quick Security Tests
        run: |
          cd scripts
          chmod +x run_security_tests.sh
          ./run_security_tests.sh quick
```

## Requirements

### System Dependencies

- **Bash** (for shell scripts)
- **Python 3.7+** (for validation scripts)
- **CMake** (for building Secasy)
- **GCC/Clang** (C11 compatible)

### Python Packages

None required for basic scripts. Optional:

- `numpy` (for advanced statistical analysis)
- `matplotlib` (for visualization)

Install with:

```bash
pip install numpy matplotlib
```

## Directory Structure

```
scripts/
├── README.md                           # This file
├── SECURITY_ANALYSIS.md                # Security analysis plan
├── python/                             # Python 3 scripts
│   ├── plot_arx_comparison.py          # ARX migration before/after chart
│   ├── plot_cell_divergence_multi.py   # Cell divergence (5 flip positions)
│   ├── plot_cell_divergence_seeds.py   # Cell divergence (cross-seed)
│   ├── plot_color_isolation.py         # Colour-operation isolation plots
│   ├── plot_field_size_sweep.py        # Field-size sweep plots
│   ├── plot_grid_landscape.py          # 3D grid-state landscape
│   ├── plot_round_reduction.py         # Round-reduction analysis
│   ├── phase2_reconvergence_solver.py  # Exact Phase-2/3/4 solver models
│   ├── scan_reduced_params.py          # Reduced-parameter scanning
│   ├── secasy_second_preimage.py        # Generate verified second preimages
│   ├── up_rotor_cycle_search.py         # Find and verify uniform-byte Phase-2 cycles
│   ├── validate_collisions.py          # Collision test validation
│   └── validate_sac.py                # SAC matrix validation
├── shell/                              # Bash scripts
│   ├── generate_hashes.sh             # Hash generation for stat. tests
│   ├── run_extended_avalanche_wsl.sh  # Extended avalanche (WSL)
│   └── run_security_tests.sh          # Test battery orchestrator
└── c/                                  # Standalone C analysis tools
    ├── avalanche.c                    # Avalanche analysis
    ├── collision_finder.c             # Collision search
    └── targeted_attack.c             # Targeted attack simulation
```

## Next Steps

### To Be Implemented

1. **NIST STS Integration** (`run_nist_sts.sh`)
    - Download NIST STS
    - Generate bitstream from hashes
    - Parse results, extract p-values
    - Automated pass/fail

2. **Dieharder Wrapper** (`run_dieharder.sh`)
    - Stream hashes to dieharder
    - Filter results
    - Summary report

3. **Differential Search** (`differential_search.py`)
    - Reduced-round testing
    - DDT construction
    - Probability estimation

4. **Linear Bias Sampler** (`linear_bias_sampler.py`)
    - Mask generation
    - Walsh-Hadamard transform
    - Bias distribution analysis

5. **Visualization Suite**
    - SAC matrix heatmaps
    - Collision distribution plots
    - Differential/linear probability graphs

## Contributing

When adding new scripts:

1. Follow naming convention: `<action>_<target>.{sh,py}`
2. Include usage comments at top of file
3. Add entry to this README
4. Ensure executable permissions: `chmod +x script.sh`
5. Test on both Linux and macOS (if applicable)

## License

Same as parent Secasy project.

## Contact

For questions about these scripts or the security analysis plan, see `SECURITY_ANALYSIS_PLAN.md` or contact the
repository maintainer.
