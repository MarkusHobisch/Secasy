# Secasy Hash Function – Cryptographic Security Analysis Plan

**Document Version:** 1.0  
**Date:** 2025-10-02  
**Status:** Planning Phase  
**Estimated Timeline:** 12 weeks (minimum) to 12 months (comprehensive + iterations)

---

## Executive Summary

This document outlines a systematic, multi-phase approach to evaluate the cryptographic security properties of the Secasy hash function. The analysis progresses from basic diffusion measurements through advanced cryptanalytic techniques, culminating in a go/no-go decision on production readiness.

**Current Baseline:** Preliminary avalanche testing shows promising first-order diffusion (~0.5 flip rate), but no second-order properties, collision resistance bounds, or structural security arguments have been established.

**Goal:** Determine whether Secasy can achieve security comparable to SHA-2/SHA-3 family members, or identify fundamental weaknesses requiring redesign.

---

## Analysis Phases Overview

| Phase | Focus Area | Duration | Priority | Blocking? |
|-------|-----------|----------|----------|-----------|
| 1 | Specification & Design Review | 1 week | Critical | Yes |
| 2 | First-Order Diffusion (SAC, Correlation) | 1 week | Critical | Yes |
| 3 | Collision Resistance (Birthday, Truncation) | 2 weeks | Critical | Yes |
| 4 | Differential Cryptanalysis | 2 weeks | High | Yes |
| 5 | Linear Cryptanalysis | 1 week | High | No |
| 6 | Algebraic Attacks | 1 week | Medium | No |
| 7 | Preimage/Second-Preimage | 1 week | High | Yes |
| 8 | Statistical Quality (NIST, Dieharder) | 1 week | Medium | No |
| 9 | Side-Channel Considerations | 1 week | Low | No |
| 10 | Formal Methods (Optional) | 2+ weeks | Low | No |

**Total Minimal Path:** Phases 1-4, 7 = ~7 weeks  
**Comprehensive Coverage:** All phases = ~12+ weeks

---

## Phase 1: Specification & Design Review

### Objectives
- Produce complete formal specification of the algorithm
- Identify potential structural weaknesses from design
- Establish baseline security claims to test

### Tasks

#### 1.1 Algorithm Documentation
- [ ] **2D Field Architecture**: Document exact dimensions, initial state, coordinate system
- [ ] **Color Operations**: Formal definition of all 6 operations (ADD, SUB, XOR, AND, OR, INVERT) with edge-case behavior
- [ ] **Prime Number Initialization**: Sieve parameters, selection criteria, impact on state space
- [ ] **Processing Loop**: Input chunking, field update order, round structure
- [ ] **Finalization Function**: Current fmix-inspired mixing, bit-width reduction, hex formatting
- [ ] **Padding Scheme**: Ensure unambiguous message framing (critical for security)

#### 1.2 State Space Analysis
- [ ] Calculate total internal state size (bits)
- [ ] Compare state size vs. output size (must be significantly larger to prevent preimage shortcuts)
- [ ] Document all parameters: rounds, field size, prime count range

#### 1.3 Design Philosophy Assessment
- [ ] Compare with known constructions (Merkle-Damgård, Sponge, HAIFA, Wide-Pipe)
- [ ] Evaluate novelty risks: 2D field = unconventional → higher audit burden
- [ ] Security margin: What is the minimal number of rounds for assumed security? How many extra rounds are included?

#### 1.4 Edge Cases & Special Inputs
- [ ] Empty input handling
- [ ] Single-byte inputs
- [ ] Very large inputs (GB-scale)
- [ ] All-zero vs. all-one inputs
- [ ] Collision of padding with actual data

### Deliverables
- `docs/SPECIFICATION.md` – Complete algorithm description
- `docs/DESIGN_RATIONALE.md` – Security design choices
- State-space diagram / flowchart

### Success Criteria
- ✓ No obvious algebraic shortcuts identified
- ✓ State space >> output space (by factor >2^32)
- ✓ Padding unambiguous under all conditions

---

## Phase 2: First-Order Diffusion Analysis

