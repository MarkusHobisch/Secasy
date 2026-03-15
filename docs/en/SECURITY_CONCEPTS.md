\newpage

# Secasy Security Testing — Concept Cheatsheet

> **Purpose:** This document explains all concepts and metrics used in the
> Secasy security tests. It serves as a reference to better understand
> and interpret the test results.

---

## Table of Contents

1. Fundamentals
2. Hamming Distance
3. Popcount (Hamming Weight)
4. Avalanche Effect & SAC
5. Chi-Square Test (χ²)
6. Birthday Paradox & Collision Resistance
7. Preimage and Second-Preimage Resistance
8. Differential Cryptanalysis
9. Linear Approximation & Bit Independence
10. NIST Statistical Tests (SP 800-22)
11. Length Extension Attack
12. Weak Key Detection & Entropy Analysis
13. Z-Test & Confidence Intervals
14. Round Reduction
15. Near-Collision Resistance
16. Non-linearity Test
17. Fuzzing & Memory Safety
18. Performance & Benchmarking
19. Internal State Complexity
20. Overview of All Secasy Tests
21. Quick Reference

---

\newpage

## 1. Fundamentals

### Hash Function — What Do We Expect?

A cryptographic hash function maps an arbitrarily long input to a fixed-length
output value (in Secasy: 512 bits). The **ideal model** is a **Random Oracle**:
the output behaves like a true random number — deterministic (same input →
same hash), but without any discernible pattern.

All tests check whether Secasy meets this ideal.

### Most Widely Used Test Suite: NIST SP 800-22

The US standards body NIST has published a standard for statistical randomness
tests (`NIST SP 800-22`). Several Secasy tests are modeled after it
(`SecasyStatisticalRandomness`).

---

## 2. Hamming Distance

**Files:** `differential_test.c`, `comprehensive_security_test.c`, `stat_rigor_test.c`

### Definition

The **Hamming distance** between two bit strings of equal length is the number
of positions at which the strings differ.
Example: H("10110", "10011") = 2 (positions 3 and 5 are different).

For hex strings (like hash outputs), the bits of each hex digit are compared.
A 512-bit hash output has 128 hex characters = 512 bits.

### Significance for Secasy

For two random 512-bit hashes, the expected Hamming distance is
**≈ 256 bits (= 50 %)**, since each bit independently matches with 50 % probability.

| Measurement            | Ideal Value     | Deviation → Problem |
|------------------------|-----------------|---------------------|
| Avg. Hamming Distance  | 256 of 512 bits | < 240 or > 272      |
| Min. Pairwise Distance | > 200 bits      | < 100 bits          |

---

## 3. Popcount (Hamming Weight)

### Definition

**Popcount** (Population Count) counts the number of set bits (ones) in a
binary number. Also known as the **Hamming weight**.

```c
int popcount(uint64_t x) {
    int count = 0;
    while (x) { count += x & 1; x >>= 1; }
    return count;
}
```

### Relationship to Hamming Distance

Popcount is the **tool** used to compute the Hamming distance:
XOR-ing two hashes produces a 1 at every position where bits differ —
the popcount of that XOR result equals the Hamming distance.

In short: Hamming distance *measures* the difference, popcount *computes* it.

---

## 4. Avalanche Effect & SAC

**Files:** `avalanche.c`, `comprehensive_security_test.c`

### Definition

The **Avalanche Effect** states: flipping **a single input bit** should cause
**≈ 50 %** of all output bits to change.

The **Strict Avalanche Criterion (SAC)** tightens this further: *every individual*
output bit must flip with probability exactly 0.5 when *any single* input bit
is flipped.

### Why Does This Matter?

If a tiny input change only affects a few output bits, an attacker could
systematically search for inputs that produce specific hash prefixes.

### Measured Values for Secasy

At 10 rounds the mean avalanche rate is ≈ 50.0 % ± 0.3 %;
the maximum per-bit bias stays within ± 2 %.

### SAC Matrix

An n×m matrix (n = input bits, m = output bits) counts how often output bit j
flipped when input bit i was toggled. Each entry SAC[i][j] is computed as
(number of flips of bit j) / (total trials) and should be close to 0.5.

---

