# Secasy Cryptanalytic Attack Research Plan

**Classification:** Internal research plan.
**Target:** Current Secasy implementation, default 512-bit output and 10 rounds.
**Status:** Secasy is unpublished, not peer-reviewed, and has no formal security proof.

## 1. What counts as a break

The primary target is one of the following message-level results against the production implementation:

1. A collision `M != M'` with `H(M) = H(M')` requiring substantially less than `2^256` work.
2. A preimage for a chosen 512-bit digest requiring substantially less than `2^512` work.
3. A second preimage requiring substantially less than `2^512` work.
4. A practical distinguisher with a quantified advantage and complexity.

The following are useful intermediate results but are not, by themselves, a full break:

- a collision on a reduced-round, reduced-word, reduced-grid, or truncated-output variant;
- a collision between attacker-chosen internal states that are not reachable from messages;
- a differential trail that cannot pass the Phase-2 reachability constraints;
- a statistical anomaly without a reproducible exploitation procedure.

Every result must report time, memory, solver version, parameters, success probability, and verification against the production C implementation.

### Breaking result — 2026-08-27

Priority A ultimately produced a practical attack through a structured rotor
cycle rather than a short SMT reconvergence. The message `00^2,131,224`
returns the complete pre-finalisation Phase-2 continuation state to the
canonical initial state. Therefore, for the file interface,
`H(M) = H(00^2,131,224 || M)`. Production C verification confirms this
second-preimage construction as well as the equal-length collision
`00^2,131,224` / `aa^2,131,224` at 512 bits and ten rounds.

The attack is implemented in `scripts/python/up_rotor_cycle_search.py` and
packaged for arbitrary file inputs in
`scripts/python/secasy_second_preimage.py`. It supersedes the earlier bounded
negative results: those results remain valid for their exact message lengths,
but do not imply security for long structured inputs. Collision and
second-preimage resistance of the current configuration are broken.

## 2. Correct decomposition of the problem

For a message `M`, write the hash as

`H(M) = E(S3(S2(M), C(M), P(M)))`,

where:

- `S2` is the Phase-2 value state;
- `C` is the message-derived colour schedule;
- `P` is the final cursor position;
- `S3` is the 10-round, schedule-dependent Phase-3 bijection on cell values;
- `E` is the 16,384-to-512-bit MAR extractor.

An exact collision after Phase 2 is sufficient for a full collision, but it is not necessary. Distinct Phase-2 states may pass through distinct Phase-3 permutations and collide under the compressing Phase-4 extractor. The attack must therefore investigate both Phase-2 reconvergence and the much larger set of collisions created by Phase-4 compression.

## 3. Priority A — exact Phase-2 reconvergence solver

### Goal

Find two distinct messages whose collision-relevant Phase-2 states are identical. Such a pair bypasses all questions about Phase 3 and Phase 4 and immediately gives a production hash collision.

### Exact model

Encode Phase 2 as a deterministic transition system, not as a randomized search:

- cursor coordinates: two 4-bit words;
- message: four 2-bit directions per byte;
- per-cell prime index;
- per-cell colour index or visit count modulo 6;
- prime lookup as a constant table;
- movement from `prime[index] mod 16`;
- direction-dependent update `index += 1 + direction`;
- the final `setPrimeNumberOfLastTile()` transition.

Use an SMT solver with bit-vectors and arrays. Maintain two copies of the transition system and assert:

- the messages differ;
- final values, colour map, and cursor position are equal;
- message lengths are valid byte lengths;
- prime-table bounds match the production configuration.

Search incrementally by message length. Run separate same-length, cross-length, and neutral-suffix searches. Add symmetry breaking: lexicographic message ordering and a fixed first point of divergence.

### Why this is the first target

Phase 2 overwrites a cell value while the cursor movement retains only the old prime modulo 16. This is an explicit information-loss boundary. Exhaustive enumeration has only excluded collisions through three bytes; it gives no bound beyond that domain.

