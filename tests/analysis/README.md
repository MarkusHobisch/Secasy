# Legacy Analysis Utilities

This folder contains standalone analysis and debugging tools developed during early
exploration of the Secasy hash function. **These files are not built by the CMake
build system** and are preserved for historical reference only.

## Contents

| File | Purpose |
|------|---------|
| `analyze_2byte.c` | Examines hash behavior for 2-byte inputs |
| `analyze_collision.c` | Early collision analysis tool |
| `benchmark_speed.c` | Simple speed benchmark |
| `collision_attack.c` | Collision search attempt |
| `collision_test_fixed.c` | Fixed-parameter collision test |
| `compare_fields.c` | Compares internal field states between inputs |
| `compare_hashes.py` | Python script for hash output comparison |
| `debug_directions.c` | Traces directional movement through the grid |
| `exact_trace.c` | Full execution trace for debugging |
| `find_all_collisions.c` | Exhaustive collision finder (small spaces) |
| `full_field_compare.c` | Compares full 256-cell field between runs |
| `hash_analysis.py` | Python analysis of hash output statistics |
| `preimage.c` | Preimage resistance exploration |
| `sac_exploit.c` | SAC exploitation attempt |
| `show_fields.c` | Pretty-prints the internal grid state |
| `simple_functional_test.c` | Basic correctness sanity check |
| `statistical_test.c` | Early statistical quality test |
| `temp_dir.c` | Temporary directional analysis utility |
| `test_1byte.c` | Single-byte input analysis |
| `test_2_3_byte.c` | 2- and 3-byte input analysis |
| `test_position_mixing.c` | Tests position-dependent mixing quality |
| `test_rounds_needed.c` | Early round-count exploration |
| `trace_movements.c` | Traces cursor movement across the grid |

## Notes

- Many of these files reference outdated parameters (e.g., `numberOfRounds = 100000`,
  `FIELD_SIZE = 8`). They have **not** been updated to match the current production
  defaults (`DEFAULT_NUMBER_OF_ROUNDS = 10`, `FIELD_SIZE = 16`).
- For current security testing, use the CMake-built test targets in the sibling folders
  (`security/`, `avalanche/`, `collision/`, etc.).
- To compile any file manually:
  ```bash
  gcc -std=c11 -O2 -o tool_name tool_name.c ../../Calculations.c ../../InitializationPhase.c \
      ../../ProcessingPhase.c ../../SieveOfEratosthenes.c ../../util.c -lm -I../..
  ```
