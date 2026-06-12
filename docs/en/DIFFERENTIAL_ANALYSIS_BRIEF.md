\newpage

# Secasy — Specification and Open Problem for Differential Cryptanalysis

**Audience.** This document is a self-contained brief for a cryptographer
working on symmetric-primitive analysis. It specifies the Secasy hash
function precisely enough to permit differential / algebraic analysis,
summarises the analysis already performed by the author, and states the
single open problem on which external expertise is sought.

**Status.** Secasy is an unpublished, non-peer-reviewed research hash
function. No formal security proof exists. All security statements below are
either *structural arguments* or *empirical observations*; neither establishes
resistance against the attack class discussed here. The purpose of this brief
is precisely to obtain the formal analysis that the author cannot provide
alone.

---

## 1. What Is Being Asked

Secasy passes the statistical batteries the author has applied (avalanche,
bit-independence, $\chi^2$ uniformity, sequential correlation, NIST SP 800-22
[@bassham2010_sp800_22]) and resists every white-box structural probe the
author constructed (Section 4). However, **statistical indistinguishability is
not resistance to differential cryptanalysis** [@biham1991_differential]: a
construction can pass all standard randomness tests while admitting a
high-probability differential characteristic that no such test detects.

The open problem is therefore:

> **Open Problem.** Does there exist a differential characteristic over the
> $r = 10$ diffusion rounds of Secasy's Phase 3 (Section 3.3), reachable from a
> message-level input difference through Phase 2 (Section 3.2) and surviving the
> Phase-4 finalizer (Section 3.4), whose probability exceeds the
> generic collision bound $2^{-256}$ (resp. the preimage bound $2^{-512}$)?

Equivalently: bound the maximum expected differential probability (MEDP) of the
Phase-3 round function and the number of rounds required to drive it below the
birthday bound, in the spirit of the wide-trail strategy [@daemen2002_aes].

---

## 2. Notation and Primitives

- All arithmetic on cell values is in the ring $\mathbb{Z}/2^{64}$ (64-bit
  unsigned, wrap-around). $\boxplus$ and $\boxminus$ denote addition and
  subtraction in this ring; $\oplus$ is bitwise XOR; $\lll$ / $\ggg$ are
  left / right bit-rotation; $\lnot$ is bitwise complement.
- The state is a grid $G$ of $N \times N$ **cells**, $N = 16$, so
  $256$ cells. Each cell $(x, y)$, $x, y \in \{0, \dots, 15\}$, stores a
  64-bit `value`, a `colorIndex` $\in \{0,\dots,5\}$, and a `primeIndex`.
  The full internal state is $256 \times 64 = 16{,}384$ bits.
- The digest is $512$ bits by default (eight 64-bit blocks). The state is thus
  **32× wider than the output** (wide-pipe), which structurally precludes
  length extension [@menezes1997_hac].
- Index arithmetic on coordinates is modulo $N$, realised as `& 15` since
  $N = 2^4$.

The two differential difference notions relevant here are the XOR difference
$\Delta^\oplus$ and the additive difference $\Delta^\boxplus$; the round function
mixes both, so the analysis must track the cost of switching between them
through modular addition, for which exact transition probabilities are known
[@lipmaa2002_addition].

---

## 3. Specification

Secasy processes an input in four phases: (1) deterministic grid
initialisation, (2) input-driven fingerprinting, (3) $r$ diffusion rounds,
(4) extraction. Phases 3 and 4 are the differential-analysis targets; Phase 2
determines which input differences are *reachable*.

### 3.1 Phase 1 — Initialisation (input-independent)

The grid is filled with a fixed, deterministic pattern of values, prime
indices, and color indices. This phase does not depend on the message and is
therefore a constant; it can be treated as a fixed starting state $G_0$ for
differential purposes.

### 3.2 Phase 2 — Input Integration (the reachability filter)

A cursor starts at $(0,0)$. Each input byte is split into four 2-bit codes
(LSB first); each code selects a direction (UP / DOWN / LEFT / RIGHT). For each
direction the cursor jumps a distance derived from the **prime** currently
stored at the cell, advances that cell's `primeIndex` by a
direction-dependent amount, sets its `colorIndex` to `(visits mod 6)`, and
updates its `value`. A constant `SQUARE_AVOIDANCE_VALUE = 1` is added on DOWN
and RIGHT to break axis symmetry.

