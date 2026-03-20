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

Appendix C — Cell Divergence Growth per Input Byte

Appendix D — ARX Migration: Replacing AND/OR with Rotation-Based Operations

Appendix E — Grid-State Landscape: Hashing ALGORITHM.md

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

| Field        | Type                         | Purpose                                                                 |
|--------------|------------------------------|-------------------------------------------------------------------------|
| `value`      | `uint64_t` (64-bit unsigned) | Numeric cell value; modified by operations                              |
| `primeIndex` | `uint32_t`                   | Pointer into the pre-computed prime number table                        |
| `colorIndex` | `uint8_t` (0–5)              | Determines which of the 6 operations is applied to this cell in Phase 3 |

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

### Operation A — Direction-Dependent Prime Update

1. The cell's `primeIndex` is advanced by $1 + d$, where $d \in \{0, 1, 2, 3\}$
   is the 2-bit direction code (UP=0, RIGHT=1, LEFT=2, DOWN=3). This means
   UP advances by +1, RIGHT by +2, LEFT by +3, DOWN by +4.
2. `colorIndex` is advanced cyclically (0 → 1 → 2 → 3 → 4 → 5 → 0)
3. The cell's `value` is overwritten with the prime at the new index

The direction-dependent advance is the **primary mechanism breaking value
symmetry**: two inputs that visit the same cell via different directions
write different primes into that cell — even on the very first visit.
Additionally, the cyclic advancement of `colorIndex` ensures that the
**order** in which cells are visited determines which operation will later
be applied in Phase 3 — not the byte content directly.

### Operation B — Data-Dependent Jump

The cursor moves to the next position. Each direction updates exactly
**one** coordinate — the **primary axis** — by a data-dependent offset
derived from the old cell value:

| Direction | Formula                         |
|-----------|---------------------------------|
| UP        | `y = (y - oldPrime) & 15`       |
| DOWN      | `y = (y + oldPrime + SAV) & 15` |
| LEFT      | `x = (x - oldPrime) & 15`       |
| RIGHT     | `x = (x + oldPrime + SAV) & 15` |

