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

---

\newpage

## 1. Motivation — Why a New Hash Algorithm?

### The Dominant Paradigm: Merkle-Damgård

The most widely known cryptographic hash functions — MD5, SHA-1, SHA-256,
SHA-512 — are all built on the **Merkle-Damgård construction** from 1989 [1, 2].
The principle is straightforward: the input is split into equal-sized blocks,
which are processed sequentially through a compression function. The output
state after the final block is the hash.

This linear processing chain has a well-known structural weakness: the
**length extension attack**. Given $H(m)$, an attacker can — without knowing
$m$ — compute $H(m \| \text{padding} \| m')$ for arbitrary continuations $m'$.
The reason: the hash output value *is* the internal state of the compression
function; the attacker simply resumes the computation from there. SHA-3
(Keccak) [4] and BLAKE2 [5] address this through different constructions.

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

The value 2 is not arbitrary: it is the first prime number and is guaranteed
to be free from absorption states for the AND and OR operations in Phase 3
(zero would be absorbed by AND, all-ones by OR).

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

### Operation B — Non-Linear Jump

The cursor moves to the next position. The jump distance is
**data-dependent**: it is based on the **old cell value** (before the Prime
Update) together with a direction-dependent offset:

| Direction | x-computation                   | y-computation                   |
|-----------|---------------------------------|---------------------------------|
| UP        | `x = (x + (y >> 1) + 1) & 15`   | `y = (y - oldPrime + OFF) & 15` |
| DOWN      | `x = (x + (y >> 1) + 1) & 15`   | `y = (y + oldPrime + OFF) & 15` |
| LEFT      | `x = (x - oldPrime + OFF) & 15` | `y = (y + (x >> 1) + 1) & 15`   |
| RIGHT     | `x = (x + oldPrime + OFF) & 15` | `y = (y + (x >> 1) + 1) & 15`   |

(`OFF` is a direction-specific integer offset that prevents wrap-around into
negative territory; all coordinates are masked with `& 15` to stay in $[0, 15]$.)

**Critically:** For vertical movements, the new x-coordinate depends on the
new y-coordinate, and vice versa for horizontal movements. This **cross-axis
coupling** breaks commutativity:

The sequence LEFT→UP produces a different path than UP→LEFT — even from the
same starting position. Combined with prime-driven jump distances, different
inputs follow completely different paths through the grid.

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

| colorIndex | Operation                  | Neighbour          |
|------------|----------------------------|--------------------|
| 0 — ADD    | `value += neighbour.value` | above              |
| 1 — SUB    | `value -= neighbour.value` | below              |
| 2 — XOR    | `value ^= neighbour.value` | left               |
| 3 — AND    | `value &= neighbour.value` | right              |
| 4 — OR     | `value                     | = neighbour.value` | left |
| 5 — INVERT | `value = ~value`           | —                  |

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

### Traversal Order

Cells are not processed in a fixed row-by-row order but with an offset derived
from the **cursor's final position after Phase 2**. That position is
input-dependent — so the order of diffusion itself varies with the input.

### Round Reduction and Minimum Rounds

The effective round count is $\max(r,\, \lceil \text{hashBits} / 64 \rceil)$.
For a 512-bit hash at least 8 rounds always execute — since exactly one 64-bit
block is extracted per round in Phase 4.

Empirically, all security metrics are stable from as few as 1 round (see
Round Reduction Analysis). This is because collision resistance and the
avalanche effect originate primarily in Phase 2; the processing rounds amplify
diffusion but are not its source.

---

## 7. Phase 4 — Hash Extraction

After each processing round, a **64-bit block** is extracted from the grid
state. The extraction function iterates all 256 cells in row-major order
and accumulates:

$$\text{block} = \bigoplus_{i=0}^{255} \text{ROL}_7\!\left(\text{acc} \oplus (w_i \cdot \text{cell}_i.\text{value})\right)$$

Where:

- $w_i = i + 1 \in \{1, \ldots, 256\}$ — a **position-bound weight**
- $\text{ROL}_7$ — left-rotate by 7 bits after each step
- $\oplus$ — XOR accumulation

The position weight is decisive: if two different cells had the same value
but their positions were swapped, multiplication by $w_i$ would still produce
a different output block. Permutations of identical cell values are therefore
not collision-equivalent.

For a 512-bit hash, 8 such blocks are collected from 8 consecutive rounds
and concatenated.

---

## 8. Security Arguments

Secasy is an **empirically evaluated** research algorithm. The following
arguments are based on structural reasoning and empirical measurements —
not on formal security proofs.

### 8.1 Collision Resistance

**Structural argument:** Two different inputs would need to leave identical
states in all 256 cells after Phase 2. The state space encompasses
$\approx 2^{16{,}384}$ possible configurations. For a collision, despite
different prime-driven paths and cross-axis-coupled jumps, all 256 cells
must match exactly — a combinatorially extreme coincidence.

**Empirical confirmation:** Zero collisions in 1,000,000 attempts with
512-bit output. The birthday bound lies at $2^{256}$ [7] — a random test is
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
| SHA-256 [3]   | 256 bits        | 256 bits     | 1:1      | Yes                     |
| SHA-512 [3]   | 512 bits        | 512 bits     | 1:1      | Yes                     |
| SHA-3-256 [4] | 1,600 bits      | 256 bits     | 6.25:1   | No                      |
| **Secasy**    | **16,384 bits** | **512 bits** | **32:1** | **No**                  |

### 8.4 Avalanche Effect (empirically confirmed) [14]

A single flipped input bit changes the traversal path from the first affected
direction code onward. Since the jump distance is based on the old cell value,
a different cell modification leads to a different jump, which leads to a
different modification — a cascading, non-linear effect. Measurement:
50.0007 % output bit flips for single-bit input changes
(N = 1,000,000; 95 % CI: [49.9963 %, 50.0051 %]).

### 8.5 Non-Linearity

AND and OR create non-linear relationships between input and output.
Testing: no instances of $H(A \oplus B) = H(A) \oplus H(B)$ in 10,000
random input pairs.

### 8.6 Statistical Randomness (NIST-inspired)

All 10 NIST-inspired tests [6] passed on a bitstream of 50,000 concatenated hashes
(6.4 million bits): Monobit, Runs, Longest Run, Serial, Approximate Entropy,
Cumulative Sums, Byte Distribution, Autocorrelation, Bit Transition, Hash
Collision.

---

## 9. Comparison with Established Constructions

### 9.1 Deviation from Ideal (empirical)

| Algorithm    | Avalanche     | Bit Distribution | Deviation from Ideal |
|--------------|---------------|------------------|----------------------|
| BLAKE2b [5]  | 50.0 %        | 50.01 %          | 0.03 %               |
| SHA-512 [3]  | 49.9 %        | 50.18 %          | 0.06 %               |
| SHA3-256 [4] | 49.9 %        | 50.28 %          | 0.06 %               |
| SHA-256 [3]  | 50.2 %        | 49.87 %          | 0.21 %               |
| **Secasy**   | **50.0007 %** | **50.0007 %**    | **0.0007 %**         |

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

In AES-GCM [9] and similar constructions, each round is structurally distinct
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
| Algebraic attacks [10]  | Polynomial representation of the function | Not investigated |
| Meet-in-the-middle [13] | Splitting the computation                 | Not investigated |
| Rebound attacks [11]    | Weaknesses in the diffusion layer         | Not investigated |
| Cube attacks [12]       | Low-degree approximations                 | Not investigated |
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
   or key-derivation function, a constant-time variant would be required [8].

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

*Created: 2026-03-15 · Reference implementation: Secasy 512-bit, 10 rounds*

---

## 11. References

[1] R. C. Merkle, "A Certified Digital Signature," in *Advances in Cryptology – CRYPTO 1989*, Lecture Notes in Computer
Science, vol. 435, Springer, Berlin, 1990, pp. 218–238.

[2] I. B. Damgård, "A Design Principle for Hash Functions," in *Advances in Cryptology – CRYPTO 1989*, Lecture Notes in
Computer Science, vol. 435, Springer, Berlin, 1990, pp. 416–427.

[3] National Institute of Standards and Technology, *Secure Hash Standard (SHS)*, FIPS PUB 180-4, Aug. 2015. DOI:
10.6028/NIST.FIPS.180-4

[4] National Institute of Standards and Technology, *SHA-3 Standard: Permutation-Based Hash and Extendable-Output
Functions*, FIPS PUB 202, Aug. 2015. DOI: 10.6028/NIST.FIPS.202

[5] J.-P. Aumasson, S. Neves, Z. Wilcox-O'Hearne, and C. Winnerlein, "BLAKE2: Simpler, Smaller, Fast as MD5," in
*Applied Cryptography and Network Security – ACNS 2013*, Lecture Notes in Computer Science, vol. 7954, Springer, Berlin,
2013, pp. 119–135.

[6] A. Rukhin et al., *A Statistical Test Suite for Random and Pseudorandom Number Generators for Cryptographic
Applications*, NIST Special Publication 800-22, Rev. 1a, National Institute of Standards and Technology, Apr. 2010.

[7] A. J. Menezes, P. C. van Oorschot, and S. A. Vanstone, *Handbook of Applied Cryptography*, CRC Press, 1996.
Available: http://cacr.uwaterloo.ca/hac/

[8] P. C. Kocher, "Timing Attacks on Implementations of Diffie-Hellman, RSA, DSS, and Other Systems," in *Advances in
Cryptology – CRYPTO 1996*, Lecture Notes in Computer Science, vol. 1109, Springer, Berlin, 1996, pp. 104–113.

[9] National Institute of Standards and Technology, *Advanced Encryption Standard (AES)*, FIPS PUB 197, Nov. 2001. DOI:
10.6028/NIST.FIPS.197

[10] N. T. Courtois and J. Pieprzyk, "Cryptanalysis of Block Ciphers with Overdefined Systems of Equations," in
*Advances in Cryptology – ASIACRYPT 2002*, Lecture Notes in Computer Science, vol. 2501, Springer, Berlin, 2002, pp.
267–287.

[11] F. Mendel, C. Rechberger, M. Schläffer, and S. S. Thomsen, "The Rebound Attack: Cryptanalysis of Reduced Whirlpool
and Grøstl," in *Fast Software Encryption – FSE 2009*, Lecture Notes in Computer Science, vol. 5665, Springer, Berlin,
2009, pp. 260–276.

[12] I. Dinur and A. Shamir, "Cube Attacks on Tweakable Black Box Polynomials," in *Advances in Cryptology – EUROCRYPT
2009*, Lecture Notes in Computer Science, vol. 5479, Springer, Berlin, 2009, pp. 278–299.

[13] W. Diffie and M. E. Hellman, "Special Feature: Exhaustive Cryptanalysis of the NBS Data Encryption Standard,"
*Computer*, vol. 10, no. 6, pp. 74–84, Jun. 1977.

[14] A. F. Webster and S. E. Tavares, "On the Design of S-Boxes," in *Advances in Cryptology – CRYPTO 1985*, Lecture
Notes in Computer Science, vol. 218, Springer, Berlin, 1986, pp. 523–534.