## 5. Chi-Square Test (χ²)

**Files:** `collision.c`, `comprehensive_security_test.c`, `stat_rigor_test.c`

### Definition

The **chi-square test** checks whether an observed frequency distribution
matches an expected distribution (*goodness-of-fit*).

$$\chi^2 = \sum_{i=1}^{k} \frac{(O_i - E_i)^2}{E_i}$$

- $O_i$ = observed frequency of category $i$
- $E_i$ = expected frequency
- $k$ = number of categories

### Application to Hashes

If Secasy is truly random, each of the 16 hex digits (`0`–`f`) should appear
equally often in the hash output. For N hash outputs each of length L, the
expected frequency of each digit is:

$$E_i = \frac{N \cdot L}{16}$$

A high χ² value means the distribution deviates noticeably from random.

### p-Value & Significance Threshold

The **p-value** is the probability of observing a χ² value at least as extreme
as the one measured, assuming the data is truly random.

| p-value  | Interpretation                       |
|----------|--------------------------------------|
| p > 0.05 | No indication of non-randomness (OK) |
| p > 0.01 | Secasy tests use α = 0.01            |
| p < 0.01 | **Suspicious — test failure**        |

### Applied in Secasy to

- Global hex digit distribution (all 16 digits)
- Positional uniformity (χ² per position in the hash)
- Leading byte distribution (256-way χ²)

---

## 6. Birthday Paradox & Collision Resistance

**Files:** `collision.c`, `comprehensive_security_test.c`, `practical_exploit_test.c`

### The Paradox

In a group of just **23 people**, the probability that two share the same
birthday is already > 50 %. Intuitively surprising!

### Transferred to Hash Collisions

For an n-bit hash, an attacker needs approximately $2^{n/2}$ random attempts
to find a **collision** (two different inputs with the same hash output)
with good probability.

| Hash Bits | Expected Attempts Until Collision    |
|-----------|--------------------------------------|
| 32 bits   | ≈ 65,000                             |
| 64 bits   | ≈ 4 billion                          |
| 128 bits  | ≈ $1.7 \times 10^{19}$               |
| 512 bits  | ≈ $2^{256}$ — practically impossible |

### Birthday Bound in Secasy Tests

The truncation sweep in `collision.c` tests hash spaces of 16–36 bits where
collisions are observable and compared against the formula:

$$P(\text{collision}) \approx 1 - e^{-N^2 / (2 \cdot 2^n)}$$

---

## 7. Preimage and Second-Preimage Resistance

**File:** `comprehensive_security_test.c`

### Preimage Resistance (One-Way Function)

Given a hash $h$, it should be practically impossible to find an $m$ such
that $H(m) = h$.

**Test:** 1 million random inputs against a target hash — no matches expected.

### Second-Preimage Resistance

Given a message $m_1$ and $H(m_1)$, it should be practically impossible to
find another $m_2 \neq m_1$ with $H(m_2) = H(m_1)$.

**Difference from collision:** Here $m_1$ is fixed in advance.

| Property        | Attacker knows     | Goal                          |
|-----------------|--------------------|-------------------------------|
| Preimage        | only $h$           | find $m$ with $H(m)=h$        |
| Second Preimage | $m_1$ and $H(m_1)$ | find $m_2 \neq m_1$ same hash |
| Collision       | nothing            | find any $m_1 \neq m_2$       |

Collisions are the easiest to find (birthday), preimage the hardest.

---

## 8. Differential Cryptanalysis

**Files:** `differential_test.c`, `test_deep_security.c`

### Basic Principle

Differential cryptanalysis investigates whether controlled **differences** in
the input lead to predictable differences in the output.

**Ideal behavior:** The output difference (measured as Hamming distance) is
randomly distributed around 50 % — regardless of how the input difference looks.

### Difference Types Tested in Secasy

| Test             | Input Difference              | Expected Result |
|------------------|-------------------------------|-----------------|
| Sequential       | $n$ vs $n+1$                  | Hamming ≈ 50 %  |
| Single-Bit       | exactly 1 bit different       | Hamming ≈ 50 %  |
| Common Suffix    | same suffix, different prefix | Hamming ≈ 50 %  |
| Sparse Diff      | few bytes different           | Hamming ≈ 50 %  |
| Length Extension | $m$ vs $m \| \text{extra}$    | Hamming ≈ 50 %  |

