# Secasy – Security Analysis Notes

## 1. Algorithm Overview

Grid-based cryptographic hash function operating on a 16×16 field of `uint64_t` cells (256 cells total).  
Four phases: **Initialization** → **Input Integration** → **Processing** (10 rounds default) → **Extraction** (512-bit
hash output).  
Six operations: ADD, SUB, XOR, RLX (Rotate-Left–XOR), RRA (Rotate-Right–Add), INVERT — applied per cell with neighbour coupling.

## 2. Empirical Security Results

| Property               | Result                            |
|------------------------|-----------------------------------|
| Avalanche Effect       | 49.99% bit-flip rate (ideal: 50%) |
| Collision Resistance   | 0 collisions in 5,000 tests       |
| Statistical Randomness | 10/10 tests passed                |
| Differential Analysis  | 5/5 tests passed                  |
| Extended Security      | 5/5 tests passed                  |

## 3. Key Design Properties

### 3.1 Data-Dependent Traversal

During input integration, the traversal order is determined by field values themselves. The jump distance to the next
cell depends on the current tile's value and the direction:

```c
const uint64_t oldPrime = tile->value;
// Direction-dependent jump (SAV added only to DOWN and RIGHT):
switch (direction) {
    case UP:    pos.y = (pos.y - oldPrime) & FIELD_SIZE_MASK;                           break;
    case DOWN:  pos.y = (pos.y + oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;  break;
    case LEFT:  pos.x = (pos.x - oldPrime) & FIELD_SIZE_MASK;                           break;
    case RIGHT: pos.x = (pos.x + oldPrime + SQUARE_AVOIDANCE_VALUE) & FIELD_SIZE_MASK;  break;
}
```

Each step's jump distance depends on the *current* cell value, creating a feedback loop. The SAV offset (+1) on
DOWN/RIGHT breaks the symmetry between opposite directions on the same axis.
### 3.2 ARX Non-Linearity

The rotation-based operations RLX (`ROL64(v,13) ^ neighbour`) and RRA (`ROR64(v,7) + neighbour`) are **bijective** on
$\{0,\ldots,2^{64}-1\}$ — they preserve entropy by not mapping any values to an absorbing fixed point. At the same
time they introduce **non-linearity**: the carry chain of modular addition and the bit-rotation couple bits in a way
that is non-linear with respect to individual bit positions. This prevents simple linear approximations and
back-computation from hash output to internal state.

### 3.3 Cross-Position Mixing

The operation applied to cell $(x,y)$ is determined by the `colorIndex` of a *different* cell at an offset position.  
This creates inter-cell dependencies that prevent isolated analysis of individual cells.

## 4. Side-Channel Resistance

### 4.1 Classical Side-Channel Attack Vectors

**Timing Attacks:** An attacker measures execution time differences to infer internal state. In Secasy, the
`switch(colorIndex)` statement selects one of 6 operations per cell. Branch prediction misses and varying instruction
latencies could leak which operation was executed [@kocher1996_timing].

**Power / EM Analysis:** Each arithmetic operation causes a measurable difference in power consumption proportional to
the Hamming distance between old and new register values. Differential Power Analysis (DPA) [@kocher1999_dpa] correlates power traces
across many executions to statistically recover which operation was applied at each cell.

**Cache-Timing Attacks (Flush+Reload / Prime+Probe):** The prime table (`primes.h`, 88,801 entries, ~355 KB) exceeds L1
cache size. Indexed access via `primeArray[primeIndex]` causes cache misses whose pattern depends on `primeIndex`. An
attacker sharing the same CPU (e.g., co-located cloud VM) can observe which cache lines are loaded and deduce the
accessed prime indices — revealing the traversal order. This is the same class of attack used to extract AES keys from
T-Table implementations [@bernstein2005_cache; @osvik2006_cache].

### 4.2 Inherent Resistance by Design

Even if an attacker successfully leaks the full operation sequence and traversal order via the above channels,
reconstruction of a secret input remains infeasible because:

1. **No forward simulation** — without knowing the secret input, the attacker cannot compute field values, so leaked
   information cannot be anchored to concrete data
2. **No backward computation** — the ARX operations (RLX, RRA) are bijective but their non-linear carry propagation
   makes algebraic inversion computationally infeasible; the operation sequence alone (without operand values) is
   insufficient to reconstruct the input
3. **Data-dependent traversal** — the next position depends on the current cell value after modification, creating a
   feedback loop that cannot be replayed without full state knowledge
4. **Cascading dependencies** — 10 rounds × 256 cells = 2,560 chained operations with mutual feedback across the entire
   field (empirically confirmed to be sufficient — see round-reduction analysis)

This stands in contrast to designs like AES, where the structure is fixed and all operations are invertible — making
side-channel leaks directly exploitable.

### 4.3 Implementation Considerations

The current implementation is **not** constant-time:

- `switch(colorIndex)` introduces data-dependent branching (timing leak)
- `primeArray[primeIndex]` causes data-dependent cache access patterns (cache-timing leak)

This is acceptable for pure hashing (no secret input). For use as HMAC or key derivation primitive, a constant-time
variant would be required — e.g., replacing the `switch` with a branchless lookup table and masking prime table
accesses. This is left as future work.

## 5. Length Extension Resistance