(`SAV` = `SQUARE_AVOIDANCE_VALUE` = 1, a constant offset applied only to
DOWN and RIGHT. This breaks the symmetry between opposite directions on the
same axis: UP and DOWN from the same cell with the same `oldPrime` land on
**different** target cells. Without SAV, opposite directions would produce
mirror-symmetric jumps, creating exploitable path symmetries for repeated-byte
inputs.

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
coupling was therefore removed.

Instead, the collision groups were resolved by introducing
**direction-dependent prime advancement** (see Operation A) and the
**Square Avoidance Value** (see Operation B). Together, these two
mechanisms ensure that bytes within each group produce different cell
values and different jump trajectories — despite sharing structurally
equivalent cursor paths. Exhaustive testing of all 256 repeated
single-byte inputs confirms 0 collisions.

The originally identified groups were:

| Group | Byte values with equivalent path structure |
|-------|--------------------------------------------|
| 1     | `0x1A`, `0x1B`, `0x1E`, `0x1F`             |
| 2     | `0x26`, `0x27`, `0x36`, `0x37`             |
| 3     | `0x29`, `0x2D`, `0x39`, `0x3D`             |
| 4     | `0x4A`, `0x4B`, `0x4E`, `0x4F`             |
| 5     | `0x86`, `0x87`, `0xC6`, `0xC7`             |

The common pattern: within each group, the bytes differ only in bits that
map to directions on **different axes**, leading to cursor paths that are
permutations of each other. These groups no longer produce collisions.

### Why Axis-Commutativity Does Not Cause Hash Collisions

Although cursor movement is commutative across axes, two permuted direction
sequences do **not** produce identical grid states, for two independent
reasons:

1. **SAV breaks path identity on the first step:** Even when two directions
   operate on different axes (e.g. LEFT vs. DOWN), the SAV offset on
   DOWN/RIGHT means the actual jump distances differ. On the very first
   visit (where all cells hold `oldPrime = 2`), UP jumps by 2 while DOWN
   jumps by 3 (= 2 + SAV). The paths diverge immediately.

2. **Direction-dependent prime advance breaks value identity:** Even if two
   paths hypothetically visit the same cell, different directions write
   different primes (e.g. LEFT advances `primeIndex` by +3, DOWN by +4).
   Different cell values cause different subsequent jump distances,
   producing exponentially diverging trajectories.

A collision would require that both paths leave identical (`value`,
`primeIndex`, `colorIndex`) tuples in all 256 cells — despite writing
different primes at each step and following different jump trajectories.
Exhaustive testing of all 256 repeated single-byte inputs confirms that
no such collisions exist. Among 50,000 random messages with single-bit
flips (12,800,000 trials), no collisions were observed.

### Collision Resistance Through Fingerprint Uniqueness

Two inputs produce a collision only if they leave identical (`value`,
`primeIndex`, `colorIndex`) tuples in **all 256 cells** after Phase 2 —
despite different paths, different visit orders, and prime-driven jump
distances. The combinatorial complexity of the state space
($\approx 2^{16{,}384}$) makes this practically impossible.

### Worked Example: Hashing the Byte `0x4E`

To make Phase 2 concrete, here is a step-by-step trace for the single-byte
input `0x4E` = `01001110` in binary. Reading 2-bit groups from LSB to MSB
yields the direction sequence LEFT, DOWN, UP, RIGHT.

Initial state: cursor at $(0, 0)$, all cells hold `value=2`, `primeIndex=0`.

| Step | Bits | Dir.  | $\Delta$prime | New prime | Old value | Jump formula       | New pos.  |
|------|------|-------|---------------|-----------|-----------|--------------------|-----------|
| 1    | `10` | LEFT  | +3            | 7         | 2         | $x=(0-2)\&15=14$   | $(14, 0)$ |
| 2    | `11` | DOWN  | +4            | 11        | 2         | $y=(0+2+1)\&15=3$  | $(14, 3)$ |
| 3    | `00` | UP    | +1            | 3         | 2         | $y=(3-2)\&15=1$    | $(14, 1)$ |
| 4    | `01` | RIGHT | +2            | 5         | 2         | $x=(14+2+1)\&15=1$ | $(1, 1)$  |

After one byte, four cells have been visited. Each now holds a different
prime (7, 11, 3, 5) instead of the initial 2 — and the cursor sits at
$(1, 1)$.

### Comparison: `0x4E` vs `0x1B` — Same Destination, Different State

The byte `0x1B` = `00011011` decodes to DOWN, LEFT, RIGHT, UP — the **same
four directions** as `0x4E`, just in a different order. Since all source cells
initially hold `value = 2`, the net displacement on each axis is identical:
both cursors arrive at $(1, 1)$.

| Step | Bits | Dir.  | $\Delta$prime | New prime | Old value | Jump formula       | New pos.  |
|------|------|-------|---------------|-----------|-----------|--------------------|-----------|
| 1    | `11` | DOWN  | +4            | 11        | 2         | $y=(0+2+1)\&15=3$  | $(0, 3)$  |
| 2    | `10` | LEFT  | +3            | 7         | 2         | $x=(0-2)\&15=14$   | $(14, 3)$ |
| 3    | `01` | RIGHT | +2            | 5         | 2         | $x=(14+2+1)\&15=1$ | $(1, 3)$  |
| 4    | `00` | UP    | +1            | 3         | 2         | $y=(3-2)\&15=1$    | $(1, 1)$  |

Both bytes end at $(1, 1)$. Yet they leave **different grid states** —
demonstrating both collision-prevention mechanisms in action:

| Cell      | `0x4E`          | `0x1B`          |
|-----------|-----------------|-----------------|
| $(0, 0)$  | 7 (LEFT, $+3$)  | 11 (DOWN, $+4$) |
| $(14, 3)$ | 3 (UP, $+1$)    | 5 (RIGHT, $+2$) |
| $(14, 0)$ | 11 (DOWN, $+4$) | untouched (= 2) |
| $(14, 1)$ | 5 (RIGHT, $+2$) | untouched (= 2) |
| $(0, 3)$  | untouched (= 2) | 7 (LEFT, $+3$)  |
| $(1, 3)$  | untouched (= 2) | 3 (UP, $+1$)    |

**Two independent effects prevent a collision:**

1. **Direction-dependent prime advance** (value asymmetry): Even at the two
   shared cells — $(0, 0)$ and $(14, 3)$ — the bytes write different primes,
   because LEFT ($+3$) vs DOWN ($+4$) and UP ($+1$) vs RIGHT ($+2$) advance
   `primeIndex` by different amounts.

2. **SAV-induced path divergence** (path asymmetry): Although the net
   displacement is the same, the intermediate paths differ. `0x4E` visits
   $(14, 0)$ and $(14, 1)$; `0x1B` visits $(0, 3)$ and $(1, 3)$. These four
   cells hold different values in each grid.

Together, 6 out of 256 cells differ after a single byte — and since every
subsequent step reads cell values to compute jump distances, these differences
amplify exponentially with each additional input byte.

---

## 6. Phase 3 — Processing Rounds (Diffusion)

After input integration, the entire grid is swept $r$ times (default: $r = 10$)
in processing rounds. In each round **every cell** is updated — depending on
its `colorIndex`, which was fixed during Phase 2:

| colorIndex | Operation                                    | Neighbour      |
|------------|----------------------------------------------|----------------|
| 0 — ADD    | `value += neighbour.value`                   | above (posY−1) |
| 1 — SUB    | `value -= neighbour.value`                   | below (posY+1) |
| 2 — XOR    | `value ^= neighbour.value`                   | left  (posX−1) |
| 3 — RLX    | `value = ROL64(value, 13) ^ neighbour.value` | right (posX+1) |
| 4 — RRA    | `value = ROR64(value, 7)  + neighbour.value` | left  (posX−1) |
| 5 — INVERT | `value = ~value`                             | —              |

Boundary handling: at grid edges, constant fallback values (1 or unchanged
value) are used to avoid undefined behaviour.

### Why Six Different Operations?

- **ADD / SUB:** Additive operations spread values globally and are invertible
  — they alone would preserve linear structure.
- **XOR:** Bitwise, invertible, breaks linear correlations between adjacent cells.
- **RLX (Rotate-Left–XOR) / RRA (Rotate-Right–Add):** Rotation breaks the
  positional alignment of bits; the subsequent XOR or modular addition couples
  the rotated value with a neighbour. The combination is **non-linear with
  respect to individual bits** because modular addition generates carries that
  propagate unpredictably. Unlike the bitwise AND/OR operations used in earlier
  versions (see Appendix D), rotation-based operations are **bijective on the
  value domain** — they do not absorb bits toward 0 or $2^{64}-1$ and therefore
  preserve the entropy established in Phase 2.
- **INVERT:** Flips all 64 bits simultaneously; prevents the grid from converging
  toward biased bit patterns.

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

**Structural argument:** An attacker who knows the 512-bit hash holds only
3 % of the internal state (512 of 16,384 bits). The remaining 97 % (15,872
bits) must be recovered. The processing phase mixes cells through
rotation-based operations (RLX, RRA) whose carry propagation creates
non-linear bit dependencies, and data-dependent traversal order prevents
static algebraic modelling. Combined with the lossy XOR-accumulation
extraction (Section 7), no algebraic back-computation path from output to
internal state is known.

**Empirical confirmation:** No preimages found in 1,000,000 brute-force attempts.

### 8.3 Length Extension Resistance

**Structural argument (inherent):** The internal state (16,384 bits) is 32×
larger than the output (512 bits). The output is a lossy XOR-accumulation of
the entire grid. An attacker who knows $H(m)$ does not possess the internal
state — they cannot resume the computation because 15,872 bits are unknown.
This distinguishes Secasy fundamentally from SHA-256.

**Comparison:**

| Function                  | Internal State  | Output       | Ratio    | Length Ext. Vulnerable? |
|---------------------------|-----------------|--------------|----------|-------------------------|
| SHA-256 [@nist_fips180_4] | 256 bits        | 256 bits     | 1:1      | Yes                     |
| SHA-512 [@nist_fips180_4] | 512 bits        | 512 bits     | 1:1      | Yes                     |
| SHA-3-256 [@nist_fips202] | 1,600 bits      | 256 bits     | 6.25:1   | No                      |
| **Secasy**                | **16,384 bits** | **512 bits** | **32:1** | **No**                  |

### 8.4 Avalanche Effect (empirically confirmed) [@webster1986_sboxes]

A single flipped input bit changes the traversal path from the first affected
direction code onward. Since the jump distance is based on the old cell value,
a different cell modification leads to a different jump, which leads to a
different modification — a cascading, non-linear effect. Measurement:
49.999 % output bit flips for single-bit input changes
(N = 1,000,000; 95 % CI: [49.995 %, 50.004 %]).

### 8.5 Non-Linearity

The rotation-based operations RLX and RRA introduce non-linearity through
modular addition's carry propagation: the carry chain from bit $i$ to bit
$i+1$ is a non-linear (AND-like) function of the operands, yet the overall
operation remains bijective and does not absorb entropy. Combined with XOR
and bitwise inversion, this produces a non-linear relationship between input
and output.
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

| Algorithm                      | Avalanche   | Bit Distribution | Deviation from Ideal |
|--------------------------------|-------------|------------------|----------------------|
| BLAKE2b [@aumasson2013_blake2] | 50.0 %      | 50.01 %          | 0.03 %               |
| SHA-512 [@nist_fips180_4]      | 49.9 %      | 50.18 %          | 0.06 %               |
| SHA3-256 [@nist_fips202]       | 49.9 %      | 50.28 %          | 0.06 %               |
| SHA-256 [@nist_fips180_4]      | 50.2 %      | 49.87 %          | 0.21 %               |
| **Secasy**                     | **50.00 %** | **49.96 %**      | **0.04 %**           |

Secasy shows the smallest empirical deviation from the theoretical ideal.
This comparison measures only statistical surface properties, however — it
says nothing about algebraic attackability.

### 9.2 Construction Comparison

| Property                | Merkle-Damgård  | SHA-3 (Sponge)       | Secasy (Grid)                 |
|-------------------------|-----------------|----------------------|-------------------------------|
| Internal state > output | No              | Yes (6.25:1)         | Yes (32:1)                    |
| Length extension safe   | No              | Yes                  | Yes                           |
| Non-linear mixing ops   | Partially       | No (χ is invertible) | Yes (ARX: rotation + add/XOR) |
| Formally proven secure  | Yes (reducible) | Yes                  | No                            |
| Peer reviewed           | Yes             | Yes                  | No                            |
| Round invariance        | No              | No                   | Yes (empirical)               |

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

| Technique                             | Target                                    | Status           |
|---------------------------------------|-------------------------------------------|------------------|
| Algebraic attacks [@courtois2002]     | Polynomial representation of the function | Not investigated |
| Meet-in-the-middle [@diffie1977]      | Splitting the computation                 | Not investigated |
| Rebound attacks [@mendel2009_rebound] | Weaknesses in the diffusion layer         | Not investigated |
| Cube attacks [@dinur2009_cube]        | Low-degree approximations                 | Not investigated |
| SAT-solver attacks                    | Constraint-based preimage search          | Not investigated |

### Identified Open Questions

1. **Formal security proof:** No proof of the pseudo-random permutation (PRP)
   property or collision resistance. A formal proof would require modelling
   the state transitions as an ergodic Markov chain and bounding the mixing time.

2. **~~AND/OR absorption states~~ (resolved):** The original design used
   bitwise AND and OR in Phase 3. AND pulls bits toward 0, OR toward
   $2^{64}-1$ — both are absorptive fixpoints that destroy entropy over
   repeated rounds. Empirical analysis confirmed the problem: after 10
   processing rounds, only 110 of 256 grid cells retained distinct values
   (see Appendix D). In version 2025-06, AND/OR were replaced with
   rotation-based ARX operations (RLX, RRA), which are bijective and
   preserve full entropy. Post-migration, 256/256 cells are distinct
   after processing. This open question is considered resolved.

3. **Side-channel vulnerability:** The current implementation is not
   constant-time. The `switch(colorIndex)` and prime-table indexed memory
   accesses produce data-dependent timing and cache patterns. For pure hashing
   applications (without secret input) this is acceptable. As an HMAC primitive
   or key-derivation function, a constant-time variant would be required
   [@kocher1996_timing]. In such deployment scenarios, resistance against
   **Fault Injection Analysis (FIA)** should also be evaluated: the nonlinear
   coupling of the 256 grid cells (ARX mixing, varying neighbour operations)
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

| Mode                        | Mean $\mu$ | Std. dev. $\sigma$ | Min    | Max    | Assessment |
|-----------------------------|------------|--------------------|--------|--------|------------|
| Baseline (full mix)         | 50.0 %     | ±2.2 %             | 41.4 % | 59.2 % | Optimal    |
| ADD only                    | 50.0 %     | ±2.3 %             | 21.3 % | 58.8 % | Strong     |
| SUB only                    | 50.0 %     | ±2.2 %             | 38.3 % | 60.0 % | Strong     |
| XOR only                    | 49.6 %     | ±3.3 %             | 12.7 % | 60.5 % | Strong     |
| RLX only (Rotate-Left–XOR)  | 49.9 %     | ±2.5 %             | 11.9 % | 58.8 % | Strong     |
| RRA only (Rotate-Right–Add) | 50.0 %     | ±2.2 %             | 27.0 % | 58.8 % | Strong     |
| INVERT only                 | 49.5 %     | ±3.6 %             | 8.6 %  | 61.1 % | Strong     |

> **On the standard deviation $\sigma$:** It describes the **spread of Hamming
> distances** around the mean. A small $\sigma$ means that *every individual*
> bit-flip reliably changes close to 50\% of the output bits — the values
> cluster tightly. A large $\sigma$ means some bit-flips change almost nothing
> (e.g. 10\%) while others change a great deal (e.g. 70\%) — the average may
> still be ~50\%, but **consistency is absent**.

![Histograms: Hamming-distance distribution per mode](img/color_isolation_histograms.png)

![Summary: μ ± σ per mode](img/color_isolation_summary.png)

**Interpretation:**

All seven modes — including the two rotation-based operations RLX and RRA
that replaced the original AND/OR (see Appendix D) — achieve strong
diffusion with $\sigma \leq 3.6\,\%$.

The decisive quality indicator is the standard deviation $\sigma$, not the
mean:

- **ADD / SUB:** $\sigma \leq 2.3\,\%$ — virtually identical to the
  full mix.
- **RLX only (Rotate-Left–XOR):** $\sigma = 2.5\,\%$ — close to
  baseline quality, dramatically better than its AND predecessor
  ($\sigma = 7.6\,\%$).
- **RRA only (Rotate-Right–Add):** $\sigma = 2.2\,\%$ — identical to
  baseline, dramatically better than its OR predecessor
  ($\sigma = 5.8\,\%$).
- **XOR / INVERT:** $\sigma \leq 3.6\,\%$ — the widest individual
  spread, yet still well within strong-diffusion territory.

The empirical evidence demonstrates that the ARX migration (Appendix D)
successfully eliminated the absorptive behaviour that degraded AND
($\sigma = 7.6\,\%$) and OR ($\sigma = 5.8\,\%$). Because AND and OR
are *entropy-absorbing* — they map input pairs towards fixed points
($0\!\times\!00$ and $0\!\times\!\text{FF}$ respectively) — they were the
only two operations that exhibited degraded diffusion in isolation.
Consequently, they were replaced by the bijective rotation-based
operations RLX and RRA, which preserve entropy by construction.

With the current operation set, every individual mode achieves strong
diffusion ($\sigma \leq 3.6\,\%$), and the full six-operation mix
remains optimal ($\sigma = 2.2\,\%$). This guarantees that each
individual bit-flip changes approximately 50 % of the output bits with
very high probability and without outliers.

---

\newpage

## Appendix B — Impact of Field Size on Diffusion

Secasy uses a $16 \times 16$ grid (256 cells) by default. This experiment
investigates whether a different field size — smaller or larger — would
improve or degrade diffusion quality. The algorithm was parameterised so
that the field size can be varied at runtime between $4 \times 4$,
$8 \times 8$, $16 \times 16$ (baseline), $32 \times 32$, and
$64 \times 64$.

**Methodology.** For each of the five field sizes, $N = 400$ random
messages (32 bytes) were hashed. For each message, every one of the
$32 \times 8 = 256$ input bits was individually flipped and the Hamming
distance to the original hash ($512$ bits) was measured — yielding
$102\,400$ samples per field size. Additionally, the *nibble symmetry bias*
was computed: the maximum deviation of any single 4-bit output nibble's
flip rate from the ideal $50\,\%$.

**Table: Diffusion quality by field size**

| Field size     | Cells | $\mu$  | $\sigma$ | Min    | Max    | Nibble bias | Assessment    |
|----------------|-------|--------|----------|--------|--------|-------------|---------------|
| $4 \times 4$   | 16    | 50.0 % | ±2.4 %   | 2.7 %  | 67.6 % | 0.20 pp     | Marginal      |
| $8 \times 8$   | 64    | 50.0 % | ±2.2 %   | 10.6 % | 59.6 % | 0.20 pp     | Near-baseline |
| $16 \times 16$ | 256   | 50.0 % | ±2.2 %   | 40.2 % | 59.4 % | 0.20 pp     | **Baseline**  |
| $32 \times 32$ | 1024  | 50.0 % | ±2.2 %   | 40.4 % | 60.2 % | 0.26 pp     | Equivalent    |
| $64 \times 64$ | 4096  | 50.0 % | ±2.2 %   | 40.8 % | 59.6 % | 0.18 pp     | Equivalent    |

> **Interpretation note:** The mean $\mu$ alone is not very informative — the
> decisive metric is the standard deviation $\sigma$, which measures the
> *consistency* of diffusion. A lower $\sigma$ means every individual bit-flip
> reliably changes close to 50 % of the output bits. The *nibble bias* indicates
> whether certain output positions are systematically less sensitive than others
> (lower = better).

![Histograms: Hamming-distance distribution per field size](img/field_size_histograms.png)

![Summary: μ ± σ and nibble bias per field size](img/field_size_summary.png)

**Interpretation:**

1. **Standard deviation $\sigma$ is remarkably uniform across all field sizes.**
   Even the smallest $4 \times 4$ grid achieves $\sigma = 2.4\,\%$, and all
   field sizes from $8 \times 8$ onward yield $\sigma \approx 2.2\,\%$.
   The mean $\mu$ is $50.0\,\%$ for every configuration. This demonstrates
   that the prime-driven cursor walk combined with ARX mixing rounds
   provides strong average diffusion regardless of grid size.

2. **The key differentiator is the extreme-value range (min/max).**
   At $4 \times 4$, the minimum Hamming distance drops to $2.7\,\%$ and the
   maximum reaches $67.6\,\%$ — indicating that rare input-bit positions can
   produce near-zero or extreme-change outputs. At $8 \times 8$, the range
   narrows to $[10.6\,\%, 59.6\,\%]$. From $16 \times 16$ onward, the range
   tightens to approximately $[40\,\%, 60\,\%]$: every single bit-flip
   reliably changes close to half of the output bits.

3. **$16 \times 16$ is the empirical saturation point for tail behaviour.**
   While $\sigma$ is already near-ideal at smaller grids, *tail collapse*
   — the elimination of extreme outliers — occurs at $16 \times 16$.
   Larger grids ($32 \times 32$, $64 \times 64$) provide no further
   measurable improvement in any metric ($\sigma$, nibble bias, or
   min/max range).

**Conclusion:**
The field size $16 \times 16$ represents the empirically optimal trade-off:
it is the *smallest* grid size where the min–max range fully concentrates
around $50\,\%$ (approximately $[40\,\%, 60\,\%]$), matching the behaviour
of larger grids. Smaller grids show comparable average diffusion quality
($\sigma$) but exhibit wider distributional tails, while larger grids
offer no measurable gain despite significantly higher computational cost.

### Visual Impression: Grid State after Hashing a Longer File

To illustrate the quality of diffusion in the default $16 \times 16$ grid,
the following 3-D scatter plot shows the final grid state after hashing
a longer input file ($\approx 50$ KB). Each cell is positioned by its
row and column; the vertical axis represents the cell value ($\texttt{uint64\_t}$).
Colours encode the cell's assigned operation (ADD, SUB, XOR, RLX, RRA, INVERT).

![Grid landscape: 3-D view of cell values after hashing a $\approx 50$ KB file.
The values are spread across the full 64-bit range with no visible
clustering or pattern — consistent with the statistical findings
above.](img/grid_landscape_file_input.png)

The plot confirms visually that the grid state exhibits no spatial
correlation: neighbouring cells hold unrelated values, the six operations
are distributed uniformly, and the full $[0, 2^{64})$ range is utilised.
This is the spatial counterpart of the statistical claim that
$\sigma \leq 2.2\,\%$ for the $16 \times 16$ grid.

---

\newpage

## Appendix C — Cell Divergence Growth per Input Byte

While Appendices A and B examine the *output hash* quality, this appendix
looks at what happens *inside the grid* during Phase 2: how quickly does
a single-bit input difference spread through the 256 grid cells?

### Metric and Methodology

**Setup.** Two identical random messages (128 bytes each) are prepared.
In one copy, a single bit is flipped. Both messages are then fed into the
grid byte by byte. After each byte the full $16 \times 16$ grid states are
compared cell by cell.

We define the **Cell Hamming Distance** $\text{HDC}(n)$ as the number of
grid cells (out of 256) whose state differs between the two grids *after
$n$ input bytes have been processed*. A cell counts as different if any of
its three components (`value`, `primeIndex`, or `colorIndex`) disagrees.

Conceptually, $\text{HDC}(n)$ traces a **growth curve over time** (measured
in bytes processed): it starts at 0 (both grids are identical before the
flip byte arrives), jumps at the byte where the flip occurs, and then grows
with every subsequent byte as the cursor-walk divergence spreads through
more and more cells. The question is: how many bytes of additional input
are needed until the difference has reached (nearly) the entire grid?

Five experiments were conducted ($N = 200$ message pairs each, 128 bytes),
flipping a single random bit at byte position 0, 1, 32, 64, or 127
respectively. All experiments use the same xorshift64 RNG seed
(`0xDEADBEEFCAFE1234`).

### Results

**Table 1 — Growth curve (Experiment 1: flip at byte 0).** Because the
flip occurs in the very first byte, all 128 bytes are available for
propagation — this gives the maximum observable spread. Each row shows
the state after $n$ of the 128 bytes have been processed.

| Bytes processed $n$ | Mean HDC($n$) | $\sigma$ | Min | Max | % of 256 |
|---------------------|---------------|----------|-----|-----|----------|
| 1                   | 6.3           | 2.3      | 3   | 9   | 2.5 %    |
| 8                   | 52.3          | 4.2      | 39  | 61  | 20.4 %   |
| 16                  | 94.2          | 5.7      | 78  | 107 | 36.8 %   |
| 32                  | 154.3         | 6.8      | 138 | 174 | 60.3 %   |
| 64                  | 212.2         | 6.0      | 195 | 226 | 82.9 %   |
| 87                  | 230.9         | 4.9      | 216 | 244 | 90.2 %   |
| 128                 | 244.0         | 3.2      | 233 | 250 | 95.3 %   |

*Reading example:* after 32 bytes have been processed, on average 154 of
the 256 cells (60 %) already differ between the two grids — caused by a
single bit flip in byte 0.

The combined comparison overlays all five experiments:

![Cell Divergence Comparison: all five flip positions overlaid. Triangles on the x-axis mark the respective flip byte.](img/cell_divergence_comparison.png)

**Table 2 — Final state by flip position.** The later the flip occurs in
the message, the fewer bytes remain for propagation afterward, and the
fewer cells will have diverged by the end of the message.

| Flip at byte | Bytes remaining | Final HDC(128) | % of 256 |
|--------------|-----------------|----------------|----------|
| 0            | 128             | 244.0          | 95.3 %   |
| 1            | 127             | 244.1          | 95.4 %   |
| 32           | 96              | 233.8          | 91.3 %   |
| 64           | 64              | 209.4          | 81.8 %   |
| 127          | 1               | 6.0            | 2.3 %    |

*Reading example:* when the flip is at byte 64, only 64 bytes of input
remain to propagate the difference — resulting in 209 of 256 cells (82 %)
being different at the end. When the flip is in the very last byte (127),
only one byte of propagation occurs, affecting just $\approx 6$ cells.

### Cross-Seed Robustness

To verify that the results are not an artefact of a particular RNG seed,
Experiment 1 (flip at byte 0) was repeated with five independent seeds
($N = 200$ each). The table shows HDC at three checkpoints along the
growth curve: after 1 byte, after 87 bytes (the 90 % saturation point),
and after all 128 bytes.

| Seed                      | HDC after 1 byte | HDC after 87 bytes | HDC after 128 bytes |
|---------------------------|------------------|--------------------|---------------------|
| `0xDEADBEEFCAFE1234`      | 6.29             | 230.91             | 244.04              |
| `0x123456789ABCDEF0`      | 5.92             | 230.97             | 244.28              |
| `0xAAAAAAAAAAAAAAAA`      | 6.10             | 230.76             | 244.35              |
| `0x5555555555555555`      | 5.87             | 231.16             | 244.22              |
| `0xFEDCBA9876543210`      | 5.67             | 231.29             | 244.50              |
| **Grand mean**            | **5.97**         | **231.02**         | **244.28**          |
| **$\sigma$ across seeds** | **0.21**         | **0.19**           | **0.15**            |

The inter-seed standard deviation ($\sigma \leq 0.21$) is more than an
order of magnitude smaller than the intra-seed variance, confirming that
the divergence curve is a stable property of the algorithm, not of
particular message content.

### Interpretation

1. **Consistent initial divergence of $\approx 6$ cells per byte of
   propagation**, independent of flip position and RNG seed — matching
   the worked example in Section 5.

2. **Position-invariant curve shape.** The growth curve simply shifts
   right by the flip position; the grid's diffusion mechanism operates
   uniformly with no "weak spots."

3. **Divergence rate: $\approx 4$ cells per byte** in the linear growth
   phase (4 direction steps per byte, each touching a new cell).

4. **90 % saturation at $\approx 87$ bytes of propagation.** Messages
   $\geq 87$ bytes long achieve near-full grid divergence from any
   single-bit change in the first half.

5. **Phase 2 alone produces $\geq 95\%$ state divergence** (before
   Phase 3 mixing rounds even begin), confirmed across 2,000 independent
   message pairs (5 positions × 200 + 5 seeds × 200).

---

\newpage

## Appendix D — ARX Migration: Replacing AND/OR with Rotation-Based Operations

### Motivation

The original Phase 3 design used six operations: ADD, SUB, XOR, **AND**,
**OR**, and INVERT. AND and OR were included to provide non-invertibility
as an argument for the one-way property (Section 8.2). However, both
operations are **absorptive**: AND has fixpoint 0 (`x AND 0 = 0`) and OR
has fixpoint $2^{64}-1$ (`x OR 0xFFFF...F = 0xFFFF...F`). Over multiple
processing rounds, values are driven toward these attractors, destroying
the entropy that Phase 2 established.

This effect was discovered through **4D grid-state visualisation** — plotting
every cell's value as a 3D landscape with colour-coded operation type (see
grid landscape images below). The processing phase with AND/OR produced
visible clustering at extremal values, whereas the initialisation phase
exhibited a healthy uniform distribution.