### Objectives
- Verify avalanche effect holds at per-bit granularity
- Detect positional biases or correlation structure
- Extend beyond current global mean measurements

### Tasks

#### 2.1 Strict Avalanche Criterion (SAC) Matrix
**Tool:** Extend `tests/avalanche/avalanche.c`

**Implementation:**
```c
// New flag: --sac-matrix <output.csv>
// For each input bit i (0..inputBits-1):
//   For each output bit j (0..outputBits-1):
//     Measure P(output_bit_j flips | input_bit_i flipped)
//     Store in matrix[i][j]
```

**Test Protocol:**
- Input sizes: 64, 128, 256, 512 bytes
- Rounds: 50%, 100%, 150% of default
- Messages per (i,j) pair: ≥1000
- Output: CSV matrix for visualization

**Acceptance:**
- 95% of cells in [0.48, 0.52]
- No row/column showing systematic bias (e.g., all values <0.45 or >0.55)
- Chi² test: χ² < critical value for df=(inputBits × outputBits - 1)

#### 2.2 Bit Correlation Analysis
**Pairwise Output Bit Correlation:**
- Generate 10^6 random hashes
- For each bit pair (j, k): Calculate Pearson correlation r_{jk}
- Build correlation matrix (symmetric)

**Mutual Information:**
- I(bit_j ; bit_k) for all pairs
- Expected: ~0 bits for independent outputs

**Thresholds:**
- |r| < 0.05 for >99% of pairs
- I < 0.01 bits for >99% of pairs

#### 2.3 Multi-Bit Perturbation Stability
**Extend current k={2,4,8} to k={1,2,4,8,16,32,64}:**
- For each k: flip k random input bits simultaneously
- Measure output Hamming distance distribution
- Compare vs. Binomial(n=outputBits, p=0.5)

**Statistical Test:**
- Kolmogorov-Smirnov test: D < 0.05
- Mean within [0.49, 0.51]
- Variance consistent with binomial

### Deliverables
- `results/sac_matrix_64B.csv`, `results/sac_matrix_256B.csv`, etc.
- `results/correlation_matrix.png` (heatmap visualization)
- `results/multibit_distribution.pdf` (histogram overlays)

### Success Criteria
- ✓ SAC acceptance rate ≥95%
- ✓ No significant correlations detected
- ✓ Multi-bit distributions indistinguishable from ideal

**Failure Response:** Strengthen finalization mixing, add nonlinear layer, increase rounds

---

## Phase 3: Collision Resistance Testing

### Objectives
- Validate birthday-bound behavior at multiple truncation levels
- Detect structural clustering or bias enabling shortcuts
- Test structured input classes for unexpected collisions

### Tasks

#### 3.1 Truncated Birthday Experiments
**Already partially implemented in `tests/collision/collision.c`**

**Extended Test Matrix:**

| Truncation (bits) | Messages (m) | Expected Collisions | Threshold (±3σ) |
|-------------------|--------------|---------------------|-----------------|
| 24 | 100,000 | ~298 | [282, 314] |
| 28 | 200,000 | ~750 | [718, 782] |
| 32 | 1,000,000 | ~116 | [96, 136] |
| 36 | 2,000,000 | ~290 | [258, 322] |
| 40 | 5,000,000 | ~454 | [412, 496] |
| 48 | 20,000,000 | ~710 | [660, 760] |
| 56 | 100,000,000 | ~694 | [645, 743] |
| 64 | 500,000,000 | ~678 | [630, 726] |

**Execution:**
```bash
for bits in 24 28 32 36 40 48; do
  m=$((2**(bits/2 + 3)))
  ./SecasyCollision -m $m -l 64 -r 100000 -T $bits -s $RANDOM \
    > results/collision_${bits}bit.log
done
```

**Acceptance:** Observed collisions within ±3√E for all levels

#### 3.2 Distribution Quality Tests
**Global Hex Frequency (-F):**
- 16 symbols (0-f) should appear equally: ~1/16 each
- Chi² test: df=15, α=0.01 → χ² < 30.58

**Positional Frequency (-P):**
- Each hex position (0..63 for 256-bit) should be uniform
- No "hot spots" or missing nibbles

