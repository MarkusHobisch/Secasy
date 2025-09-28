# Secasy Hash Project Status

Last Updated: 2025-09-28

## 1. Overview
Experimental cryptographic hash design using a 2D field evolution and mixed arithmetic/bitwise "color" operations with a strengthened finalization layer producing 128-bit (currently) hex output. Goal: Evaluate whether the construction can progress toward a modern secure hash primitive.

## 2. Current Maturity Level
Status: Research Prototype (Pre-Alpha Cryptography)
Not suitable for production, security boundaries, authentication, integrity, password storage, signatures, or blockchain use.

## 3. Positive Empirical Signals
- First-order avalanche: Mean flip rate ≈ 0.5 across tested regimes
- Per-bit diffusion: No persistent positional bias in sufficiently sampled runs
- Multi-bit perturbations (k=2,4,8) retain ≈ 0.5 diffusion ratio
- Finalization hardening removed earlier leading nibble bias
- Tooling: Collision sweep, avalanche extended mode, distribution probes in place

## 4. Known Gaps / Deficiencies
- No Strict Avalanche Criterion (SAC) matrix (input bit → output bit probabilities)
- No bit/byte correlation matrix or higher-order statistical independence tests
- No differential / linear trail exploration (reduced-round analysis missing)
- Entropy sampler currently unreliable (reports 0.0 placeholder)
- No large-scale structured/adversarial input campaigns (low Hamming weight, repeated blocks, counters)
- No preimage / second-preimage complexity discussion or internal state entropy audit
- No side-channel or timing uniformity considerations
- No formal specification (state transitions, invariants, domain separation) document

## 5. Risk Assessment (Concise)
| Area | Current Risk | Notes |
|------|--------------|-------|
| Collision Resistance | Unknown | Only truncated-space empirical checks done |
| Preimage Resistance | Unknown | State vs. output entropy not audited |
| Second Preimage | Unknown | Same as preimage; structural analysis absent |
| Diffusion (1st order) | Moderate Positive | Good empirical avalanche behavior |
| Diffusion (higher order) | Unvalidated | No SAC / correlation yet |
| Differential / Linear | Unvalidated | No trail search performed |
| Output Uniformity | Partially Probed | Hex freq OK; entropy tool broken |
| Implementation Robustness | Moderate | Simple C code; needs spec + tests |

## 6. Interim Security Position
The hash shows promising diffusion but lacks every other major cryptanalytic validation step. Treat as insecure. No security guarantees should be inferred from observed avalanche metrics alone.

## 7. Immediate Roadmap (Near-Term)
1. Repair entropy sampling (raw bytes; enforce min sample counts)
2. Generate SAC matrix with confidence intervals + threshold alerts
3. Pairwise bit correlation / mutual information + Chi² gating
4. Structured input battery (all-zero, all-one, low weight, patterned, incremental)
5. JSON/CSV export for automated regression & CI gating

## 8. Mid-Term Objectives
6. Differential characteristic search (reduced rounds) and probability decay study
7. Linear approximation / bias scan (Walsh-Hadamard or correlation matrix)
8. Round count sensitivity analysis (find minimum safe diffusion depth)
9. Formal specification document (state, operations, finalization, parameter semantics)
10. Distinguish & enforce target output length vs. internal buffer capacity

## 9. Stretch Goals
11. Multi-threaded test harness & large sample campaigns
12. Probabilistic collision surface probing (random walk / hill-climb attempts)
13. Side-channel (timing/memory access uniformity) hardening guidance
14. Fuzzing for structural cycles / fixed points
15. Optional domain separation tags & versioned personalization

## 10. Usage Policy (Enforced)
Experimental only. DO NOT deploy for any security purpose. All contributions must keep documentation. Security claims without empirical + analytical evidence will be rejected.

## 11. Decommission Criteria
Project should be reconsidered or redesigned if:
- Stable positional or differential biases persist after remediation
- High-probability reduced-round trails fail to decay exponentially
- Structural cycles or linear leakage remain unresolved

## 12. Success Criteria for Advancing to "Candidate"
- SAC matrix within tolerance (no systemic deviations)
- No anomalous correlations beyond statistical noise thresholds
- Differential probabilities decay ~exponentially with rounds
- Fixed, audited entropy sampler shows near‑uniform output distribution
- Reproducible regression suite with automated gating green

## 13. Changelog (Analytical Evolution - Placeholder)
- 2025-09-27: Extended avalanche mode & collision sweep instrumentation
- 2025-09-28: Documentation consolidation; status file introduced

---
*End of STATUS.md*
