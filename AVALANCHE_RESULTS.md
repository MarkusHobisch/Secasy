## Secasy Hash – Avalanche Summary
Date: 2025-09-27 (condensed overview)

Goal: Check whether single and small multi-bit input changes flip roughly half of the output bits (target probability ≈ 0.5) and reveal any obvious positional bias.

### Test Runs (Extended Mode `-X`)
| Run | Key Params | Purpose |
|-----|------------|---------|
| All Bits | `-m 4 -l 64 -B 0 -r 8000` | Exhaustive single‑bit flips (highest precision per bit) |
| Bias Sample | `-m 20 -l 64 -B 64 -r 10000` | Higher sampled density; stabilize global mean |
| Large Input | `-m 8 -l 256 -B 32 -r 12000` | Observe scaling with bigger messages |

### Core Metrics
| Metric | Range Observed |
|--------|----------------|
| Mean avalanche rate | 0.4988 – 0.5004 (ideal 0.5) |
| Bits outside [0.45,0.55] | 1–4 (dense runs) / 72 (sparse large-input) |
| Multi-bit (k=2,4,8) means | All within ±0.002 of 0.5 |
| Entropy (byte sampler) | 0.0 (instrumentation flaw, ignore) |

### Interpretation
Diffusion is strong: global and per-bit behavior (when sufficiently sampled) matches the expected ~50% flip rate; no stable positional bias detected. Multi-bit perturbations preserve the same rate, suggesting adequate nonlinear mixing depth at tested round counts.

### Known Gaps / Pending Work
* Entropy measurement logic faulty (placeholder values)
* No SAC matrix, correlation tests, or higher-order statistical independence yet
* No structured/adversarial input classes evaluated
* Collision-focused statistical campaigns tracked separately (see README collision section)

### Interim Security Assessment (Non-Final)
The observed avalanche characteristics are consistent with a healthy diffusion core. However, this alone does not establish cryptographic security. The hash must still undergo: collision resistance probing at full bit lengths, preimage analysis considerations, strict avalanche criterion matrix computation, correlation and linear/differential resistance studies. Until those are complete, the function remains experimental and MUST NOT be relied upon for real security use cases.

### Next Analytical Priorities
1. Repair entropy collector (raw binary sampling, minimum counts)
2. Generate SAC matrix (input bit → output bit probabilities)
3. Add pairwise bit correlation & Chi² acceptance checks
4. Structured / adversarial input battery (low weight, patterned, incremental)
5. Optional export (CSV/JSON) for regression tracking

For usage instructions and extended flag descriptions see the README (Avalanche Test Tool section).

*End (concise report)*