Two properties matter for the analyst:

1. **Phase 2 is lossy and prime-valued.** After Phase 2 the cell values are
   functions of a prime-indexed walk, *not* arbitrary attacker-chosen 64-bit
   words. This is the reason the Phase-4 weakness in Section 4 does not lift to
   a message-level attack: the attacker cannot directly write a chosen grid
   state.
2. **The `colorIndex` field — which fixes the per-cell operation used in every
   Phase-3 round — is frozen at the end of Phase 2 and is input-dependent.**

### 3.3 Phase 3 — Diffusion Rounds (primary differential target)

The grid is swept $r$ times (default $r = 10$, with a floor of
$\lceil \text{hashBits}/64 \rceil$). In each round, **every** cell $(i,j)$ is
updated by the operation selected by a `colorIndex` read from a
cursor-offset position, then a round- and position-dependent constant is added.

The six operations (selected by `colorIndex`) are:

| `colorIndex` | Operation | Update rule (neighbour $n$) |
|---|---|---|
| 0 ADD | additive | $v \leftarrow v \boxplus n_{\text{above}}$ |
| 1 SUB | additive | $v \leftarrow v \boxminus n_{\text{below}}$ |
| 2 XOR | bitwise | $v \leftarrow v \oplus n_{\text{left}}$ |
| 3 RLX | ARX | $v \leftarrow (v \lll 13) \oplus n_{\text{right}}$ |
| 4 RRA | ARX | $v \leftarrow (v \ggg 7) \boxplus n_{\text{left}}$ |
| 5 INV | bitwise | $v \leftarrow \lnot v$ |

At grid edges the neighbour is replaced by the constant $1$. After the operation,
the following injection is applied to cell $(i,j)$:

$$v_{i,j} \;\leftarrow\; v_{i,j} \;\boxplus\; \big(K \cdot (\rho + 1)\big) \;\boxplus\; (i \cdot N + j),$$

where $\rho \in \{0, \dots, r-1\}$ is the round index and
$K = \texttt{0x9E3779B97F4A7C15}$ is a fixed round constant (the fractional part
of the golden ratio scaled to 64 bits — a *nothing-up-my-sleeve* value). The
factor $(\rho+1)$ makes every round a **distinct** mapping (a deliberate
countermeasure against slide and self-similarity attacks); the position term
$(i\cdot N + j)$ breaks the translational symmetry that an all-equal state would
otherwise retain.

The cursor offset that selects which `colorIndex` drives cell $(i,j)$ advances
by one column per round, so the *schedule* of operations across rounds is fixed
by the post-Phase-2 color map and is **value-independent**.

**The honest structural caveat.** Operations ADD, SUB, XOR, RLX, RRA are
$\mathrm{GF}(2)$-linear *except* for the carry chains of the modular additions;
INV is affine. There is **no S-box**. Consequently the entire Phase-3 map is, in
the worst case (an all-ADD/SUB color layout), affine over $\mathbb{Z}/2^{64}$,
and its only source of $\mathrm{GF}(2)$-nonlinearity is the addition carry.
The author has verified empirically (Section 4) that this does *not* manifest as
a measurable avalanche defect, but **whether it admits a low-weight differential
characteristic is exactly the open question** — statistical flatness does not
preclude it.

### 3.4 Phase 4 — Extraction / Finalizer

After all rounds, each 64-bit output block $b$ is computed by a
multiply–add–rotate (MAR) accumulation over all 256 cells in row-major order:

$$
\text{acc} \leftarrow 0; \qquad
\text{for each } (x,y): \;
\text{acc} \leftarrow \big((\text{acc} \boxplus v_{x,y}\cdot w_{x,y,b}) \lll 7\big),
$$

with the **odd** position weight
$w_{x,y,b} = 2\big((x N + y + 1) + b N^2\big) + 1$. Oddness makes
$v \mapsto v \cdot w$ a unit map (bijection) in $\mathbb{Z}/2^{64}$, providing
whitening for low-entropy states. The combine step uses modular **addition**
(not XOR) before each rotation: this is deliberate, because $\lll$ distributes
over $\oplus$ but **not** over $\boxplus$ (carries cross the rotation boundary),
which removes the separable-XOR / generalized-birthday structure that an
XOR accumulator would expose (see A1/A3 in Section 4).