**Byte Distribution (-B):**
- 256 classes, expected count = total / 256
- Chi²: df=255, α=0.01 → χ² < 310.46

#### 3.3 Structured Input Classes
**Test Suites:**

1. **Low Hamming Weight:**
   - Generate inputs with ≤8 set bits
   - Hash 1 million samples
   - Check for collision clustering

2. **Repetitive Patterns:**
   - "AAAA..." (repeated byte)
   - "ABAB..." (alternating)
   - "0123456789ABCDEF" repeated
   - 100k samples each

3. **Incremental Counters:**
   - Hash(i || padding) for i=0..10^6
   - Detect sequential structure in outputs

4. **Related Messages:**
   - Pairs (m, m⊕δ) for small Hamming distance δ={1,2,4,8}
   - Output Hamming distance should still be ~0.5 × outputBits

**Acceptance:**
- No observable clusters (visual inspection + nearest-neighbor analysis)
- Collision rates match random baseline

### Deliverables
- `results/collision_summary.csv` (all truncation levels)
- `results/structured_inputs_report.md`
- `results/distribution_chi2.log`

### Success Criteria
- ✓ All truncation levels pass ±3σ test
- ✓ Chi² tests pass for hex/byte distributions
- ✓ No structural weaknesses in special input classes

**Failure Response:** Identify bias source (finalization? field ops?), apply targeted fix, re-test

---

## Phase 4: Differential Cryptanalysis

### Objectives
- Search for high-probability differential characteristics
- Evaluate how input differences propagate through rounds
- Determine security margin vs. differential attacks

### Tasks

#### 4.1 Reduced-Round Differential Search
**Variants to Test:**
- 10% rounds (e.g., 10,000 instead of 100,000)
- 25% rounds
- 50% rounds
- 75% rounds
- 100% (baseline)

**Method:**
1. Fix input difference Δ (e.g., single bit, byte, specific pattern)
2. Hash many pairs (m, m⊕Δ)
3. Compute output differences Δ' = H(m) ⊕ H(m⊕Δ)
4. Build difference distribution table (DDT)

**Metrics:**
- Max probability over all Δ → Δ' transitions
- Effective differential probability (EDP) for best trail

**Tool Development:**
```bash
# New tool: tests/differential/differential.c
./SecasyDifferential \
  --rounds 50000 \
  --input-delta 0x0000000000000001 \
  --samples 1000000 \
  --export-ddt ddt_50pct.csv
```

**Acceptance Thresholds:**

| Rounds (%) | Max Single-Path Probability | EDP Bound |
|------------|----------------------------|-----------|
| 10 | <2^-8 | <2^-4 |
| 25 | <2^-16 | <2^-12 |
| 50 | <2^-32 | <2^-24 |
| 75 | <2^-48 | <2^-40 |
| 100 | <2^-64 | <2^-56 |

#### 4.2 Color Operation Differential Properties
**Individual Op Analysis:**
- Isolate each operation (ADD, SUB, XOR, AND, OR, INVERT)
- Build mini-DDT for single-step application
- Identify linear vs. nonlinear ops

**XOR Linearity Check:**
- Is XOR op fully linear? (Expected: yes, by definition)
- Does it provide sufficient mixing when combined with others?

**SUB/ADD Modular Wrapping:**
- Do overflows introduce nonlinearity?
- Probability of difference propagation through carry chains

#### 4.3 Diffusion Speed Measurement
**Per-Round Tracking:**
- Start with single-bit input difference
- Measure active output bits after each round (1, 10, 100, 1000, ...)
- Plot "diffusion curve"

**Ideal Behavior:**
- Exponential spread in early rounds
- Full avalanche (all output bits affected ~50%) by round R_min
- R_min << default rounds (security margin)

### Deliverables
- `results/differential_probability_table.csv`
- `results/diffusion_speed_plot.pdf`
- `results/color_op_ddt_analysis.md`

### Success Criteria
- ✓ No exploitable differential trails at 50%+ rounds
- ✓ EDP decreases exponentially with round count
- ✓ Full diffusion achieved in <25% of total rounds

**Failure Response:** Add rounds, introduce S-box or nonlinear layer, redesign field update order

