# Cryptanalysis & Diffusion Analysis Tools

This folder contains the white-box and exhaustive analysis tools for the Secasy
hash function. **All files here are built by the CMake build system** as dedicated
test targets and use the current production defaults
(`DEFAULT_NUMBER_OF_ROUNDS = 10`, `FIELD_SIZE = 16`).

To build a single tool: `cmake --build build --target <TargetName>`.
To run: `& .\build\<TargetName>.exe` (some accept flags — see the file header).

## Contents

| File                       | CMake target                | Purpose                                                                 |
|----------------------------|-----------------------------|-------------------------------------------------------------------------|
| `algebraic_analysis.c`     | `SecasyAlgebraicAnalysis`   | Algebraic degree growth, cube and linear-approximation probes on a scaled-down model |
| `algebraic_attack_n64.c`   | `SecasyAlgebraicAttackN64`  | Regression check proving that the retired separable-XOR attack no longer matches production |
| `brute_collision_scan.c`   | `SecasyBruteCollisionScan`  | Exhaustive 1/2/3-byte digest collision scan at 32/48/64-bit truncation  |
| `cell_divergence.c`        | `SecasyCellDivergence`      | Cell Hamming-distance growth across Phase 2 (byte-by-byte diffusion)     |
| `color_op_isolation.c`     | `SecasyColorIsolation`      | Per-operation diffusion contribution (isolates each of the 6 colour ops) |
| `debug_collision.c`        | `SecasyDebugCollision`      | Diagnoses zero-Hamming collisions by tracing internal grid states       |
| `differential_replay.c`    | `SecasyDifferentialReplay`  | High-N (200k x 5 seeds) replay of the suspect output-bit bias from the linearity attack |
| `field_size_sweep.c`       | `SecasyFieldSizeSweep`      | Avalanche and symmetry across multiple grid sizes                        |
| `linearity_attack.c`       | `SecasyLinearityAttack`     | GF(2) linearity, differential distribution and cross-length collision search |
| `message_attack_n64.c`     | `SecasyMessageAttackN64`    | Bounded message-level Phase-3 and 64-bit output diffusion diagnostic |
| `phase2_collision_scan.c`  | `SecasyPhase2Collision`     | Exhaustive Phase-2 internal-state collision / neutral-block / path-cycle probe |
| `structural_attack.c`      | `SecasyStructuralAttack`    | White-box phase-by-phase instrumentation (nonlinearity census, internal avalanche, min-avalanche differential, Phase-3 bijection rank, Phase-4 dead bits) |
| `trace_collision.c`        | `SecasyTraceCollision`      | Structural enumeration of direction-collision grid positions            |

## Notes

- These tools complement the black-box suites in the sibling folders
  (`security/`, `avalanche/`, `collision/`, `statistical/`, etc.).
- CSV/log outputs are written to the build directory (the working directory when
  run from `build/`) and are git-ignored.
- `SecasyPhase2Collision --dump-hex <hex>` emits the complete reachable Phase-2
  state in a machine-readable form. Use `-` for the empty message. The exact Z3
  model in `scripts/python/phase2_reconvergence_solver.py` uses this mode as its
  independent production oracle.
- `SecasyPhase2Collision --dump-final-hex <hex>` additionally runs the ten
  production Phase-3 rounds and emits the 512-bit digest plus final value grid.
  The inverse and differential tools use it to validate every local Phase-3/4
  computation against C.
- `SecasyPhase2Collision --hash-values-hex <4096 hex digits>` applies the
  production Phase-4 extractor directly to 256 supplied 64-bit cell values.
