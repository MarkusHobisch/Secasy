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

**Configuration:** default parameters ($r = 10$, 512-bit output, prime table $88{,}801$ entries); single-threaded
Windows / MinGW-w64 GCC 15; the $L = 3$ pass completes in $\approx 122$ s wall-clock.

**Results.** $N_1 = 256$, $N_2 = 65{,}536$, $N_3 = 16{,}777{,}216$, total $N = 16{,}843{,}008$:

| Length | 64-bit collisions | 48-bit collisions | 32-bit collisions | Ideal 32-bit ($E$) |
|--------|------------------:|------------------:|------------------:|-------------------:|
| 1      | 0                 | 0                 | 0                 | $0.0$              |
| 2      | 0                 | 0                 | 1                 | $0.5$              |
| 3      | 0                 | 0                 | **32,638**        | $32{,}768$         |

The $L = 3$ 32-bit collision count deviates from the ideal birthday expectation by $-0.72\,\sigma$ (standard
deviation $\sqrt{E} \approx 181$), which is
unremarkable and consistent with ideal-random behaviour. **No cross-length collisions** were observed between $L = 1$,
$L = 2$ and $L = 3$ inputs at any truncation width.

**What this does and does not show.** Within the input range fully accessible to enumeration on commodity hardware, the
construction is empirically indistinguishable from a random oracle on the collision metric. It does **not** address
differential, algebraic, or longer-input collision attacks, all of which require dedicated techniques
[@biham1991_differential; @matsui1994_linear; @wang2005_sha1].

## 9b. Phase-2 Internal-State Collision Enumeration

The exhaustive scan of Section 9a operates on the final 512-bit digest and therefore cannot localise *where* in the
construction a collision originates. To isolate the dominant structural collision source, a second exhaustive
enumeration was carried out directly on the internal state after the input-absorption phase
(`tests/analysis/phase2_collision_scan.c`).

**Motivation.** The construction comprises four phases: (1) state initialisation, (2) a lossy input-driven cursor walk,
(3) a round-based mixing permutation, and (4) digest extraction. Phase 3 was independently shown to be a bijection on
the cell-value state (rank $256/256$ over the visited-cell transformation; see Section on structural analysis), and
Phase 4 reads only the cell values. Consequently, any collision in the full digest must already be present as a
collision in the *collision-relevant Phase-2 state*. Phase 2 is the natural suspect because the previous cell value
enters the cursor update only through a reduction modulo 16 — discarding the upper 60 bits of a 64-bit word — before
being overwritten by the next prime. This information loss admits, in principle, *neutral blocks* (an input whose
Phase-2 state equals the canonical initial state, hence colliding with the empty input) or *path cycles* (two distinct
inputs converging to the same internal state).

**Method.** For each input, the collision-relevant Phase-2 state was fingerprinted as the concatenation of all 256 cell
values ($256 \times 8$ bytes), all 256 colour indices ($256 \times 1$ byte) and the cursor coordinates ($2 \times 4$
bytes), totalling $2{,}312$ bytes. The `primeIndex` field was deliberately **excluded** from the fingerprint, since it
is not read after Phase 2; including it would only mask collisions that are real with respect to the downstream
computation, so its exclusion yields a strictly more sensitive (conservative) test. Candidate collisions flagged by a
128-bit fingerprint were confirmed by recomputing both full $2{,}312$-byte states and comparing them byte-for-byte,
eliminating false positives.

**Configuration.** Default parameters ($r = 10$, 512-bit output, prime table $88{,}801$ entries); single-threaded
Windows / MinGW-w64 GCC 15; the $L = 3$ pass completes in $\approx 137$ s wall-clock.

**Results.** All $N = 16{,}843{,}008$ inputs of length $L \in \{1, 2, 3\}$ were enumerated:

| Length | Inputs       | Confirmed Phase-2 state collisions | Neutral-from-init |
|--------|-------------:|-----------------------------------:|------------------:|
| 1      | $256$        | $0$                                | $0$               |
| 2      | $65{,}536$   | $0$                                | $0$               |
| 3      | $16{,}777{,}216$ | $0$                            | $0$               |

No internal-state collision and no neutral block were observed: the lossy input walk is **empirically injective** over
the entire enumerated domain. This is the expected complement to the Section 9a digest-level result — had a Phase-2
collision existed, it would necessarily have surfaced as a full-digest collision.

**What this does and does not show.** The result provides empirical evidence that, despite the deliberate information
loss in the cursor update, the prime- and colour-driven schedule restores injectivity for all short inputs, so no
trivial neutral-block or path-collision attack exists in this range. It is **not** a proof of collision resistance: the
enumerable domain ($L \le 3$) is negligible against the full message space, and the test does not rule out internal
collisions for longer inputs, which would require differential or meet-in-the-middle techniques
[@biham1991_differential; @aoki2009_mitm].

## 10. Formal Security (Open Questions)

The analyses in this document are **empirical**: they establish behaviour over finite, sampled, or exhaustively
enumerated input domains. This section delineates precisely (i) what the empirical evidence does and does not establish,
(ii) the limitations that are intrinsic to the empirical method and therefore cannot be closed by any amount of
additional testing, and (iii) the concrete formal results that would close those gaps. It is intended as an explicit
hand-off to a cryptanalyst or formal-methods reviewer.

### 10.1 Boundary between tested and untested