In Merkle-Damgård constructions [@merkle1990; @damgard1990] (MD5, SHA-1, SHA-256), an attacker who knows $H(m)$ can
compute $H(m \| padding \| m')$ without knowing $m$. This is possible because the hash output *is* the internal state.

Secasy is **inherently immune** to this attack: the internal state is 256 × 64 = 16,384 bits, while the output is only
512 bits. The hash is a lossy XOR-accumulation of the full field — the attacker cannot reconstruct the remaining 15,872
bits of internal state from the output alone.

## 6. Wide-Pipe Advantage

The ratio of internal state to output size provides a strong security margin:

| Hash Function | Internal State | Output      | Ratio    |
|---------------|----------------|-------------|----------|
| SHA-256 [@nist_fips180_4]   | 256 bit        | 256 bit     | 1:1      |
| SHA-512 [@nist_fips180_4]   | 512 bit        | 512 bit     | 1:1      |
| SHA-3-256 [@nist_fips202] | 1,600 bit      | 256 bit     | 6.25:1   |
| **Secasy**    | **16,384 bit** | **512 bit** | **32:1** |

This 32:1 ratio means that even with full knowledge of the hash output, an attacker has access to only ~3% of the
internal state. This makes both collision-finding and preimage attacks significantly harder than the output size alone
would suggest.

## 7. Absorbing States — Historical Note (Resolved)

The original Phase 3 operations included bitwise AND and OR, which introduced a structural bias:

- `AND` with 0 always yields 0 — zeros could spread through the field over repeated rounds
- `OR` with all-1s always yields all-1s — ones could spread similarly

Empirical analysis confirmed the problem: after 10 processing rounds, only 110 of 256 grid cells retained distinct
values under the AND/OR design. In version 2025-06, AND and OR were replaced by the rotation-based ARX operations
**RLX** (`ROL64(v,13) ^ neighbour`) and **RRA** (`ROR64(v,7) + neighbour`). Both are bijective on $\{0,\ldots,2^{64}-1\}$
and therefore **free of absorbing fixed points** — they cannot drive cell values toward 0 or $2^{64}-1$ under any input.
Post-migration, 256/256 grid cells retain distinct values after processing (see Appendix D of ALGORITHM.md).

**Status: closed** — the bias concern is resolved by design. No formal proof of entropy preservation per round is
necessary for ARX operations because bijectivity guarantees that the full $2^{64}$-valued domain is preserved.

## 8. Quantum Resistance

Quantum algorithms reduce the effective security of hash functions:

- **Preimage (Grover [@grover1996]):** $2^{512} \rightarrow 2^{256}$ quantum operations — well beyond feasibility
- **Collision (BHT algorithm [@brassard1998_quantum]):** $2^{256} \rightarrow 2^{170}$ quantum operations — still considered secure

Note: For collisions, the Brassard-Høyer-Tapp (BHT) algorithm [@brassard1998_quantum] achieves $2^{n/3}$ complexity, not $2^{n/2}$ as Grover
does for preimage.

Secasy's 512-bit output provides a **256-bit post-quantum preimage security level** and **170-bit post-quantum collision
security level**, exceeding the quantum resistance of SHA-3-256 (128-bit post-quantum preimage, ~85-bit post-quantum
collision).

## 9. Degenerate Input Behavior

Pathological inputs deserve consideration:

- **Empty input (0 bytes):** No field modification occurs beyond initialization. The hash is deterministic and unique,
  but derived from the default field state (all cells = 2). This is by design — no padding is applied.
- **All-zero input (e.g., 1000× `0x00`):** Byte `0x00` decodes to direction `00 00 00 00` (4× UP). Repeated identical
  directions could produce suboptimal field distribution. However, the prime-based jump distances ensure that even
  uniform directions produce distinct cell visits, since each visit changes `primeIndex` and thus the next jump distance.
- **Very short input (1 byte):** Only 4 traversal moves. The field remains close to its initial state, but the
  processing rounds (default: 10) still provide sufficient diffusion — confirmed by round-reduction analysis down to 1
  round.

These cases are not security vulnerabilities — the hash remains deterministic and collision-free. Empirical validation
of avalanche properties for edge-case inputs is recommended.

## 9a. Exhaustive Short-Input Collision Enumeration

To probe collision behaviour beyond random sampling, an exhaustive enumeration of all 1-, 2- and 3-byte inputs was
carried out (`tests/analysis/brute_collision_scan.c`). The first 64-bit block of the 512-bit default digest was
retained for each input, and collisions were detected at three truncation widths.

**Configuration:** default parameters ($r = 10$, 512-bit output, prime table $1.6 \times 10^7$); single-threaded
Windows / MinGW-w64 GCC 15; the $L = 3$ pass completes in $\approx 122$ s wall-clock.

**Results.** $N_1 = 256$, $N_2 = 65{,}536$, $N_3 = 16{,}777{,}216$, total $N = 16{,}843{,}008$:

| Length | 64-bit collisions | 48-bit collisions | 32-bit collisions | Ideal 32-bit ($E$) |
|--------|------------------:|------------------:|------------------:|-------------------:|
| 1      | 0                 | 0                 | 0                 | $0.0$              |
| 2      | 0                 | 0                 | 0                 | $0.5$              |
| 3      | 0                 | 0                 | **32,869**        | $32{,}768$         |

The $L = 3$ 32-bit collision count deviates from the ideal birthday expectation by $+0.56\,\sigma$, which is
unremarkable and consistent with ideal-random behaviour. **No cross-length collisions** were observed between $L = 1$,
$L = 2$ and $L = 3$ inputs at any truncation width.

**What this does and does not show.** Within the input range fully accessible to enumeration on commodity hardware, the
construction is empirically indistinguishable from a random oracle on the collision metric. It does **not** address
differential, algebraic, or longer-input collision attacks, all of which require dedicated techniques
[@biham1991_differential; @matsui1994_linear; @wang2005_sha1].

## 10. Formal Security (Open Questions)

A formal proof of pseudorandom permutation (PRP) properties would require:

- Showing that the state transition forms an **ergodic Markov chain** over the state
  space $\{0, \ldots, 2^{64}-1\}^{256}$
- Bounding the **mixing time** (e.g., via coupling method) to confirm convergence within the default 10 rounds
- Bounding the **mixing per round** of the ARX operations (RLX, RRA) — a formal coupling argument or chi-squared
  convergence bound for the 16×16-cell state graph would quantify how many rounds guarantee near-uniform diffusion
- Proving that the wide-pipe extraction (Section 6) preserves uniformity

These are identified as future work suitable for a dedicated formal analysis.

---

## References