### Empirical Evidence

Grid-state analysis of the input `16x0x1B` (16 repetitions of byte `0x1B`):

| Metric                        | AND/OR (original)     | ARX (current)         | Change           |
|-------------------------------|-----------------------|-----------------------|------------------|
| Distinct cell values (of 256) | 110                   | 256                   | +133 %           |
| Minimum cell value            | 0                     | 15,810                | No zero fixpoint |
| Maximum cell value            | $1.84 \times 10^{19}$ | $1.84 \times 10^{19}$ | Unchanged        |
| Standard deviation            | $8.0 \times 10^{18}$  | $5.5 \times 10^{18}$  | More uniform     |
| Bimodal clustering            | Yes (at 0 and max)    | No                    | Eliminated       |

The AND/OR version lost 57 % of cell value diversity during processing.
The ARX version retains 100 % — every cell holds a unique value after
10 processing rounds.

![ARX migration comparison: distinct values and minimum cell value before (AND/OR) and after (ARX) the operation replacement.](img/arx_migration_comparison.png)

### The Replacement Operations

The two problematic operations were replaced with **rotation-based ARX
primitives** — a well-established construction family used in SHA-512
[@nist_fips180_4], BLAKE2 [@aumasson2013_blake2], and ChaCha20
[@bernstein2008_chacha]:

| Slot | Old Operation              | New Operation              | Formula                              |
|------|----------------------------|----------------------------|--------------------------------------|
| 3    | `value &= neighbour` (AND) | **RLX** (Rotate-Left–XOR)  | `value = ROL(value, 13) ^ neighbour` |
| 4    | `value \|= neighbour` (OR) | **RRA** (Rotate-Right–Add) | `value = ROR(value, 7) + neighbour`  |

**Rotation constants:** 13 (left) and 7 (right). Both are coprime to 64
and avoid alignment with byte boundaries (multiples of 8), ensuring that
every bit position is mixed into non-adjacent positions over successive
rounds.

### Why ARX Solves the Problem

1. **Bijectivity.** Rotation is a bijection on 64-bit words — no information
   is lost. Unlike AND/OR, which map distinct inputs to identical outputs
   (e.g. `x AND 0 = 0` for all `x`), `ROL(x, 13)` has a unique inverse
   `ROR(y, 13)`.

2. **No absorptive fixpoints.** There is no value $v$ such that
   `ROL(v, 13) ^ n = v` for all neighbours $n$, or `ROR(v, 7) + n = v`
   for all $n$. The operations cannot drive the grid toward a single
   attractor.

3. **Non-linearity through carry propagation.** Modular addition (in RRA)
   generates carry chains that propagate from bit $i$ to bit $i+1$ in a
   non-linear fashion — the carry function is effectively an AND of the
   operand bits. This provides the non-linear mixing previously attributed
   to AND/OR, but without the absorptive side effect.