---

## Phase 5: Linear Cryptanalysis

### Objectives
- Measure linear approximation bias
- Identify best linear trails
- Ensure resistance to Matsui-style attacks

### Tasks

#### 5.1 Walsh-Hadamard Transform Sampling
**Concept:**
- Input mask α, output mask β
- Compute bias ε = |P(α·input ⊕ β·output = 0) - 0.5|
- Small ε → good resistance

**Sampling Strategy:**
- Random (α, β) pairs: 10^6 samples
- Focused search: Low Hamming weight masks
- Reduced rounds: 25%, 50%, 75%

**Tool:**
```bash
# New: tests/linear/linear_bias.c
./SecasyLinear \
  --rounds 50000 \
  --mask-samples 1000000 \
  --max-weight 8 \
  --export linear_bias_50pct.csv
```

**Acceptance:**

| Rounds (%) | Max Bias (ε) |
|------------|--------------|
| 25 | <2^-16 |
| 50 | <2^-32 |
| 75 | <2^-48 |
| 100 | <2^-64 |

#### 5.2 Linear Approximation Table (LAT)
**Reduced-Round Core:**
- Build LAT for 1-round, 2-round, 4-round fragments
- Identify worst-case linear hull
- Extrapolate to full rounds

**Metrics:**
- LAT max absolute entry
- Number of entries > threshold (e.g., >2^-10)

### Deliverables
- `results/linear_bias_distribution.csv`
- `results/lat_reduced_rounds.txt`
- `results/linear_hull_estimate.md`

### Success Criteria
- ✓ Max bias at 50% rounds < 2^-32
- ✓ No exploitable linear approximations at full rounds

**Failure Response:** Add algebraic complexity, more rounds, nonlinear injection

---

## Phase 6: Algebraic Attacks

### Objectives
- Assess vulnerability to equation-solving attacks
- Measure algebraic degree and complexity

### Tasks

#### 6.1 Boolean Function Representation
**Output Bits as Polynomials:**
- Express each output bit as ANF (Algebraic Normal Form) over input bits
- Measure degree (max term size)
- Count monomials

**Ideal:**
- Degree > outputBits / 2
- High monomial count (near-maximum)
- No low-degree annihilators

#### 6.2 SAT/SMT Solver Probing
**Small Instance Test:**
- Reduce to 32-bit output variant (if feasible)
- Encode as SAT problem: given output, find input
- Use CryptoMiniSat, Glucose, or Z3

**Benchmark:**
- Timeout: 24 hours on modern CPU
- Success → **Critical Failure**

**Tool:**
```python
# scripts/algebraic_sat_test.py
# Generate CNF from reduced Secasy instance
# Feed to solver, measure time-to-solution
```

#### 6.3 Gröbner Basis Attempt
**Using Sage/Singular:**
- Build polynomial system for 2-4 rounds
- Attempt Gröbner basis computation
- Measure complexity growth vs. rounds

### Deliverables
- `results/algebraic_degree_analysis.txt`
- `results/sat_solver_benchmark.log`
- `results/groebner_complexity.md`

### Success Criteria
- ✓ Algebraic degree ≥ 128 (for 256-bit output)
- ✓ SAT solver fails within 24h (or requires 2^64+ operations estimate)

**Failure Response:** May require fundamental redesign if solvable

---

## Phase 7: Preimage & Second-Preimage Resistance

### Objectives
- Ensure no shortcuts to invert the hash
- Verify finalization is not easily reversible
- Check for length-extension vulnerabilities

### Tasks

#### 7.1 State Space vs. Output Space
**Analysis:**
- Internal state bits: S
- Output bits: O
- Generic preimage attack: 2^O
- If S ≤ O → potential shortcut

**Requirement:** S ≥ O + 128 (for 128-bit security margin)

#### 7.2 Finalization Invertibility
**Current Design:** fmix-inspired mixing → hex formatting

**Tests:**
- Can we reverse the finalization steps?
- Given final hash, can we recover the pre-finalization state?
- Is there a many-to-one mapping introducing collisions?

