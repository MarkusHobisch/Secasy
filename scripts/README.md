# Security Testing Scripts

This directory contains automation scripts for the Secasy security analysis plan.

## Available Scripts

### 1. `generate_hashes.sh`
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

### 2. `validate_sac.py`
Validate SAC (Strict Avalanche Criterion) matrix results.

**Usage:**
```bash
python validate_sac.py <sac_matrix.csv> [--threshold 0.95]
```

**Examples:**
```bash
# Default: require 95% of cells in [0.48, 0.52]
python validate_sac.py ../results/sac_matrix_64B.csv

# Custom threshold and bounds
python validate_sac.py ../results/sac_matrix.csv --threshold 0.90 --lower 0.47 --upper 0.53
```

**Output:**
- Matrix statistics (mean, median, std dev, min, max)
- Acceptance rate
- Pass/fail decision
- Exit code: 0 (pass) or 1 (fail)

### 3. `validate_collisions.py`
Validate collision test results against birthday bounds.

**Usage:**
```bash
python validate_collisions.py <collision_log.txt> [--sigma 3.0]
```

**Examples:**
```bash
# Default: ±3σ acceptance band
python validate_collisions.py ../results/collision_sweep.log

# Stricter: ±2σ
python validate_collisions.py ../results/collision_24bit.log --sigma 2.0
```

**Output:**
- Per-truncation-level validation
- Deviation in σ units
- Overall pass/fail
- Exit code: 0 (pass) or 1 (fail)

### 4. `run_security_tests.sh`
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
├── README.md                    # This file
├── generate_hashes.sh           # Hash generation
├── validate_sac.py              # SAC matrix validation
├── validate_collisions.py       # Collision test validation
├── run_security_tests.sh        # Test battery orchestrator
└── (future scripts)
    ├── run_nist_sts.sh         # NIST Statistical Test Suite wrapper
    ├── run_dieharder.sh        # Dieharder wrapper
    ├── differential_search.py  # Differential cryptanalysis tool
    └── linear_bias_sampler.py  # Linear cryptanalysis tool
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

For questions about these scripts or the security analysis plan, see `SECURITY_ANALYSIS_PLAN.md` or contact the repository maintainer.