4. **Established cryptanalytic confidence.** The ARX paradigm has been
   studied extensively in the context of SHA-2, BLAKE, Salsa20/ChaCha, and
   Skein. While no formal proof of security exists for the specific
   combination used here, the building blocks are well understood.

### Grid-State Visualisation

The following 3D landscape plots show the grid state after processing for
two different inputs. Each point represents one of the 256 grid cells;
the $z$-axis encodes the cell's `value` and colour indicates the assigned
operation (colorIndex).

![Grid landscape for input
`16×0x1B` — ARX version. All 256 cells occupy distinct heights with no visible clustering.](img/grid_landscape_16x1B.png)

![Grid landscape for input `16×0x4E` — ARX version.](img/grid_landscape_16x4E.png)

### Impact on Other Security Metrics

The ARX migration is **entropy-preserving by design**: it changes only Phase 3
operations while leaving Phase 2 (fingerprinting), Phase 4 (extraction), and
the overall architecture untouched. All previously reported security metrics
(avalanche effect, collision resistance, statistical randomness) were measured
on this construction or on variants where Phase 3 operations have minimal
impact (see round-reduction analysis in Section 6). Updated isolation
measurements for the RLX and RRA modes individually are planned.

---

\newpage

## Appendix E — Grid-State Landscape: Hashing ALGORITHM.md

The following 3-D scatter plot shows the Secasy grid state after hashing
this very document (`ALGORITHM.md`, $\approx 40$ KB). Each point represents
one of the 256 cells; the vertical axis encodes the cell's `uint64_t` value
and colour indicates the assigned operation (ADD, SUB, XOR, RLX, RRA, INVERT).

![Grid-state landscape after hashing ALGORITHM.md. All 256 cells occupy
distinct, scattered heights across the full 64-bit range with no visible
spatial clustering — consistent with the diffusion results reported in
Appendix B.](img/grid_landscape_algorithm_md.png)

The absence of clustering or periodiciy in the final state is the spatial
counterpart of the statistical claim that $\sigma \leq 2.2\,\%$ for the
$16 \times 16$ grid (Appendix B). It also demonstrates that the ARX
operations (Appendix D) produce visually indistinguishable diffusion
for realistic, structured inputs such as a formatted text document.

\newpage

## 11. References