**Method:**
- Attempt analytical inversion
- Brute-force small subsets (e.g., recover 32-bit chunks)

#### 7.3 Meet-in-the-Middle Feasibility
**Conceptual Split:**
- Divide rounds into two halves: R1 (forward), R2 (backward)
- Can we compute:
  - Forward: All states after R1
  - Backward: All states before R2
  - Find collision in middle state

**Complexity Estimate:**
- If feasible with complexity < 2^(O/2) → **weakness**

#### 7.4 Length-Extension Check
**Test:**
- Given H(m), can we compute H(m || extension) without knowing m?
- Secasy is not Merkle-Damgård, so should be immune
- Verify with concrete examples

### Deliverables
- `results/state_space_analysis.md`
- `results/finalization_invertibility_test.log`
- `results/mitm_complexity_estimate.md`

### Success Criteria
- ✓ State space >> output space
- ✓ Finalization not invertible (or only via brute-force)
- ✓ MITM complexity ≥ 2^(outputBits)
- ✓ No length-extension vulnerability

**Failure Response:** Expand internal state, strengthen finalization, add domain separation

---

## Phase 8: Statistical Quality Tests

### Objectives
- Pass industry-standard randomness test suites
- Ensure output indistinguishable from random oracle

### Tasks

#### 8.1 NIST Statistical Test Suite (SP 800-22)
**Tests (15 total):**
1. Frequency (Monobit)
2. Block Frequency
3. Runs
4. Longest Run of Ones
5. Binary Matrix Rank
6. Discrete Fourier Transform
7. Non-Overlapping Template Matching
8. Overlapping Template Matching
9. Maurer's Universal Statistical
10. Linear Complexity
11. Serial (2 variants)
12. Approximate Entropy
13. Cumulative Sums (2 variants)
14. Random Excursions (8 variants)
15. Random Excursions Variant (18 variants)

**Protocol:**
- Generate 10^6 hashes from random inputs
- Concatenate outputs into bitstream
- Run NIST STS

**Acceptance:** p-value ≥ 0.01 for ALL tests

**Execution:**
```bash
# Generate test data
./generate_random_hashes.sh 1000000 > nist_input.bin

# Run NIST STS
cd nist-sts
./assess 1048576 < ../nist_input.bin
```

#### 8.2 Dieharder Test Battery
**Comprehensive Randomness Tests:**
```bash
./generate_hashes.sh | dieharder -a -g 200
```

**Acceptance:** No "FAILED" results, <5% "WEAK"

#### 8.3 TestU01 (BigCrush)
**Most Stringent Suite:**
- 160 tests
- Runtime: hours to days

**Execution:**
```c
// Wrapper to feed Secasy output to TestU01
#include "TestU01.h"
unif01_Gen *gen = /* Secasy hash generator */;
bbattery_BigCrush(gen);
```

**Acceptance:** All tests pass

### Deliverables
- `results/nist_sts_report.txt`
- `results/dieharder_summary.log`
- `results/testu01_bigcrush.txt`

### Success Criteria
- ✓ NIST: 15/15 pass
- ✓ Dieharder: All pass or weak (no fails)
- ✓ BigCrush: All pass

**Failure Response:** Fix output formatting, strengthen finalization, check for hidden structure

---

## Phase 9: Side-Channel Considerations

### Objectives
- Identify potential timing or power leakage
- Recommend countermeasures for hardware/embedded implementations

### Tasks

#### 9.1 Timing Analysis
**Variable-Time Operations:**
- Do any color ops have data-dependent branches?
- Field access patterns: constant-time indexing?
- Prime sieve: timing dependent on input size?

**Test:**
- Hash identical-length inputs with different content
- Measure execution time variance
- Statistical test: coefficient of variation < 1%

#### 9.2 Cache Timing
**Field Access Patterns:**
- Does coordinate movement reveal input structure?
- Are table lookups (if any) cache-friendly or exploitable?

**Mitigation:**
- Recommend constant-time field access
- Pre-load field into cache before processing

#### 9.3 Power/EM (Conceptual)
**If Implemented in Hardware:**
- Correlate power consumption with field values
- Recommend masking or shuffling techniques