### Escalation if direct SMT stalls

1. Replace the 256-cell array by sparse read-over-write event chains.
2. Search for short reconvergence diamonds from a shared intermediate state.
3. Use bounded model checking with incremental clauses.
4. Use bidirectional search where reverse transitions are enumerated exactly.
5. Search continuation-equivalent cycles, not only cycles from the initial state.

### Success test

Re-run both messages through the unmodified production implementation at 512 bits and 10 rounds, compare all intermediate states, then verify that common suffixes preserve the collision where the complete continuation state is equal.

### Implementation status — 2026-07-14

The first exact bounded model is implemented in
`scripts/python/phase2_reconvergence_solver.py`. It uses finite bit-vectors for
the 2-bit directions, 4-bit cursor coordinates, prime-index sums, visit counts,
the final-cell update, and the production prime table. The
`SecasyPhase2Collision --dump-hex` mode is an independent C oracle.

Validation and current bounds:

- 200 concrete random messages of length 0–64 bytes matched the C oracle in
  every cursor, prime-index, color, and prime-value field. In addition, 20
  fixed symbolic instances up to 8 bytes matched both the concrete simulator
  and the C oracle.
- Same-length 1-byte reconvergence: `UNSAT` (Z3 4.16.0, 0.085 s).
- Same-length 2-byte reconvergence: `UNSAT` (Z3 4.16.0, 30.3 s).
- Monolithic same-length 3-byte search: `unknown` after a 300 s timeout.
- Partitioned 3-byte search by first differing direction: steps 9, 10, and 11
  are `UNSAT`; step 8 timed out after 120 s and earlier steps remain open.
- The sum of the final color indices modulo 6 equals the number of Phase-2
  update events modulo 6. A non-empty `L`-byte message has `4L + 1` events.
  Therefore two non-empty messages of different lengths can reconverge only if
  their byte lengths differ by a multiple of three. The invariant excludes a
  direct finalized non-empty/empty Phase-2 equality, but does not exclude a
  neutral *pre-finalisation prefix* followed by a common suffix; the later
  2,131,224-byte rotor cycle exploits exactly that distinction.
- Cross-length `1x4` reconvergence is `UNSAT` (19.6 s). The next admissible
  `2x5` case remained `unknown` after 240 s.

These are bounded solver results, not a security claim. Each run writes JSON
containing the exact parameters, solver version, elapsed time, status, and
timeout reason.

## 4. Priority B — synthesize weak, equal Phase-3 schedules

### Goal

Find message pairs that do not collide after Phase 2 but produce:

- the same final cursor position;
- the same colour map;
- a low-weight difference in cell values.

The pair then enters the same Phase-3 permutation, turning the problem into a controlled value differential instead of two unrelated computations.

### Constraints to prioritize

1. All visited cells finish with colour `ADD`.
2. All colours are restricted to `ADD` or `SUB`.
3. Both messages produce exactly the same colour map.
4. The number of differing Phase-2 cells is minimized.
5. Differing prime values have equal or favourable low-bit patterns to suppress carry growth.

The all-`ADD`/`SUB` case is especially valuable because Phase 3 is affine over `Z/(2^64)` for a fixed schedule. Existing random single-bit flips are not a substitute for this adversarially synthesized message family.

### Deliverable

A catalogue of solver-generated message pairs, their exact schedules, initial cell differences, and the resulting difference after each Phase-3 round.

### Implementation status — 2026-07-14

The solver now has an `equal-schedule` mode requiring the same complete color
map and cursor while allowing different prime values. Same-length one- and
two-byte instances are `UNSAT`; three bytes is `SAT`.

The first C-verified pair was `41c363` / `c1c1c3`: two Phase-2 value cells
differ, yet only five cells and 29 state bits differ after ten rounds. The
schedule contains 245 `ADD`, 10 `SUB`, one `RLX`, and no `XOR`, `RRA`, or
`INVERT` cells. The MAR extractor expands this to a 253-bit digest difference,
showing that output avalanche masks a sparse internal trail.

