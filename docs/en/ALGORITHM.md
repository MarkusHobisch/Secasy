\newpage

# Secasy — Algorithm Description

## Table of Contents

1. Motivation — Why a New Hash Algorithm?
2. The Core Concept — Grid-Based State
3. Data Structure in Detail
4. Phase 1 — Initialization
5. Phase 2 — Input Integration (Fingerprinting)
6. Phase 3 — Processing Rounds (Diffusion)
7. Phase 4 — Hash Extraction
8. Security Arguments
9. Comparison with Established Constructions
10. Known Limitations and Open Questions
11. References

Appendix A — Empirical Isolation of Individual Colour Operations

Appendix B — Impact of Field Size on Diffusion

---

\newpage

## 1. Motivation — Why a New Hash Algorithm?

### The Dominant Paradigm: Merkle-Damgård

The most widely known cryptographic hash functions — MD5, SHA-1, SHA-256,
SHA-512 — are all built on the **Merkle-Damgård construction** from 1989 [@merkle1990; @damgard1990].
The principle is straightforward: the input is split into equal-sized blocks,
which are processed sequentially through a compression function. The output
state after the final block is the hash.

This linear processing chain has a well-known structural weakness: the
**length extension attack**. Given $H(m)$, an attacker can — without knowing
$m$ — compute $H(m \| \text{padding} \| m')$ for arbitrary continuations $m'$.
The reason: the hash output value *is* the internal state of the compression
function; the attacker simply resumes the computation from there. SHA-3
(Keccak) [@nist_fips202] and BLAKE2 [@aumasson2013_blake2] address this through different constructions.

### The Guiding Question Behind Secasy

*What happens if you radically decouple internal state from output, and organise
diffusion spatially rather than sequentially?*

Secasy explores this idea: a **two-dimensional grid** as the state space, where
each input byte triggers a non-linear movement through the grid and modifies
cells in its path. The internal state (16,384 bits) is 32 times larger than the
output (512 bits) — making length extension structurally impossible and
broadening the bound against collision attacks.

---

## 2. The Core Concept — Grid-Based State

### Spatial Rather Than Sequential Diffusion

Classic hash functions move data through a chain of blocks:

```
Input → [Block₁] → [Block₂] → [Block₃] → ... → Hash
```

Secasy instead moves a **cursor** through a two-dimensional field:

```
           Column 0   1   2  ...  15
Row 0    [ ●   ·   ·  ...  · ]
Row 1    [ ·   ·   ·  ...  · ]
...      [                    ]
Row 15   [ ·   ·   ·  ...  · ]
              ↑
         256 cells, each with its own state
```

Each input byte is decomposed into four 2-bit direction codes. Each code
controls a cursor movement and modifies the visited cell. After the last
input byte, the grid carries a **unique fingerprint** of the input — only
then does the actual diffusion phase begin.

### Why Is This Structurally Different?

In Merkle-Damgård, the state after block $i$ is fully described by the output
of block $i$ — the state space equals the output size. In Secasy the state
space is $256 \times 64 = 16{,}384$ bits, while the output is only 512 bits.
Even with full knowledge of the hash value, an attacker holds only 3 % of the
internal state — algebraic reconstruction of the grid is not feasible.

---

## 3. Data Structure in Detail

The grid consists of **256 cells** arranged in a 16×16 raster. Each cell
stores three independent values:

| Field        | Type                      | Purpose                                                                 |
|--------------|---------------------------|-------------------------------------------------------------------------|
| `value`      | `int64_t` (64-bit signed) | Numeric cell value; modified by operations                              |
| `primeIndex` | `uint32_t`                | Pointer into the pre-computed prime number table                        |
| `colorIndex` | `uint8_t` (0–5)           | Determines which of the 6 operations is applied to this cell in Phase 3 |

Additionally there is a **prime number table** (`primes.h`) containing the
first 16,000,000 primes (~355 KB). This table is compiled from the header
during build and serves as a deterministic source of jump distances.

The **cursor** is a two-dimensional position $(x, y)$ in the range
$[0, 15] \times [0, 15]$, which moves through the grid during Phase 2.

---

## 4. Phase 1 — Initialization

Initialization is **input-independent** and fully deterministic. Every
execution starts from the same initial state:

- All 256 cells: `value = 2`, `primeIndex = 0`, `colorIndex = ADD`
- Cursor: position $(0, 0)$

The value 2 is the starting value. It is the first prime number and serves as
the initialization. One could equally have chosen 1 or any other value; the
choice of 2 was made for purely pragmatic reasons.

---

## 5. Phase 2 — Input Integration (Fingerprinting)

This is the **critical phase** for collision resistance. Here the input is
transformed into a unique grid state — a fingerprint.

### Byte Decomposition

Each input byte (8 bits) is split into four **2-bit direction codes**:

| Bits | Direction |
|------|-----------|
| `00` | UP        |
| `01` | RIGHT     |
| `10` | LEFT      |
| `11` | DOWN      |

One byte therefore produces four cursor movements. For each movement, **two
operations** are performed on the current cell $(x, y)$:

### Operation A — Prime Update

1. The cell's `primeIndex` is incremented by 1
2. `colorIndex` is advanced cyclically (0 → 1 → 2 → 3 → 4 → 5 → 0)
3. The cell's `value` is overwritten with the next prime from the table

Through the cyclic advancement of `colorIndex`, the **order** in which cells
are visited determines which operation will later be applied to them in
Phase 3 — not the byte content directly.

### Operation B — Data-Dependent Jump

The cursor moves to the next position. Each direction updates exactly
**one** coordinate — the **primary axis** — by a data-dependent offset
derived from the old cell value:

| Direction | Formula                                 |
|-----------|-----------------------------------------|
| UP        | `y = (y - oldPrime + SAV) & 15`         |
| DOWN      | `y = (y + oldPrime) & 15`               |
| LEFT      | `x = (x - oldPrime) & 15`               |
| RIGHT     | `x = (x + oldPrime + SAV) & 15`         |

(`SAV` = `SQUARE_AVOIDANCE_VALUE` = 1, a constant offset applied only to UP
and RIGHT that prevents the cursor from tracing a closed square and returning
to its starting position after four moves. Without it, clockwise and
counter-clockwise traversals would produce identical grid states, leading to
identical hashes.

All coordinates are masked with `& 15` to stay in $[0, 15]$.
Since the grid size is $N = 16 = 2^4$, a power of two, the identity

$$x \bmod N \;=\; x \;\&\; (N-1) \;=\; x \;\&\; 15$$

holds — the bitwise AND replaces the modulo division and executes in a
single CPU instruction.)

Because UP and DOWN modify only $y$ while LEFT and RIGHT modify only $x$,
cursor movements along different axes are **independent**: the final position
is the componentwise sum of all individual jumps per axis. This means that
two direction sequences which contain the same multiset of per-axis offsets
arrive at the same cell — regardless of the order in which the directions
were issued. Formally, for any two directions $d_1, d_2$ that operate on
**different** axes:

$$\text{move}(d_1) \circ \text{move}(d_2) = \text{move}(d_2) \circ \text{move}(d_1)$$

This commutativity is a **known structural limitation** that can, in
principle, lead to path collisions (see Section 10).

Concretely, consider two bytes that differ only in the lowest 2 bits, e.g.
`0x1A` (bits: `00 01 10 00`) and `0x1B` (bits: `00 01 10 11`). The first
2-bit block (`byte & 3`) yields LEFT (10) vs. DOWN (11) — all three
subsequent steps are identical. Since LEFT modifies only $x$ and DOWN
modifies only $y$, these two directions produce **orthogonal** movements on
the grid. When hashing repeated bytes (e.g. `0x1A` × 16 vs. `0x1B` × 16),
the 64 individual steps may visit the **same cells** in permuted order.

Earlier testing with a variant that additionally coupled both axes
(secondary-axis offset derived from the primary axis) eliminated these
collision groups — but introduced a different flaw (direction aliasing
through information loss in the coupling formula). The secondary-axis
coupling was therefore removed. The following byte groups require further
investigation, as they share structurally equivalent cursor paths:

| Group | Byte values with equivalent path structure |
|-------|--------------------------------------------|
| 1     | `0x1A`, `0x1B`, `0x1E`, `0x1F`             |
| 2     | `0x26`, `0x27`, `0x36`, `0x37`             |
| 3     | `0x29`, `0x2D`, `0x39`, `0x3D`             |
| 4     | `0x4A`, `0x4B`, `0x4E`, `0x4F`             |
| 5     | `0x86`, `0x87`, `0xC6`, `0xC7`             |

The common pattern: within each group, the bytes differ only in bits that
map to directions on **different axes**, leading to cursor paths that are
permutations of each other.

### Why Commutativity Does Not Trivially Cause Hash Collisions

Although cursor movement is commutative across axes, two permuted direction
sequences do **not** generally visit the same cells in the same order.
The jump distance at each step depends on the **current value** of the
visited cell, which changes after each visit (because `nextPrimeNumber`
advances `primeIndex`). Therefore:

- After the first divergent step, the two paths land on **different cells**
  with potentially different `value` fields.
- Subsequent jump distances differ, causing the paths to diverge further.

A collision requires that both paths visit exactly the **same multiset** of
cells with the same per-cell visit counts — so that every cell ends up with
the same `primeIndex` and `value`. This is a much stronger condition than
mere cursor convergence. Empirically, no such collisions have been observed
among 50,000 random messages with single-bit flips (12,800,000 trials);
however, no formal proof of collision-freeness exists, and the byte groups
listed above remain an open research question (see Section 10).

### Collision Resistance Through Fingerprint Uniqueness

Two inputs produce a collision only if they leave identical (`value`,
`primeIndex`, `colorIndex`) tuples in **all 256 cells** after Phase 2 —
despite different paths, different visit orders, and prime-driven jump
distances. The combinatorial complexity of the state space
($\approx 2^{16{,}384}$) makes this practically impossible.

---

## 6. Phase 3 — Processing Rounds (Diffusion)

After input integration, the entire grid is swept $r$ times (default: $r = 10$)
in processing rounds. In each round **every cell** is updated — depending on
its `colorIndex`, which was fixed during Phase 2:

| colorIndex | Operation                   | Neighbour          |
|------------|-----------------------------|--------------------|
| 0 — ADD    | `value += neighbour.value`  | above              |
| 1 — SUB    | `value -= neighbour.value`  | below              |
| 2 — XOR    | `value ^= neighbour.value`  | left               |
| 3 — AND    | `value &= neighbour.value`  | right              |
| 4 — OR     | `value \|= neighbour.value` | left               |
| 5 — INVERT | `value = ~value`            | —                  |

Boundary handling: at grid edges, constant fallback values (1 or unchanged
value) are used to avoid undefined behaviour.

### Why Six Different Operations?

- **ADD / SUB:** Additive operations spread values globally and are invertible
  — they alone would preserve linear structure.
- **XOR:** Bitwise, invertible, breaks linear correlations between adjacent cells.
- **AND / OR:** **Not invertible.** From `a AND b = c`, neither $a$ nor $b$
  can be uniquely reconstructed. These two operations are fundamental to the
  one-way property: even complete knowledge of the output hash provides no
  route back to the internal state.
- **INVERT:** Flips all 64 bits simultaneously; prevents the grid from converging
  to all-zero or all-one absorption states.

The empirical justification for this mix — a comparison of diffusion
consistency when each operation is used in isolation — can be found in
**Appendix A**.

### Traversal Order

Cells are not processed in a fixed row-by-row order but with an offset derived
from the **cursor's final position after Phase 2**. That position is
input-dependent — so the order of diffusion itself varies with the input.

### Round Reduction and Minimum Rounds

The effective round count is $\max(r,\, \lceil \text{hashBits} / 64 \rceil)$.
For a 512-bit hash at least 8 mixing rounds always execute to ensure
sufficient diffusion before extraction.

Mixing and extraction are **strictly separated**: all $r$ rounds of
cell-level diffusion complete first, then — from the final grid state —
the required number of 64-bit blocks is extracted in Phase 4.

Empirically, all security metrics are stable from as few as 1 round (see
Round Reduction Analysis). This is because collision resistance and the
avalanche effect originate primarily in Phase 2; the processing rounds amplify
diffusion but are not its source.

> **Structural contrast to SHA/sponge constructions:** In classical hash
> functions such as SHA-2 or Keccak, security is analysed primarily through
> the round function — fewer rounds directly imply weaker security. In Secasy,
> the primary security core lies in the **initialisation phase (Phase 2)**:
> the prime-driven cursor walk distributes and non-linearly mixes input data
> across all 256 cells before Phase 3 begins at all. Phase 3 therefore acts as
> **defense-in-depth** — an additional hardening layer, not the foundation of
> collision resistance.

> *"We propose a hash construction whose security relies on state-dependent,
> prime-driven initialisation rather than on an iterated round function, and
> empirically demonstrate that security metrics saturate within a single
> processing round."*

---

## 7. Phase 4 — Hash Extraction

After **all** processing rounds have completed, the required number of
64-bit blocks is extracted from the **final** grid state.
The extraction function iterates all 256 cells in row-major order
and accumulates:

$$\text{block}_b = \bigoplus_{i=0}^{255} \text{ROL}_7\!\left(\text{acc} \oplus (w_{i,b} \cdot \text{cell}_i.\text{value})\right)$$

Where:

- $w_{i,b} = i + 1 + b \cdot 256$ — a **position-bound weight** offset by the block index $b$
- $b \in \{0, 1, \ldots, \lceil \text{hashBits}/64 \rceil - 1\}$ — the block index
- $\text{ROL}_7$ — left-rotate by 7 bits after each step
- $\oplus$ — XOR accumulation

The block-index offset ensures that each extracted block uses a distinct
set of position weights, so every 64-bit block is a different linear
combination of the grid cells.

The position weight is decisive: if two different cells had the same value
but their positions were swapped, multiplication by $w_{i,b}$ would still produce
a different output block. Permutations of identical cell values are therefore
not collision-equivalent.

For a 512-bit hash, 8 such blocks ($b = 0 \ldots 7$) are extracted from the
final grid state and concatenated:

$$H = \text{block}_0 \,\|\, \text{block}_1 \,\|\, \cdots \,\|\, \text{block}_7$$

---

## 8. Security Arguments

Secasy is an **empirically evaluated** research algorithm. The following
arguments are based on structural reasoning and empirical measurements —
not on formal security proofs.

### 8.1 Collision Resistance

**Structural argument:** Two different inputs would need to leave identical
states in all 256 cells after Phase 2. The state space encompasses
$\approx 2^{16{,}384}$ possible configurations. For a collision, despite
different prime-driven paths, all 256 cells
must match exactly — a combinatorially extreme coincidence.

**Empirical confirmation:** Zero collisions in 1,000,000 attempts with
512-bit output. The birthday bound lies at $2^{256}$ [@menezes1997_hac] — a random test is
practically meaningless at this scale, but the absence of trivial weaknesses
is confirmed.

### 8.2 Preimage Resistance (One-Way Property)

**Structural argument:** AND and OR are not invertible. An attacker who
knows the 512-bit hash holds only 3 % of the internal state (512 of 16,384
bits). The remaining 97 % (15,872 bits) must be recovered — with
non-invertible operations and data-dependent traversal, no algebraic
back-computation is possible.

**Empirical confirmation:** No preimages found in 1,000,000 brute-force attempts.

### 8.3 Length Extension Resistance

**Structural argument (inherent):** The internal state (16,384 bits) is 32×
larger than the output (512 bits). The output is a lossy XOR-accumulation of
the entire grid. An attacker who knows $H(m)$ does not possess the internal
state — they cannot resume the computation because 15,872 bits are unknown.
This distinguishes Secasy fundamentally from SHA-256.

**Comparison:**

| Function      | Internal State  | Output       | Ratio    | Length Ext. Vulnerable? |
|---------------|-----------------|--------------|----------|-------------------------|
| SHA-256 [@nist_fips180_4]   | 256 bits        | 256 bits     | 1:1      | Yes                     |
| SHA-512 [@nist_fips180_4]   | 512 bits        | 512 bits     | 1:1      | Yes                     |
| SHA-3-256 [@nist_fips202] | 1,600 bits      | 256 bits     | 6.25:1   | No                      |
| **Secasy**    | **16,384 bits** | **512 bits** | **32:1** | **No**                  |

### 8.4 Avalanche Effect (empirically confirmed) [@webster1986_sboxes]

A single flipped input bit changes the traversal path from the first affected
direction code onward. Since the jump distance is based on the old cell value,
a different cell modification leads to a different jump, which leads to a
different modification — a cascading, non-linear effect. Measurement:
49.96 % output bit flips for single-bit input changes
(N = 3,200; 95 % CI: [49.88 %, 50.03 %]).

### 8.5 Non-Linearity

AND and OR create non-linear relationships between input and output.
Testing: no instances of $H(A \oplus B) = H(A) \oplus H(B)$ in 10,000
random input pairs.

### 8.6 Statistical Randomness (NIST-inspired)

All 10 NIST-inspired tests [@bassham2010_sp800_22] passed on a bitstream of 50,000 concatenated hashes
(6.4 million bits): Monobit, Runs, Longest Run, Serial, Approximate Entropy,
Cumulative Sums, Byte Distribution, Autocorrelation, Bit Transition, Hash
Collision.

---

## 9. Comparison with Established Constructions

### 9.1 Deviation from Ideal (empirical)

| Algorithm    | Avalanche     | Bit Distribution | Deviation from Ideal |
|--------------|---------------|------------------|----------------------|
| BLAKE2b [@aumasson2013_blake2]  | 50.0 %        | 50.01 %          | 0.03 %               |
| SHA-512 [@nist_fips180_4]  | 49.9 %        | 50.18 %          | 0.06 %               |
| SHA3-256 [@nist_fips202] | 49.9 %        | 50.28 %          | 0.06 %               |
| SHA-256 [@nist_fips180_4]  | 50.2 %        | 49.87 %          | 0.21 %               |
| **Secasy**   | **49.96 %**   | **49.96 %**      | **0.04 %**           |

Secasy shows the smallest empirical deviation from the theoretical ideal.
This comparison measures only statistical surface properties, however — it
says nothing about algebraic attackability.

### 9.2 Construction Comparison

| Property                | Merkle-Damgård  | SHA-3 (Sponge)       | Secasy (Grid)   |
|-------------------------|-----------------|----------------------|-----------------|
| Internal state > output | No              | Yes (6.25:1)         | Yes (32:1)      |
| Length extension safe   | No              | Yes                  | Yes             |
| Non-invertible ops      | Partially       | No (χ is invertible) | Yes (AND, OR)   |
| Formally proven secure  | Yes (reducible) | Yes                  | No              |
| Peer reviewed           | Yes             | Yes                  | No              |
| Round invariance        | No              | No                   | Yes (empirical) |

### 9.3 Structural Difference from AES-Based Constructions

In AES-GCM [@nist_fips197] and similar constructions, each round is structurally distinct
(round keys). Security is directly tied to round count — fewer rounds mean
algebraically simpler, attackable transformations.

In Secasy, security is tied to **Phase 2** (input integration) and the
**extraction function**, not to the round count of the processing phase.
Empirically confirmed: all security metrics remain stable from 1 to 100,000
rounds.

---

## 10. Known Limitations and Open Questions

### What the Tests Show — and What They Don't

The empirical tests verify that the output **looks statistically random**.
This is a necessary but not sufficient condition for cryptographic security.
A linear congruential generator can pass perfect statistical tests and still
be cryptographically worthless.

### Untested Attack Techniques

| Technique               | Target                                    | Status           |
|-------------------------|-------------------------------------------|------------------|
| Algebraic attacks [@courtois2002]  | Polynomial representation of the function | Not investigated |
| Meet-in-the-middle [@diffie1977] | Splitting the computation                 | Not investigated |
| Rebound attacks [@mendel2009_rebound]    | Weaknesses in the diffusion layer         | Not investigated |
| Cube attacks [@dinur2009_cube]       | Low-degree approximations                 | Not investigated |
| SAT-solver attacks      | Constraint-based preimage search          | Not investigated |

### Identified Open Questions

1. **Formal security proof:** No proof of the pseudo-random permutation (PRP)
   property or collision resistance. A formal proof would require modelling
   the state transitions as an ergodic Markov chain and bounding the mixing time.

2. **AND/OR absorption states:** AND pulls bits toward 0, OR toward 1.
   Empirically no absorption states were observed (10,000 tests with structured
   inputs), but a formal proof that ADD, XOR, and INVERT always prevent
   absorption in every case is absent.

3. **Side-channel vulnerability:** The current implementation is not
   constant-time. The `switch(colorIndex)` and prime-table indexed memory
   accesses produce data-dependent timing and cache patterns. For pure hashing
   applications (without secret input) this is acceptable. As an HMAC primitive
   or key-derivation function, a constant-time variant would be required
   [@kocher1996_timing]. In such deployment scenarios, resistance against
   **Fault Injection Analysis (FIA)** should also be evaluated: the nonlinear
   coupling of the 256 grid cells (AND, OR, varying neighbour operations)
   structurally impedes algebraic modelling of fault propagation — however,
   the current implementation provides no formal FIA guarantee.

4. **Peer review:** The algorithm has not yet been analysed by independent
   cryptographers. All security claims should therefore be regarded as
   preliminary.

### Honest Assessment

| Question                                  | Confidence      | Basis                                              |
|-------------------------------------------|-----------------|----------------------------------------------------|
| Does the output look random?              | **Very high**   | N=1M, power >99.9 %                                |
| Is the function cryptographically secure? | **Unknown**     | Needs formal analysis                              |
| Are there obvious design flaws?           | **Probably no** | 30+ tests, 2.5M+ hashes, 0 anomalies               |
| Production-ready?                         | **No**          | No peer review, no formal proofs                   |
| Interesting research contribution?        | **Yes**         | Novel construction principle, extensive evaluation |

---

*Created: 2026-03-16 · Reference implementation: Secasy 512-bit, 10 rounds*

---

\newpage

## Appendix A — Empirical Isolation of Individual Colour Operations

To experimentally validate the necessity of the mixed-operation design, the
processing phase was modified to apply **exactly one operation** to all 256
grid cells. For each mode, 100 random 32-byte messages were hashed; for every
message, each of the $32 \times 8 = 256$ input bits was individually flipped
and the Hamming distance to the original hash was measured
($N = 25\,600$ samples per mode).

**Table: Mean Hamming distance and standard deviation by mode**

| Mode                          | Mean $\mu$ | Std. dev. $\sigma$ | Min       | Max       | Assessment              |
|-------------------------------|-----------|-------------------|-----------|-----------|-------------------------|
| Baseline (full mix)           | 50.0 %    | ±2.3 %            | 0.0 %     | 58.4 %    | Optimal                 |
| ADD only                      | 50.0 %    | ±2.3 %            | 10.9 %    | 59.0 %    | Strong                  |
| SUB only                      | 50.0 %    | ±2.3 %            | 0.0 %     | 63.3 %    | Strong                  |
| XOR only                      | 49.4 %    | ±4.2 %            | 0.0 %     | 64.5 %    | Strong                  |
| AND only                      | 48.5 %    | **±6.9 %**        | 0.0 %     | 65.6 %    | Degraded                |
| OR only                       | 47.7 %    | **±8.6 %**        | 0.0 %     | 61.9 %    | Significantly weakened  |
| INVERT only                   | 49.2 %    | ±4.6 %            | 0.0 %     | 62.1 %    | Slightly weakened       |

> **On the standard deviation $\sigma$:** It describes the **spread of Hamming
> distances** around the mean. A small $\sigma$ means that *every individual*
> bit-flip reliably changes close to 50\% of the output bits — the values
> cluster tightly. A large $\sigma$ means some bit-flips change almost nothing
> (e.g. 10\%) while others change a great deal (e.g. 70\%) — the average may
> still be ~50\%, but **consistency is absent**.

![Histograms: Hamming-distance distribution per mode](img/color_isolation_histograms.png)

![Summary: μ ± σ per mode](img/color_isolation_summary.png)

**Interpretation:**

Notably, neither AND nor OR collapses entirely — the mean stays close to 50 %
in all modes. This is because the **initialisation phase already carries the
primary diffusion**: the prime-driven cursor walk distributes input data so
deeply across all 256 cells that even a monotonically saturating operation in
the processing phase cannot fully destroy that base diffusion.

The decisive quality indicator is the standard deviation $\sigma$, not the
mean:

- **AND only:** $\sigma = 6.9\,\%$ — 3.0× worse than baseline.
  The histogram is noticeably wider; a relevant fraction of samples falls
  outside the ideal 45–55 % band.
- **OR only:** $\sigma = 8.6\,\%$ — 3.7× worse than baseline.
  The histogram spans from 0 % to above 60 %; diffusion is highly
  uneven and no longer reliably tight.
- **ADD / SUB / XOR:** $\sigma \leq 4.2\,\%$ — nearly identical to the
  full mix.

These findings confirm that the **rotating operation mix** is not required to
achieve a mean of 50 % (that property emerges already from Phase 2), but it
is essential for **consistency and tightness of the distribution**. Only the
full mix achieves $\sigma = 2.3\,\%$, guaranteeing that every individual
bit-flip changes approximately 50 % of the output bits with very high
probability and without outliers.

---

\newpage

## Appendix B — Impact of Field Size on Diffusion

Secasy uses a $16 \times 16$ grid (256 cells) by default. This experiment
investigates whether a different field size — smaller or larger — would
improve or degrade diffusion quality. The algorithm was parameterised so
that the field size can be varied at runtime between $4 \times 4$,
$8 \times 8$, $16 \times 16$ (baseline), $32 \times 32$, and
$64 \times 64$.

**Methodology.** For each of the five field sizes, $N = 100$ random
messages (32 bytes) were hashed. For each message, every one of the
$32 \times 8 = 256$ input bits was individually flipped and the Hamming
distance to the original hash ($512$ bits) was measured — yielding
$25\,600$ samples per field size. Additionally, the *nibble symmetry bias*
was computed: the maximum deviation of any single 4-bit output nibble's
flip rate from the ideal $50\,\%$.

**Table: Diffusion quality by field size**

| Field size      | Cells  | $\mu$           | $\sigma$         | Min         | Max         | Nibble bias   | Assessment        |
|-----------------|--------|-----------------|------------------|-------------|-------------|---------------|-------------------|
| $4 \times 4$    | 16     | 46.9 %          | **±12.3 %**      | 0.0 %       | 62.3 %      | 3.40 pp       | Degraded          |
| $8 \times 8$    | 64     | 48.4 %          | **±9.1 %**       | 0.0 %       | 59.2 %      | 1.96 pp       | Weak              |
| $16 \times 16$  | 256    | 50.0 %          | ±2.2 %           | 0.0 %       | 59.2 %      | 0.45 pp       | **Baseline**      |
| $32 \times 32$  | 1024   | 50.0 %          | ±2.2 %           | 41.6 %      | 59.2 %      | 0.38 pp       | Equivalent        |
| $64 \times 64$  | 4096   | 50.0 %          | ±2.2 %           | 41.6 %      | 58.0 %      | 0.48 pp       | Equivalent        |

> **Interpretation note:** The mean $\mu$ alone is not very informative — the
> decisive metric is the standard deviation $\sigma$, which measures the
> *consistency* of diffusion. A lower $\sigma$ means every individual bit-flip
> reliably changes close to 50 % of the output bits. The *nibble bias* indicates
> whether certain output positions are systematically less sensitive than others
> (lower = better).

![Histograms: Hamming-distance distribution per field size](img/field_size_histograms.png)

![Summary: μ ± σ and nibble bias per field size](img/field_size_summary.png)

**Interpretation:**

1. **Below $16 \times 16$, diffusion breaks down.**
   At $4 \times 4$ ($\sigma = 12.3\,\%$) and $8 \times 8$
   ($\sigma = 9.1\,\%$), the Hamming-distance distribution is broad and
   asymmetric. Numerous samples fall into the $0{-}5\,\%$ range
   ($n = 1\,570$ and $n = 831$ out of $25\,600$ respectively), meaning
   many bit-flips produce virtually no change in the hash — the opposite
   of the avalanche criterion. The mean falls to $46.9\,\%$ and
   $48.4\,\%$, well below the ideal. The cause: with only 16 or 64 grid
   cells, the state space is too small for the prime-driven cursor walk
   to influence sufficiently many distinct cells.

2. **$16 \times 16$ is the empirical saturation point.**
   From this field size onward, $\sigma$ stabilises at $\approx 2.2\,\%$
   and the mean at $50.0\,\%$. Nibble bias drops below $0.5$ percentage
   points. The 0–5 % bin contains only $n = 1$ out of $25\,600$ samples
   — a statistical outlier suggesting that for at least one
   (message, bit-position) pair, cursor-path divergence in the 256-cell
   space does not yet fully propagate.

3. **$32 \times 32$ and $64 \times 64$ provide no measurable improvement.**
   $\sigma$, nibble bias, and mean Hamming distance are statistically
   identical to $16 \times 16$. The only improvement: the minimum Hamming
   distance rises from $0.0\,\%$ to $\approx 42\,\%$ — the 0–5 % bin is
   empty. However, this gain comes at the cost of a massively larger state
   (4× and 16× as many cells) and correspondingly higher runtime.

**Conclusion:**
The field size $16 \times 16$ represents the empirically optimal trade-off:
it is the *smallest* grid size at which diffusion fully saturates
($\sigma \leq 2.3\,\%$, $\mu = 50.0\,\%$). Larger grids offer no
measurable quality gain in $\sigma$ or nibble bias. The rare 0 %-Hamming
outliers ($\leq 1 / 25\,600$) point to isolated edge cases in the cursor
walk, not a systematic defect; at $32 \times 32$ they disappear due to
the larger walk space.

---

\newpage
## 11. References