**For Software:**
- Less critical, but document best practices

### Deliverables
- `results/timing_analysis_report.md`
- `docs/SIDE_CHANNEL_COUNTERMEASURES.md`

### Success Criteria
- ✓ Timing variance < 1% for same-length inputs
- ✓ Countermeasure recommendations documented

**Note:** This phase is lower priority for pure software use; critical for embedded/HW

---

## Phase 10: Formal Methods (Optional / Long-Term)

### Objectives
- Provide mathematical proofs of security properties
- Model algorithm in formal verification frameworks

### Tasks

#### 10.1 State Machine Formalization
**Tool:** TLA+ or Spin

**Model:**
- Initial state
- Transition functions (color ops)
- Finalization
- Invariants (no cycles, no fixed points)

**Verification:**
- Exhaustive state exploration (small instances)
- Liveness: eventually terminates
- Safety: no bad states reachable

#### 10.2 Proof-Carrying Code
**Tool:** Coq, Isabelle/HOL, Lean

**Theorems to Prove:**
- Collision resistance under ideal assumptions (e.g., random oracle for components)
- Avalanche property (formal definition)
- Preimage resistance bounds

**Deliverable:**
- Mechanized proof scripts
- Human-readable proof sketches

### Deliverables
- `formal/secasy.tla` (TLA+ specification)
- `formal/collision_resistance.v` (Coq proof)
- `docs/FORMAL_VERIFICATION.md`

### Success Criteria
- ✓ No invariant violations found
- ✓ At least one key property proven

**Note:** This is a research-level effort, not required for initial deployment but strengthens confidence

---

## Critical Go/No-Go Decision Points

### Checkpoint 1: After Phase 2 (Diffusion)
**Pass Criteria:**
- SAC matrix ≥95% in acceptable range
- No significant bit correlations

**Fail Action:**
- If <80%: **STOP** → Redesign finalization + mixing
- If 80-95%: Iterate with targeted improvements

### Checkpoint 2: After Phase 3 (Collisions)
**Pass Criteria:**
- All truncation levels within ±3σ
- Chi² tests pass

**Fail Action:**
- Systematic bias → Redesign field operations
- Chi² failure → Fix output formatting

### Checkpoint 3: After Phase 4 (Differential)
**Pass Criteria:**
- No high-probability differentials at 50%+ rounds
- Diffusion speed acceptable

**Fail Action:**
- Increase rounds by 50%
- Add nonlinear layer
- Reconsider color op design

### Checkpoint 4: After Phase 7 (Preimage)
**Pass Criteria:**
- State space adequate
- No inversion shortcuts

**Fail Action:**
- Expand internal state
- Strengthen finalization irreversibility

### Final Decision Matrix

| Metric | Weight | Pass Threshold | Current Status |
|--------|--------|----------------|----------------|
| SAC Coverage | 20% | ≥95% | **TBD** |
| Collision Behavior | 25% | All ±3σ | **TBD** |
| Differential Resistance | 20% | <2^-32 @ 50% | **TBD** |
| Linear Resistance | 15% | <2^-32 @ 50% | **TBD** |
| Preimage Security | 15% | >2^(O-8) | **TBD** |
| Statistical Quality | 5% | NIST 15/15 | **TBD** |

**Overall Grade:**
- **A (90-100%)**: Production-ready candidate, publish & peer-review
- **B (80-89%)**: Conditional use with documented limitations
- **C (70-79%)**: Research prototype, not for security-critical use
- **D/F (<70%)**: Fundamental redesign required

---

## Required Tooling & Infrastructure

### To Be Developed

#### 1. SAC Matrix Generator
**Extend:** `tests/avalanche/avalanche.c`

**New Flags:**
- `--sac-matrix <file.csv>`: Export full SAC matrix
- `--sac-threshold <value>`: Custom acceptance band (default [0.48, 0.52])
- `--sac-visual`: Generate heatmap (requires gnuplot or Python)

**Estimated Effort:** 3-5 days

#### 2. Differential Trail Searcher
**New Tool:** `tests/differential/differential.c`