The pairs expose the fixed XOR input difference `8002a0`. An exhaustive scan
of all `2^23 = 8,388,608` unique three-byte pairs with that difference found:

- 192 pairs with the same color map and cursor;
- no exact Phase-2 reconvergence;
- 18 pairs with only five active cells after ten rounds;
- the best internal pair `42f360` / `c2f1c0`, with five active cells and only
  13 differing state bits after round 10.

For that best pair, all 256 common one-byte suffixes and all 65,536 independent
one-byte suffix pairs fail to restore an equal schedule. The equivalent
one-byte SMT extension is `UNSAT`; the two-byte-per-side symbolic extension is
currently `unknown` after a 300 s timeout.

A bounded four-byte follow-up partitioned the exact equal-schedule search by
the first differing direction (16 partitions, 30 s each). Fifteen partitions
ended `unknown`; partition 15 returned the C-verified pair `a50ebc7d` /
`a50ebcfd`. It differs in one Phase-2 value cell, but expands to 16 active cells
and 122 state bits after round 10; the digests differ in 256 bits and no complete
64-bit MAR block matches. This fails the predefined continuation threshold
(at most eight active cells, one equal MAR block, or digest distance below 193),
so the four-byte equal-schedule branch is stopped rather than given unbounded
solver time.

## 5. Priority C — implement and exploit the exact Phase-3 inverse

For a fixed colour schedule and cursor offset, every elementary Phase-3 update is invertible in the updated cell value. The complete round can therefore be reversed by:

1. subtracting the round and position constants;
2. traversing cell updates in exact reverse execution order;
3. applying the inverse of the selected operation;
4. reversing rounds from round 10 to round 1.

Build an independently tested inverse and verify `inverse(forward(state)) == state` for every colour operation, mixed schedules, and all round counts.

This enables a more useful decomposition of preimage and collision attacks:

1. construct a post-Phase-3 state satisfying a Phase-4 target;
2. invert Phase 3 under a selected weak schedule;
3. solve whether the resulting state is reachable through Phase 2 while producing that same schedule.

The reachability test, not Phase-3 inversion, then becomes the explicit bottleneck.

### Implementation status — 2026-07-14

The exact inverse is implemented in the Python analysis tool. It passed 200
random full-state tests with arbitrary mixed color schedules and round counts,
plus 100 reachable production-C states from messages up to 64 bytes. In every
case, reversing the ten-round final state recovered all 256 original Phase-2
values exactly.

## 6. Priority D — attack the eight-block MAR extractor

### D1. Establish exact internal-state baselines

For one 64-bit output block, an unrestricted internal preimage is immediate when the first 255 cells are fixed. For the last cell,

`v_last = (ROR7(target) - acc_255) * inverse(weight_last) mod 2^64`.

This does not break the message hash, but it is a required baseline and confirms that the security question is entirely the simultaneous eight-block constraint plus message reachability.

The M7 production-oracle census also tested every one of the 16,384 individual
state bits over three random base states. Because every MAR weight is odd, the
model predicts zero dead high bits, and the census confirmed zero. There is no
single dead-bit extractor collision; any useful internal collision must cancel
changes across multiple cells.

### D2. Solve all eight output blocks simultaneously

Fix a 248-cell prefix and leave the final 8–16 cells symbolic. Encode all eight MAR accumulators as 64-bit bit-vector equations. Search for:

- an internal preimage of a selected 512-bit digest;
- two different symbolic tails producing the same 512-bit output;
- low-weight state differences producing equal outputs.

Start with 64-, 128-, and 256-bit output only to validate the model and measure scaling, but do not report these as a 512-bit break.

### Implementation status — 2026-07-14

The Python tool now has two exact bit-vector modes backed by a production-C
oracle for arbitrary 256-cell final states. A second-preimage search that fixes
a real message state outside a 16-cell tail remained `unknown` after 180 seconds,
both directly and after algebraically eliminating the last cell.

