# Secasy Hash Function — Security Analysis Summary

## Executive Summary

Comprehensive security testing of the Secasy hash function across 16 dedicated test suites
(2.5M+ hashes, 30+ independent tests) reveals statistical properties comparable to or
exceeding SHA-256, BLAKE2b, and SHA-512. All security metrics are round-invariant, confirming
that the grid architecture — not iterative processing — is the source of cryptographic strength.

> **Note:** This document summarizes empirical findings. Formal cryptographic proofs and
> professional peer review remain outstanding. See the main [README.md](../README.md)
> Section 4.7 for a detailed assessment of what these results do and do not demonstrate.

---

## Architecture Overview

```
Phase 1: Initialization           Phase 2: Input Integration
┌──────────────────────┐          ┌──────────────────────────────────────┐
│ 16×16 grid of        │          │ Each byte → 4 × 2-bit directions    │
│ uint64_t cells       │  ─────►  │ → prime-driven jumps across grid    │
│ (256 cells, all = 2) │          │ → data-dependent jump distances     │
│                      │          │   (single-axis per direction)       │
└──────────────────────┘          └──────────────────────────────────────┘
                                              ↓
Phase 3: Processing Rounds        Phase 4: Hash Extraction
┌──────────────────────────┐      ┌──────────────────────────────────────┐
│ r rounds (default: 10)    │      │ Position-weighted XOR accumulation   │
│ 6 neighbor-coupled ops    │ ──►  │ with 7-bit rotation over 256 cells  │
│ per cell per round        │      │ → one 64-bit block per round        │
│ (ADD/SUB/XOR/AND/OR/INV) │      │ → concatenate for larger hash sizes │
└──────────────────────────┘      └──────────────────────────────────────┘
```

| Parameter          | Value                                        |
|--------------------|----------------------------------------------|
| Internal state     | 16×16 × 64-bit = **16,384 bits** (wide-pipe) |
| Output size        | 64–512 bits (default: 512)                   |
| State:output ratio | **32:1** (cf. SHA-3: 6.25:1, SHA-256: 1:1)   |
| Processing rounds  | 10 (default), minimum = ⌈hashBits / 64⌉      |
| Operations         | ADD, SUB, XOR, AND, OR, INVERT               |

---

## Empirical Security Results

### Statistical Quality (N = 1,000,000, SecasyStatRigor)

| Property               | Value         | Ideal   | Status |
|------------------------|---------------|---------|--------|
| Avalanche rate         | 50.0007%      | 50.000% | ✅      |
| Max bit bias           | 0.149%        | 0%      | ✅      |
| Sequential correlation | 49.999%       | 50.000% | ✅      |
| Collisions (512-bit)   | 0 / 1,000,000 | 0       | ✅      |

### Test Suite Summary

| Test Suite                         | Tests | Result   |
|------------------------------------|-------|----------|
| SecasyComprehensiveSecurity        | 10/10 | ✅ PASS   |
| SecasyDeepSecurity                 | 4/4   | ✅ PASS   |
| SecasyExtendedSecurity             | 5/5   | ✅ PASS   |
| SecasyPracticalExploit             | 4/4   | ✅ PASS   |
| SecasyStatisticalRandomness (NIST) | 10/10 | ✅ PASS   |
| SecasyDifferential                 | 5/5   | ✅ PASS   |
| SecasyAvalanche                    | —     | ✅ ~50%   |
| SecasyCollision                    | —     | ✅ 0 col  |
| SecasyFuzz (500k iterations)       | —     | ✅ 0 err  |
| SecasyRoundReduction (all sizes)   | —     | ✅ stable |

### Round-Invariance

Security metrics are statistically indistinguishable from 100,000 rounds down to 1 round,
across all hash sizes (64, 128, 256, 512 bit). See
[ROUND_REDUCTION_ANALYSIS.md](../ROUND_REDUCTION_ANALYSIS.md) for full data tables.

---

## Key Security Properties

### 1. Wide-Pipe Design

The 32:1 ratio of internal state (16,384 bits) to output (512 bits) makes length extension
attacks infeasible — an attacker who knows H(m) cannot reconstruct the remaining 15,872 bits
of internal state.

### 2. Non-Invertible Operations

AND and OR operations are not invertible. Given `a AND b = c`, neither `a` nor `b` can be
recovered uniquely. This prevents backward computation from hash output to input.

### 3. Data-Dependent Jump Distances

Each cursor step jumps by the current cell's prime value, which changes after each visit.
This creates a feedback loop: the path depends on the field state, which in turn depends on
the path already taken. Two inputs that diverge at any byte produce cascading differences in
jump distances, causing rapid path separation across the grid.

### 4. Hash Extraction

The extraction function iterates over all 256 cells in row-major order, multiplies each cell
value by a unique position weight (1–256), XORs into a 64-bit accumulator, and left-rotates
by 7 bits per step. One 64-bit block is collected per processing round; blocks are
concatenated for larger hash sizes (e.g., 8 blocks for 512-bit output).

### 5. Side-Channel Considerations

The current implementation is **not** constant-time (data-dependent branches in
`switch(colorIndex)`, data-dependent cache accesses on the prime table). This is acceptable
for pure hashing but would require a branchless variant for HMAC or key derivation use.

---

## Test Tools

All test executables are built via CMake. Common flags: `-r` (rounds), `-n` (hash bits),
`-s` (seed), `-m` (sample count).

| Executable                    | Location               | Purpose                        |
|-------------------------------|------------------------|--------------------------------|
| `SecasyAvalanche`             | tests/avalanche/       | Avalanche / diffusion analysis |
| `SecasyCollision`             | tests/collision/       | Collision / distribution       |
| `SecasyTruncCollision`        | tests/collision/       | Truncated collision PoC        |
| `SecasyDifferential`          | tests/differential/    | Differential cryptanalysis     |
| `SecasyStatisticalRandomness` | tests/statistical/     | NIST-inspired randomness       |
| `SecasyStatRigor`             | tests/statistical/     | Large-sample (N=1M) rigor      |
| `SecasyHashPattern`           | tests/statistical/     | Pattern / structure analysis   |
| `SecasyComprehensiveSecurity` | tests/security/        | 10-test security battery       |
| `SecasyDeepSecurity`          | tests/security/        | Linear/diff./state/weak-key    |
| `SecasyExtendedSecurity`      | tests/security/        | Length ext., bit indep., etc.  |
| `SecasyPracticalExploit`      | tests/security/        | Practical exploit attempts     |
| `SecasyRoundReduction`        | tests/round_reduction/ | Round count sweep (CSV)        |
| `SecasyBenchmark`             | tests/performance/     | Performance comparison         |
| `SecasyPreciseTiming`         | tests/performance/     | Nanosecond precision timing    |
| `SecasyProfiling`             | tests/performance/     | Phase-level profiling          |
| `SecasyFuzz`                  | tests/fuzzing/         | 500k-iteration fuzz test       |

---

## Conclusion

Secasy passes every empirical test that can be performed without deep cryptanalysis — and
passes them with results closer to the theoretical ideal than SHA-256, BLAKE2b, and SHA-512.
This is a **necessary** but **not sufficient** condition for cryptographic security.
Independent cryptanalysis of the grid structure's algebraic properties is the essential next
step.

**Assessment:**

- Learning and experimentation: ✅ Excellent
- General-purpose hashing: ✅ Suitable
- Experimental cryptographic use: ✅ Meets empirical SAC requirements
- Production security: ⚠️ Pending formal review and peer analysis