**Features:**
- Configurable input differences (bitmask, byte-level, patterns)
- Reduced-round variants
- DDT export (CSV)
- Statistical summary (max prob, EDP, active S-boxes if applicable)

**Estimated Effort:** 1 week

#### 3. Linear Bias Sampler
**New Tool:** `tests/linear/linear_bias.c`

**Features:**
- Random mask sampling
- Focused low-weight search
- Walsh-Hadamard transform implementation
- Bias distribution histogram

**Estimated Effort:** 1 week

#### 4. Automated Regression Harness
**CI/CD Integration:**

```yaml
# .github/workflows/security_tests.yml
name: Security Regression Tests

on: [push, pull_request]

jobs:
  diffusion:
    runs-on: ubuntu-latest
    steps:
      - name: Build Secasy
        run: cmake --build build
      - name: Run SAC Test
        run: ./build/SecasyAvalanche --sac-matrix sac.csv -m 100 -l 64
      - name: Validate SAC
        run: python scripts/validate_sac.py sac.csv --threshold 0.95
        
  collision:
    runs-on: ubuntu-latest
    steps:
      - name: Truncated Collision Test (32-bit)
        run: ./build/SecasyCollision -m 1000000 -T 32 -s 42
      - name: Validate Results
        run: python scripts/validate_collisions.py collision.log
        
  # ... more test jobs
```

**Estimated Effort:** 2-3 days for CI setup

### External Tools Integration

#### NIST STS
**Download:** https://csrc.nist.gov/projects/random-bit-generation/documentation-and-software

**Wrapper Script:**
```bash
#!/bin/bash
# scripts/run_nist_sts.sh
SAMPLES=1000000
./generate_hashes.sh $SAMPLES > nist_input.bin
cd nist-sts-2.1.2
./assess $(($SAMPLES * 256 / 8)) < ../nist_input.bin
grep -A 5 "FINAL RESULT" experiments/AlgorithmTesting/*
```

#### Dieharder
**Install:**
```bash
sudo apt-get install dieharder  # Ubuntu/Debian
brew install dieharder          # macOS
```

**Wrapper:**
```bash
#!/bin/bash
# scripts/run_dieharder.sh
./generate_hashes.sh | dieharder -a -g 200 | tee dieharder_results.txt
```

#### CryptoMiniSat (Algebraic Testing)
**Install:**
```bash
pip install pycryptosat
```

**Test Script:**
```python
# scripts/algebraic_sat_test.py
from pycryptosat import Solver

# Build CNF from Secasy instance
# ...
s = Solver()
# Add clauses
# ...
sat, solution = s.solve()
print(f"SAT result: {sat}, Time: {s.time()}")
```

---

## Resource Requirements

### Personnel
- **Minimum:** 1 cryptographer/security analyst (full-time, 3 months)
- **Optimal:** 2-3 person team (parallel workstreams)

### Computational
- **Development Machine:** Multi-core (8+), 32GB RAM
- **Large-Scale Tests:** Cloud instance (64 cores, 256GB RAM) for 48-56 bit collision tests
- **Estimated Cloud Cost:** $500-2000 (spot instances)

### Timeline

#### Fast-Track (Minimal Viable Analysis)
- Phases 1-4, 7: **7 weeks**
- Assumes existing tooling can be quickly extended
- Risk: May miss subtle weaknesses

#### Comprehensive (Recommended)
- All phases 1-8: **12-16 weeks**
- Includes tool development, multiple iterations
- Higher confidence in results

#### Research-Grade (With Formal Methods)
- All phases including 9-10: **6-12 months**
- Peer-review, publication, conference presentation
- Suitable for academic/standards body submission

---

## Reporting & Documentation

### Interim Reports (Bi-Weekly)
- Phase completion summaries
- Test results with pass/fail indicators
- Issues discovered + mitigation steps

### Final Security Assessment Report
**Structure:**
1. Executive Summary
2. Methodology
3. Phase-by-Phase Results
4. Discovered Vulnerabilities (if any)
5. Security Claims (what can be asserted)
6. Limitations & Future Work
7. Recommendations (deployment, parameter tuning, etc.)

**Appendices:**
- Raw test data
- Tool documentation
- Proof sketches (if formal methods used)