A two-tail collision mode derives the final cell from block 0 and adds the
remaining block conditions incrementally from low to high bits. With ten
symbolic tail cells it produced a C-verified internal pair satisfying all 64
bits of block 0 and the low 18 pre-rotation bits of block 1 simultaneously; bit
19 remained `unknown` after 60 seconds. This is a reduced-output intermediate,
not a 512-bit collision.

Reversing both candidates through the exact Phase-3 inverse leaves 246 of 256
cells prime-valued in each candidate. The ten symbolic cells remain outside the
production prime table, so message reachability is not established. This
localizes the next composition problem to those ten cells instead of an
arbitrary 256-cell inverse state.

The follow-up model replaces the ten arbitrary final cells with an exact affine
Phase-3 image of ten symbolic Phase-2 cells. Each symbolic value is selected
from a power-of-two prefix of the production prime table. It additionally
enforces the local Phase-2 relation between prime-index increase, visit count,
and color (`visits mod 6 == color`, with each visit adding 1..4 to the index).
With the first 16 primes, it found a production-C-verified pair satisfying the
low 24 pre-rotation bits of MAR block 0. The two Phase-2 states differ in eight
cells. This remains a reduced-output result, not a hash collision.

Exact target-reachability then reduces the left candidate to message lengths 12
and 15 and the right candidate to lengths 18, 21, and 24. Both left cases are
`UNSAT` (15.5 s and 23.9 s), so the left state is message-unreachable. The
right 18-byte case remains `unknown` after 300 s even after globally filtering
every cell to its exact feasible visit counts; lengths 21 and 24 have not yet
been run. Thus prime-valued internal states alone are not enough: cursor-path
reachability remains the decisive constraint.

### D2a. Carry-aware 64-bit follow-up — 2026-07-14

The prime-tail solver now supports `--output-bits 64 --explicit-carries`.
Every MAR addition is encoded as a 64-stage ripple-carry circuit; a separate
100-case test matched native modular addition in every case. This is a
structural SAT search over locally admissible Phase-2 prime choices, not a
birthday or random-message scan.

For the ten-cell `42f360` schedule and the first 16 production primes, the
first run found a production-C-verified pair with 24 equal low pre-rotation
bits. An independent resumed run reached 25 bits in 6.6 seconds, then returned
`unknown` at bit 26 after 60 seconds. The corresponding 64-bit outputs are
different, so this is not a collision. Local visit-count constraints permit
only lengths 18/21 bytes on the left and 18/21/24/27 on the right. An exact
18-byte left reachability check produced no result within its 90-second solver
budget and was terminated by the outer process limit. No message collision was
found.

### D3. Analyze related block weights

The weights satisfy

`w(i,b) = (2*i + 3) + 512*b`.

All eight blocks therefore use closely related, affine weight families. Determine whether this creates:

- low-bit invariants;
- low-rank derivatives or Jacobians;
- cross-block differential relations;
- carry-free or carry-controlled trails;
- a meet-in-the-middle split across the 256 MAR steps.

This must be symbolic or exhaustive on reduced word sizes first, followed by exact 64-bit solver validation.

### D4. Add reachability progressively

Constrain extractor collision states in this order:

1. arbitrary 16,384-bit states;
2. states in the image of a fixed weak Phase-3 schedule;
3. states whose Phase-3 inverse is prime-valued;
4. states reachable from a Phase-2 message with the required schedule.

This prevents an internal-state result from being mistaken for a message-level break.

## 7. Priority E — CEGAR attack on the full composition

Use counterexample-guided abstraction refinement rather than one monolithic full solver model.

1. **Abstract model:** active cells, colour classes, truncated carry conditions, and output activity in MILP or SAT.
2. **Candidate generation:** produce low-cost differential or collision trails.
3. **Exact model:** validate candidates with 64-bit bit-vector equations.
4. **Reachability model:** require a real Phase-2 message pair.
5. **Refinement:** block impossible activity patterns or carry assignments and repeat.

