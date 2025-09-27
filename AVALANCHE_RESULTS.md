# Secasy Hash – Avalanche & Extended Analysis Report

Date: 2025-09-27

## 1. Objective
Evaluate the diffusion (avalanche effect) and related statistical behavior of the Secasy hash function under different flip regimes, input sizes, and analysis extensions (-X mode).

The avalanche effect goal for a strong hash: flipping a single input bit should flip each output bit with probability ≈ 0.5, independently and uniformly.

## 2. Test Set Overview
Three focused runs were executed after implementing extended diagnostics (`-X`):

| Label | Parameters | Purpose |
|-------|------------|---------|
| Bias Test | `-X -m 20 -l 64 -B 64 -r 10000 -s 55` | Higher sampling per message to stabilize per-bit flip statistics |
| All Bits | `-X -m 4 -l 64 -B 0 -r 8000 -s 77` | Exhaustive single-bit flips (all bits of each message) |
| Entropy Retest | `-X -m 8 -l 256 -B 32 -r 12000 -s 909` | Larger input size to observe diffusion scaling & entropy collection |

Parameter meanings:
- `-m` messages (random base inputs)
- `-l` input length in bytes
- `-B` bit flips per message (0 = exhaustively flip every bit exactly once)
- `-r` internal rounds of the hash core
- `-n` internal output buffer size (default 512 chars here)
- `-s` deterministic RNG seed
- `-X` extended analysis (per-bit stats, entropy, multi-bit trials)

## 3. Key Numerical Results

| Metric | Bias Test | All Bits | Entropy Retest |
|--------|-----------|----------|----------------|
| Total flips | 1280 | 2048 | 256 |
| Mean avalanche rate | 0.500130 | 0.500409 | 0.498802 |
| 95% CI (global p) | [0.498919, 0.501340] | [0.499452, 0.501366] | [0.496095, 0.501509] |
| Stddev per-flip ratio | 0.022160 | 0.021705 | 0.020160 |
| Per-bit SD (bias set) | 0.0156 | 0.0123 | 0.0324 |
| Bits outside [0.45,0.55] | 4 / 512 | 1 / 512 | 72 / 512 |
| Multi-bit k=2 mean | 0.499148 | 0.498932 | 0.499214 |
| Multi-bit k=4 mean | 0.499844 | 0.502090 | 0.498695 |
| Multi-bit k=8 mean | 0.500958 | 0.501419 | 0.500343 |
| Reported entropy | 0.0000 | 0.0000 | 0.0000 |

## 4. Interpretation of Results
### 4.1 Global Avalanche Rate
All runs produce a global avalanche fraction extremely close to the ideal 0.5. Confidence intervals are narrow, indicating statistical stability. This is a strong indication that single-bit input perturbations propagate widely and proportionally through the hash state.

### 4.2 Per-Bit Flip Distribution
- All Bits run (exhaustive) shows very tight dispersion: only 1 bit outside the 0.45–0.55 acceptance band; SD ≈ 0.0123.
- Bias Test (larger sample than baseline) also behaves well (only 4 bits outside band; SD ≈ 0.0156).
- Entropy Retest has broader dispersion (72 bits outside band; SD ≈ 0.0324) because each output bit received fewer comparative observations (total flips were much lower relative to input/output size). This is consistent with sampling noise, not necessarily structural weakness.

Conclusion: No evidence of systematic per-bit bias. Outliers in sparse runs reflect insufficient sampling density.

### 4.3 Multi-Bit Flip Diffusion (k = 2,4,8)
Diffusion under simultaneous multiple random bit flips remains centered near 0.5 for all tested k values. This implies the transformation layers do not saturate or collapse under small multi-bit perturbations and maintain consistent mixing depth.

### 4.4 Entropy (Observed 0.0000)
The entropy collector (current implementation) reported 0.0000 in all runs. Based on methodology review, this does not indicate actual degeneracy of the hash output, but rather a limitation in how entropy sampling was implemented:
- Both original and modified hash outputs were counted; potential duplication of constant leading regions.
- Possible dominance of a single byte value early in measurement due to normalization/padding.
- Sampling strategy needs refinement (e.g., only count modified hash bytes, ignore zero-length segments, enforce minimum byte sample size, or capture raw binary rather than hex-derived bytes).

Action: Treat entropy rows as “placeholder” until the collection logic is revised.

### 4.5 Stability vs. Parameters
Changing from sampled flips to exhaustive flips improves per-bit precision as expected. Increasing input length (l=256) without a proportional increase in total flips reduces reliability of per-bit statistics (wider SD) but leaves global avalanche rate intact. Overall diffusion appears parameter-stable across tested regimes.

## 5. What This Says About the Hash Function
| Property | Observation | Implication |
|----------|-------------|------------|
| Diffusion (single-bit) | Mean ≈ 0.5 | Meets core avalanche expectation |
| Output bit uniformity | Per-bit rates cluster near 0.5 (sufficient flips) | No strong positional bias detected |
| Multi-bit resilience | k=2–8 ratios ≈ 0.5 | Nonlinear layers likely distribute combined perturbations effectively |
| Parameter robustness | Different flip strategies & message sizes stable | Mixing not narrowly tuned to specific sizes |
| Entropy (current metric) | 0.0 (artifact) | Needs instrumentation fix; no conclusion yet |

Overall: The hash exhibits strong first-order avalanche characteristics under the tested scenarios. This supports—but does not prove—sound diffusion design. Further higher-order and structural analyses (independence, correlation, linear approximation, collision surface probing) are required for cryptographic assurance.

## 6. Limitations of Current Assessment
- No Strict Avalanche Criterion (SAC) matrix (input-bit ↔ output-bit independence) computed.
- No pairwise or higher-order output bit correlation analysis.
- Entropy module inconclusive (implementation artifact).
- No collision / near-collision empirical campaign executed.
- No differential characteristic search / linear hull analysis.

## 7. Recommended Next Steps
1. Fix entropy collection (count only modified hash bytes; verify diversity; add minimum sample threshold).
2. Implement SAC table: matrix of flip probabilities per (input_bit, output_bit).
3. Add pairwise correlation and Chi² test vs. Binomial(n=bitComparisons, p=0.5).
4. Introduce CSV/JSON export for automated regression.
5. Add multi-bit k parameterization (`-K 2,4,8,16`).
6. Sample structured inputs (all-zero, all-one, low Hamming weight) to detect asymmetries.
7. Fuzz adversarial patterns (repetitive blocks, incremental counters) to uncover structural dependencies.

## 8. High-Level Conclusion
The Secasy hash function demonstrates robust avalanche diffusion across multiple regimes in preliminary empirical testing. No immediate red flags emerged in single- or multi-bit diffusion. The current entropy metric must be improved before drawing conclusions about global output distribution. The function merits deeper second-order and statistical independence analysis to progress toward cryptographic confidence.

---
*End of Report*