### Public Disclosure
**If Weaknesses Found:**
- Responsible disclosure period (90 days)
- Coordinate with any downstream users
- CVE assignment if applicable

**If Passes:**
- Publish security analysis report
- Submit to conferences (e.g., CHES, FSE, Eurocrypt)
- Seek external peer review

---

## Maintenance & Ongoing Monitoring

### Regression Testing
- Add all security tests to CI/CD
- Any code change triggers full test suite
- Automated alerts on metric degradation

### Version Control
- Tag tested versions (e.g., `v1.0-security-audited`)
- Maintain changelog of security-relevant changes
- Require re-analysis for major modifications

### Community Engagement
- Bug bounty program (optional)
- Invite external cryptanalysis attempts
- Annual re-assessment

---

## References & Standards

### Cryptographic Hash Standards
- **NIST FIPS 180-4**: SHA-2 family
- **NIST FIPS 202**: SHA-3 (Keccak)
- **ISO/IEC 10118**: Hash function standards

### Testing Frameworks
- **NIST SP 800-22**: Statistical Test Suite
- **ECRYPT II**: eSTREAM testing methodology
- **CAESAR Competition**: Authenticated encryption testing (analogous rigor)

### Academic Literature
- **Handbook of Applied Cryptography** (Menezes et al.)
- **The Design of Rijndael** (Daemen & Rijmen) – design rationale example
- **SHA-3 Proposal Documentation** – comprehensive analysis template

### Attack Papers (Study for Context)
- **Differential Cryptanalysis** (Biham & Shamir)
- **Linear Cryptanalysis** (Matsui)
- **Algebraic Attacks on Stream Ciphers** (Courtois & Meier)

---

## Revision History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0 | 2025-10-02 | Initial comprehensive plan | Security Analysis Team |

---

## Appendix A: Test Data Volume Estimates

| Phase | Test Type | Data Generated | Storage |
|-------|-----------|----------------|---------|
| 2 | SAC Matrix (256B input) | ~500M hashes | ~128 GB |
| 3 | Collision (48-bit) | 20M hashes | ~5 GB |
| 4 | Differential (10^6 pairs) | 2M hashes | ~512 MB |
| 5 | Linear (10^6 samples) | 1M hashes | ~256 MB |
| 8 | NIST STS | 1M hashes | ~256 MB |
| **Total** | | | **~134 GB** |

**Note:** Most data ephemeral (generated → tested → discarded). Persistent storage for matrices, logs: ~10 GB.

---

## Appendix B: Quick-Start Checklist

**Week 1:**
- [ ] Review existing codebase
- [ ] Document full algorithm specification
- [ ] Identify all parameters (rounds, field size, etc.)
- [ ] Set up test infrastructure (build system, scripts)

**Week 2:**
- [ ] Implement SAC matrix tool
- [ ] Run first SAC test (64-byte input)
- [ ] Implement correlation analyzer
- [ ] Decision: Continue or redesign?

**Week 3-4:**
- [ ] Collision tests at 24, 28, 32, 36, 40 bits
- [ ] Statistical distribution tests (Chi²)
- [ ] Structured input class testing

**Week 5-6:**
- [ ] Differential tool development
- [ ] Reduced-round differential search
- [ ] Diffusion speed measurement

**Week 7:**
- [ ] Linear bias sampling
- [ ] Reduced-round LAT construction
- [ ] Checkpoint: Security margin assessment

**Week 8:**
- [ ] Algebraic degree analysis
- [ ] SAT solver probe (small instance)
- [ ] Decision: Fundamental design OK?

**Week 9:**
- [ ] Preimage/state-space analysis
- [ ] Finalization invertibility test
- [ ] MITM complexity estimate

**Week 10:**
- [ ] NIST STS execution
- [ ] Dieharder battery
- [ ] TestU01 (optional)

**Week 11-12:**
- [ ] Side-channel timing analysis
- [ ] Compile all results
- [ ] Write final report
- [ ] Go/no-go decision

---

**End of Security Analysis Plan**

*For questions or clarifications contact the security analysis team or repository maintainers.*