---

## 9. Linear Approximation & Bit Independence

**Files:** `test_deep_security.c`, `comprehensive_security_test.c`

### Linear Cryptanalysis

Searches for linear relationships between input and output bits:

$$\text{Bias}(i, j) = \left| P(\text{Bit}_j^{out} = \text{Bit}_i^{in}) - 0.5 \right|$$

A bias > 0 means output bit j is correlated with input bit i — that would be
a weakness.

**Secasy result:** Max. bias ≈ 0.026 (ideal: 0.0) — within expected noise for
limited sample sizes.

### Bit Independence Criterion (BIC)

Two different output bits $i$ and $j$ should be statistically independent.
Correlation between output bits would reduce effective security.

$$\text{Correlation}(i, j) = E[\text{Bit}_i \oplus \text{Bit}_j] - E[\text{Bit}_i] \cdot E[\text{Bit}_j] \approx 0$$

---

## 10. NIST Statistical Tests (SP 800-22)

**File:** `statistical_randomness_test.c`

These tests operate on the **bitstream** concatenated from 50,000 hashes.

### 10.1 Frequency (Monobit) Test

Counts the total number of ones and zeros. For true randomness: $P(\text{bit}=1) = 0.5$.

$$S_n = \frac{\#\text{ones} - \#\text{zeros}}{\sqrt{n}}$$

Expected value $S_n \approx 0$, normally distributed.

### 10.2 Runs Test

A **run** is a maximal sequence of identical bits (e.g., `0001` has a run of
length 3 of zeros). Checks whether runs are too short or too long
(which would indicate dependence between consecutive bits).

### 10.3 Longest Run of Ones

Analyzes the longest uninterrupted run of ones within 128-bit blocks.
Runs that are too long indicate periodicity.

### 10.4 Serial Test (2-Bit)

Counts the frequencies of 2-bit patterns `00`, `01`, `10`, `11`. All should
appear equally often (25 % each).

### 10.5 Approximate Entropy

Compares the frequencies of overlapping m-bit patterns for $m$ and $m+1$.
Low entropy → fewer than $2^m$ distinct patterns → weakness.

$$ApEn(m) = \Phi^m - \Phi^{m+1}$$

### 10.6 Cumulative Sums (Cusum)

Evaluates the **random walk**: each `1` → $+1$, each `0` → $-1$. The maximum
deviation from zero should lie within expected bounds for random sequences.

### 10.7 Spectral Test (DFT)

Applies the discrete Fourier transform to the bitstream. **Periodicity** would
appear as peaks in the frequency spectrum — a sign of internal structure.

---

## 11. Length Extension Attack

**Files:** `comprehensive_security_test.c`, `differential_test.c`

### The Problem with Merkle-Damgård

Many hash functions (MD5, SHA-1, SHA-256) are vulnerable: knowing $H(m)$,
one can compute $H(m \| \text{padding} \| m')$ without knowing $m$.
This enables e.g. API signature forgery.

### Test in Secasy

Comparison of $H(m)$ with $H(m \| \text{extra data})$ — the Hamming distance
should be ≈ 50 %, meaning no relationship is detectable.

---

## 12. Weak Key Detection & Entropy Analysis

**File:** `test_deep_security.c`

### Weak Inputs

Certain structured inputs (all zeros, all ones, alternating, counter) could
produce weak outputs — e.g. too many zeros in the hash or recognizable patterns.

**Entropy** measures how uniformly bits are distributed:

$$H = -\sum_{i} p_i \log_2 p_i$$

For an ideal hash: $H \approx 1.0$ bit/bit (maximum entropy).

---

## 13. Z-Test & Confidence Intervals

**File:** `stat_rigor_test.c`

### When Is a Simple Mean Not Enough?

With large sample sizes (100k–1M samples), one can state with **high confidence**
the interval in which the true value lies.

$$\text{CI}_{99\%} = \hat{p} \pm 2.576 \cdot \sqrt{\frac{\hat{p}(1-\hat{p})}{n}}$$

### Z-Test

Checks whether an observed result deviates significantly from the expected value:

$$z = \frac{\hat{p} - p_0}{\sqrt{p_0(1-p_0)/n}}$$

If $|z| > 2.576$ → deviation is significant at the 99 % level.

### Effect Size (Cohen's h)

Even a statistically significant difference can be **practically irrelevant**.
The effect size measures how large the deviation actually is:

$$h = 2 \arcsin(\sqrt{\hat{p}}) - 2 \arcsin(\sqrt{p_0})$$

**Secasy result:** All effect sizes < 0.01 → practically negligible.

---

## 14. Round Reduction

**File:** `tests/round_reduction/`

### Idea

Tests the hash function with **fewer than the normal 10 rounds** (e.g. 1, 2, 4, 6, 8).
With too few rounds, statistical weaknesses should become visible.

**Purpose:** Verifies that security truly derives from internal diffusion
and not from a coincidental property of a specific round count.

**Secasy result:** All tests pass at 8 and 10 rounds. Significant weaknesses
only become visible at very few rounds (< 4).

---

## 15. Near-Collision Resistance

**Files:** `comprehensive_security_test.c`, `practical_exploit_test.c`

### Definition

Rather than searching for exact collisions, this looks for two inputs whose
hash outputs differ in only **very few bits**.

**Attack goal:** Find two messages with Hamming distance < 10 %.
For 512 bits, that means < 51 differing bits.

**Why dangerous?** Near-collisions could serve as a stepping stone toward
full collisions (boomerang attack).

**Secasy result:** Minimum observed Hamming distance is always well above
10 % — no near-collisions found.

---

## 16. Non-linearity Test

**File:** `comprehensive_security_test.c`

### Definition

Checks whether the hash function is **non-linear** — i.e. whether XOR-combining
two inputs has a predictable effect on the output.

**Linear function (bad):** $H(A \oplus B) = H(A) \oplus H(B)$

If this were the case, an attacker could infer unknown hashes from known ones —
similar to linear algebra.

### Test

For random input pairs $A$, $B$, it is checked whether:

$$H(A \oplus B) \neq H(A) \oplus H(B)$$

Each match would be a **linear weakness**. Expected: 0 matches.

**Secasy result:** No linear relationships found — PASS.

---

## 17. Fuzzing & Memory Safety

**File:** `fuzz_test.c`

### What Is Fuzzing?

**Fuzzing** (fuzz testing) means: the implementation is bombarded with
massive amounts of random, unexpected, invalid, or extreme inputs —
to uncover crashes, memory errors, or undefined behavior.

### AddressSanitizer (ASan) & UBSan

Two important compiler tools used during fuzzing:

| Tool                                   | Detects                                       |
|----------------------------------------|-----------------------------------------------|
| **ASan** (AddressSanitizer)            | Buffer overflows, use-after-free, heap errors |
| **UBSan** (UndefinedBehaviorSanitizer) | Integer overflow, null pointer, shift errors  |

### Secasy Fuzz Test

500,000 iterations with:

- Random input lengths: 0 to 4096 bytes
- All hash sizes: 64, 128, 256, 512 bits
- All round counts: 1, 2, 5, 10, 50

**Secasy result:** 0 sanitizer violations — memory-safe across the entire
parameter space.

---

## 18. Performance & Benchmarking

**Files:** `benchmark_rounds.c`, `precise_timing.c`

### Why Test Performance?

Security and speed often conflict. A hash requiring 100,000 rounds is secure
but impractical. Benchmarking quantifies the **security-performance trade-off**.

### Measurements for Secasy

| Round Count | Relative Speed  | Security             |
|-------------|-----------------|----------------------|
| 100,000     | 1x (baseline)   | identical            |
| 10          | ~10,000x faster | identical            |
| 1           | maximum speed   | significantly weaker |

**Conclusion:** 10 rounds are approximately 10,000x faster than the original
100,000-round design — with **no measurable loss in security**.

### Key Metrics

- **Throughput:** Hashes per second (H/s)
- **Latency:** Time for a single hash (microseconds)
- **Scaling:** How does time grow with input length?

---

## 19. Internal State Complexity

**File:** `test_deep_security.c`

### Definition

Counts the number of **unique internal states** the algorithm assumes across
many different inputs. Ideal: a completely different internal state for every
different input.

**Secasy result:** 100 % unique states — no repetitions.

---

## 20. Overview of All Secasy Tests

**`avalanche.c`**
Concepts: SAC, Hamming distance, bias — Sample size: 50–1,000 messages

**`collision.c`**
Concepts: Birthday bound, chi-square, distribution — Sample size: 5,000 hashes

**`differential_test.c`**
Concepts: Differential cryptanalysis, Hamming — Sample size: 100–500 pairs

**`comprehensive_security_test.c`**
Concepts: All 10 security criteria — Sample size: 100k samples

**`test_deep_security.c`**
Concepts: Linear cryptanalysis, differential, internal states — Sample size: 10k samples

**`practical_exploit_test.c`**
Concepts: Near-collisions, birthday attack, linear predictor — Sample size: 5k–100k

**`statistical_randomness_test.c`**
Concepts: NIST SP 800-22 (7 tests) — Sample size: 50,000 hashes

**`stat_rigor_test.c`**
Concepts: Z-test, confidence intervals, effect size — Sample size: 100k–1M samples

---

**`fuzz_test.c`**
Concepts: Fuzzing, ASan/UBSan, memory safety — Sample size: 500,000 iterations

**`benchmark_rounds.c`**
Concepts: Performance, throughput, round trade-off — Measurement: 1–100,000 rounds

---

## 21. Quick Reference: What a Good Hash Must Satisfy

| Property                | Ideal Value         | Fails When       | Secasy (10 Rounds)                   |
|-------------------------|---------------------|------------------|--------------------------------------|
| Avalanche Rate          | 50.0 %              | < 45 % or > 55 % | **50.0 % ± 0.3 %** — PASS            |
| Hamming Distance (avg.) | 256 / 512 bits      | < 240 or > 272   | **≈ 256 bits** — PASS                |
| Chi-Square (p-value)    | p > 0.01            | p < 0.01         | **p >> 0.01** — PASS                 |
| Bit Bias (max.)         | 0.0 %               | > ± 2 %          | **< ± 2 %** — PASS                   |
| Collisions (512 bit)    | 0                   | > 0              | **0 found** — PASS                   |
| Preimage found          | never               | any found        | **none in 1M attempts** — PASS       |
| Entropy                 | 1.0 bit/bit         | < 0.99           | **≈ 1.0 bit/bit** — PASS             |
| Effect size             | < 0.01              | > 0.1            | **< 0.01** — PASS                    |
| Linear bias (max.)      | 0.0                 | > 0.05           | **≈ 0.026** — PASS                   |
| Near-collision distance | > 10 % (51 bits)    | < 10 %           | **well > 10 %** — PASS               |
| Internal states         | 100 % unique        | repetitions      | **100 % unique** — PASS              |
| Non-linearity           | no linear relations | any found        | **0 found** — PASS                   |
| Fuzzing (ASan/UBSan)    | 0 violations        | any crash        | **0 violations (500k)** — PASS       |
| Performance (10 rounds) | practically usable  | too slow         | **~10,000x faster than 100k rounds** |

---

## Closing Remarks

This cheatsheet demonstrates: cryptographic security is not a single test,
but a **complete picture drawn from many independent perspectives**.

Secasy passes all 21 criteria described here. This means:

- The output is **statistically indistinguishable from true randomness**
- **Small input changes** produce completely different hashes (avalanche)
- **No structural weaknesses** were found through linear or differential cryptanalysis
- The implementation is **memory-safe** (ASan/UBSan, 500k fuzz iterations)
- The design is **performant**: 10 rounds deliver full security at high throughput

Important to understand: these tests **do not prove absolute security** —
cryptographic security proofs are mathematical in nature and go beyond
empirical testing. What they do show: Secasy behaves like an ideal hash
function in all tested dimensions, and no attack vectors were found.

The tests also serve as a **regression suite**: every future change to the
algorithm must pass this test suite again — ensuring no new version
accidentally introduces a weakness.

---

*Created: 2026-03-15 · Reference implementation: Secasy 512-bit, 10 rounds*