The following table separates claims supported by direct measurement from claims that remain conjectural. The middle
column states the *empirical scope* — the exact domain over which evidence exists — to make the boundary unambiguous.

| Property | Empirical scope (what was actually checked) | Formal status |
|----------|----------------------------------------------|---------------|
| Phase-2 internal injectivity | Exhaustive over all $L \le 3$ inputs ($1.68 \times 10^7$); zero collisions (§9b) | **Conjectured** for $L > 3$; no proof |
| Phase-3 bijectivity | $\mathrm{GF}(2)$ rank $256/256$ on the visited-cell map, incl. the fully-affine worst case (§ structural analysis) | **Proven** for the measured instances; not yet proven for *all* colour layouts in closed form |
| Digest collision rate | Exhaustive $L \le 3$; matches birthday expectation to within $1\sigma$ (§9a) | **Conjectured** to hold at full $2^{256}$ scale; untested there |
| Avalanche / first-order diffusion | $\sim 10^5$–$10^6$ random single-bit flips; mean $\approx 50\%$, min $\approx 39.6\%$ (M2–M4) | **No bound**: worst case over *all* inputs unknown |
| Differential resistance | Random/internally-guided search over $> 10^6$ pairs; no high-probability characteristic found | **No bound**: existence of a hand-constructed trail not excluded |
| Statistical randomness | NIST-style battery on $10^5$–$10^6$ digests; all tests pass at $\alpha = 0.01$ | Distinguisher-freedom **not** implied by test passage |

### 10.2 Intrinsic limitations of the empirical method

The following gaps **cannot** be closed by more testing, larger samples, or longer runs — they are categorical, not
quantitative. They are listed so a reviewer need not re-derive them.

1. **The sampled domain is negligible.** All exhaustive results are confined to $L \le 3$ (i.e. $\le 2^{24}$ inputs)
   against a message space of unbounded size. Birthday-bound collision search at the full 512-bit width
   ($\approx 2^{256}$ work) is computationally unreachable, so the random-oracle hypothesis is tested only far below the
   security parameter.
2. **A single adversarial trail beats any sample.** Differential and linear cryptanalysis succeed by exhibiting *one*
   characteristic of anomalously high probability [@biham1991_differential; @matsui1994_linear]. Such a trail can be
   constructed analytically (as in the SHA-1 break [@wang2005_sha1]) in a region that random or heuristically-guided
   sampling will, with overwhelming probability, never visit. Absence of a trail in $> 10^6$ samples is evidence, not
   proof, of its non-existence.
3. **Statistical-test passage is necessary, not sufficient.** Passing NIST SP 800-22 [@bassham2010_sp800_22] excludes
   gross non-randomness but says nothing about algebraic or structural distinguishers operating below the tests'
   resolution.
4. **Worst-case diffusion is unquantified.** Measured avalanche reports the *average* (and a sampled minimum) behaviour;
   it provides no proven *lower bound* over all inputs, which is what a security argument requires.

### 10.3 Next logical steps (formal work items)

The following are the concrete results a formal analysis would need to establish, ordered roughly from most tractable to
most demanding. Each is phrased as a target statement so it can be picked up directly.

1. **Phase-3 bijectivity in closed form.** Prove that the Phase-3 round map is a bijection on the $16{,}384$-bit state
   for *every* admissible colour-index layout — not only the measured instances — e.g. by showing each colour operation
   is invertible and their per-round composition has odd determinant over $\mathrm{GF}(2)$. This upgrades the §9b/M6
   measurement to a theorem.
2. **Per-round differential branch number.** Derive an upper bound $p_{\max}$ on the probability of any non-trivial
   single-round differential characteristic of the ARX operations (`ROTATE_LEFT_XOR`, `ROTATE_RIGHT_ADD`) over the
   16×16 cell graph. The maximum-probability $r$-round trail is then bounded by $p_{\max}^{\,r}$; showing
   $p_{\max}^{10} \ll 2^{-512}$ would rule out the differential attack that the empirical search could only probe.
3. **Linear-approximation bound.** The dual of (2): bound the maximum absolute correlation of any single-round linear
   approximation and apply the piling-up lemma [@matsui1994_linear] across the 10 rounds.
4. **Mixing-time / diffusion lower bound.** Model the state transition as a Markov chain over
   $\{0,\dots,2^{64}-1\}^{256}$ and bound its mixing time (e.g. via a coupling argument or a spectral-gap / chi-squared
   convergence bound on the 16×16 state graph) to prove that 10 rounds suffice for near-uniform diffusion — converting
   the measured $\approx 50\%$ avalanche into a proven worst-case lower bound.
5. **Extraction-stage uniformity.** Prove that the wide-pipe Phase-4 extractor (§6) maps the near-uniform internal state
   to a near-uniform 512-bit digest without introducing exploitable bias, and characterise its collision kernel beyond
   the empirical "0 dead bits" finding (M7).
6. **Meet-in-the-middle / preimage resistance.** Assess whether the known-schedule structure admits a
   meet-in-the-middle preimage strategy [@aoki2009_mitm] faster than $2^{512}$, given that the round schedule is derived
   from (and therefore known with) the message.

Establishing (1)–(5) would constitute a pseudorandom-permutation argument for the core; (6) addresses one-wayness. Until
then, all security claims in this document remain **empirical conjectures**, and the construction must be treated as
unproven.

---

## References