Scale in the following order:

- word width: 8, 16, 32, 64 bits;
- grid: 4×4, 8×8, 16×16;
- rounds: 1 through 10;
- output: 64 through 512 bits;
- message length: incremental bounds.

Reduced models are diagnostic only. A claimed production attack must be re-solved and verified at 16×16, 64-bit words, 10 rounds, and 512-bit output.

## 8. Priority F — meet-in-the-middle and rebound variants

### Phase-3 MITM

Split the ten rounds after round five:

- forward: messages or structured Phase-2 states through rounds 1–5;
- backward: Phase-4-compatible states through the inverse of rounds 10–6;
- match selected middle-state cells or linear projections, then verify full states.

Use weak equal schedules from Priority B to avoid matching computations governed by unrelated permutations.

### MAR MITM

Split the 256-cell MAR chain. Forward accumulators are computed from the prefix; backward accumulators are obtained by reversing

`a_i = ROR7(a_(i+1)) - v_i*w_i mod 2^64`.

For the full digest the matching key is the vector of eight 64-bit accumulators. Test whether related weights permit matching fewer independent bits than the expected 512.

### Rebound-style strategy

Choose a sparse or structured middle difference, satisfy the low-cost local transitions around the middle exactly, and propagate outward through the invertible Phase-3 rounds. The final candidate is accepted only if both endpoints satisfy Phase-2 reachability and the outputs collide.

## 9. Priority G — formal differential and linear bounds

If direct collision solving does not succeed, derive bounds rather than collect more random samples.

1. Encode exact XOR-differential transitions of modular addition and subtraction.
2. Treat rotations and XOR as deterministic linear transitions.
3. Search the actual in-place 16×16 dependency graph for minimum-cost trails.
4. Include message-derived schedules rather than assuming a random schedule.
5. Prove bounded unsatisfiability for trail weights below a chosen threshold.
6. Repeat for linear masks and higher-order/cube relations.

A useful negative result is an independently checkable proof that no trail below a specified weight exists for a specified schedule class and number of rounds. Average avalanche values do not provide such a bound.

## 10. Tooling and validation requirements

Create one canonical, parameterized model shared by all attacks:

- exact C-compatible unsigned wrap-around;
- exact in-place update order;
- exact edge constants;
- exact round constants;
- exact Phase-2 final-cell update;
- exact eight-block MAR extraction.

For every solver model:

1. generate random concrete assignments;
2. compare each phase against the production C implementation;
3. verify every reported candidate with a standalone C reproducer;
4. save solver input, output, seed, and version;
5. distinguish `SAT`, `UNSAT`, timeout, and out-of-memory outcomes.

On 2026-07-14 this audit caught a stale local Phase-3 copy in
`structural_attack.c`: it omitted the production round and position constants.
The copy was corrected, and its built-in quick self-test now matches production
bit-for-bit before running any white-box measurements.

## 11. Recommended execution order

1. Correct the analysis assumption that Phase-2 equality is necessary for a digest collision.
2. Build the exact Phase-2 SMT model and search bounded reconvergence.
3. Extend it to synthesize equal weak schedules and minimum active-cell differences.
4. Implement and test the exact Phase-3 inverse.
5. Build the eight-block MAR bit-vector model and solve arbitrary-state tails.
6. Compose MAR constraints, inverse Phase 3, and Phase-2 reachability through CEGAR.
7. Add MITM/rebound methods if the direct composition times out.
8. In parallel, derive solver-certified differential and linear lower bounds.

The highest-payoff first experiment is the exact Phase-2 reconvergence solver. The strongest composition-level experiment is the `Phase-4 target -> inverse Phase 3 -> Phase-2 reachability` pipeline. Both search adversarial structure that random statistical tests are inherently unlikely to observe.