---

## 4. Analysis Already Performed (so it need not be repeated)

The author has run the following — all reproducible from the repository's
`tests/analysis/` harnesses. Results are summarised honestly; none constitutes a
proof of differential resistance.

- **Avalanche / SAC / BIC.** Single-bit input flips produce ~50 % output-bit
  change; SAC and BIC deviations are within statistical noise at $r = 8$ and
  $r = 10$. Avalanche is *flat* across round counts from 100 down to the
  minimum — interpreted by the author as a symptom of the linear structure,
  **not** as evidence of security.
- **White-box diffusion (M1–M7).** Reading the live global grid: a 1-bit
  message flip reaches min 219 / mean 256 of the 256 post-Phase-3 cells at
  $r = 10$ (round gradient: 25 → 51 → 72 → 108 → 152 → 219 over rounds
  1, 2, 3, 4, 6, 10). Minimum avalanche over 1.15M flips is 38.9 %, matching
  the expected order statistic for 256-bit outputs — no anomalously low-weight
  differential was found *by random search*.
- **Extractor break and fix (A1/A3).** The *original* XOR-accumulator finalizer
  was shown to factor into a XOR of 256 independent per-cell bijections,
  yielding an $O(1)$ collision in the **compression domain** (direct grid
  access). It does **not** lift to a message-level attack because Phase 2 is
  lossy and prime-valued (A2: the required 2-cell cancellation is never
  assemblable from a message; min 219 cells change per 1-bit flip). The
  finalizer was nevertheless changed from $\oplus$ to $\boxplus$ accumulation
  (Section 3.4) to destroy the separable structure outright; verified the
  closed-form match dropped from 100000/100000 to 0/100000.
- **Reduced-model algebraic probes.** On scaled-down toy rings (≤ 24-bit
  exhaustive: ANF degree via Möbius transform, cube-superpoly degree
  [@dinur2009_cube], Walsh–Hadamard linear bias [@matsui1994_linear]), the
  results are *mixed*: max ANF degree saturates quickly but **minimum** degree
  lags, and low-dimension linear superpolys persist to the last round. The
  author flags these as scaling artefacts of the tiny topology, **not** as
  results that transfer to the full 16,384-bit state.

What has **not** been done — and is the requested contribution — is a proper
differential trail analysis of the *actual* round function over
$\mathbb{Z}/2^{64}$.

---

## 5. Suggested Methodology

These are suggestions only; the collaborator's judgement governs.

1. **Single-round MEDP.** Determine the maximum expected differential
   probability of one Phase-3 round as a function of the active color set,
   treating the additions with the Lipmaa–Moriai transition machinery
   [@lipmaa2002_addition] and the XOR/rotation steps as the linear layer.
2. **Multi-round trail search.** Bound the best characteristic over $r$ rounds,
   e.g. via MILP / SAT modelling of the active-cell propagation
   [@mouha2012_milp], exploiting the fixed value-independent operation schedule
   (Section 3.3) which makes the round structure static and hence MILP-friendly.
3. **Reachability through Phase 2.** Of the input differences that yield a good
   Phase-3 trail, determine which are *reachable* from a message difference
   given the lossy prime walk — this is where the construction may be saved (or
   broken) in practice.
4. **Truncated / higher-order variants** [@knudsen1995_truncated] may be more
   natural than bit-level trails, given the cell-granular state.
5. **Structural comparison.** A central question is whether the absence of an
   S-box and the use of the *same* operation schedule each round (contrast with
   the per-round S-box and wide-trail bound of AES / Keccak
   [@daemen2002_aes; @nist_fips202]) is fatal, or whether the carry
   nonlinearity of modular addition plus the MAR finalizer suffices.

---

## 6. Reproducibility

The full C11 source, all analysis harnesses, and the canonical test vectors are
in the repository. The round function of Section 3.3 is in
`src/ProcessingPhase.c`; the finalizer of Section 3.4 is in
`src/Calculations.c`; the white-box probes are under `tests/analysis/`. The
author can provide a reference (non-optimised) Python/SageMath transcription of
the round function on request to facilitate trail-search tooling.

---

## References
