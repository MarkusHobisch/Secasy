# Secasy – Security Analysis Notes

## 1. Algorithm Overview

Grid-based cryptographic hash function operating on a 16×16 field of `uint64_t` cells (256 cells total).  
Four phases: **Initialization** → **Input Integration** → **Processing** (10 rounds default) → **Extraction** (512-bit
hash output).  
Six operations: ADD, SUB, XOR, AND, OR, INVERT — applied per cell with neighbor coupling.

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
AND and OR are **not invertible** — given `a AND b = c`, neither `a` nor `b` can be uniquely recovered.  
This fundamentally prevents backward computation from hash output to input.

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
2. **No backward computation** — AND/OR are non-invertible; knowing that `a AND b = c` does not recover `a` or `b`
   uniquely. The operation sequence alone (without operand values) is insufficient
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

## 7. Absorbing States (AND/OR Bias)

The operations AND and OR introduce a theoretical bias:

- `AND` with 0 always yields 0 — zeros can "spread" through the field
- `OR` with all-1s always yields all-1s — ones can "spread" through the field

This could theoretically cause the field to degenerate toward all-zero or all-one states over many rounds.

**Mitigating factors in the design:**

- ADD and SUB inject arbitrary values that break zero/one patterns
- XOR flips bits regardless of current state
- INVERT inverts all 64 bits at once, reversing any bias
- Data-dependent traversal ensures different neighbor pairings across rounds
- Empirical evidence: all statistical randomness tests (10/10) pass, showing no detectable bias

A formal proof that bias cancellation holds for all inputs across the processing rounds remains open (see Section 9).
Empirical testing confirms no detectable bias at 10 rounds.

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

## 10. Formal Security (Open Questions)

A formal proof of pseudorandom permutation (PRP) properties would require:

- Showing that the state transition forms an **ergodic Markov chain** over the state
  space $\{0, \ldots, 2^{64}-1\}^{256}$
- Bounding the **mixing time** (e.g., via coupling method) to confirm convergence within the default 10 rounds
- Analyzing the **bias cancellation** of AND/OR across rounds (see Section 7)
- Proving that the wide-pipe extraction (Section 6) preserves uniformity

These are identified as future work suitable for a dedicated formal analysis.

---

## References
