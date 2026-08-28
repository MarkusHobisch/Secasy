# Secasy

Secasy is an experimental grid-based hash function written in C. It is a
research project and must not be used for cryptographic security.

> **Security status: broken.** The default configuration has practical
> collisions and second preimages caused by a neutral Phase-2 cycle.

## Main result

Let `P = 2,131,224` bytes. Both of these blocks are neutral prefixes:

```text
N0  = 00 repeated P times
Naa = aa repeated P times
```

For every file `M`:

```text
H(M) = H(N0 || M) = H(Naa || M)
```

Neutral blocks can be combined. With `k` prepended blocks chosen independently
from `{N0, Naa}`, one message has at least `2^k` distinct colliding messages of
the same added length. Across all values of `k`, every message therefore has an
infinite collision family. `2^k` is only a lower bound; more neutral prefixes
may exist.

The construction was verified against the production C implementation with a
512-bit output and ten rounds.

## What we investigated

| Work | Result |
|------|--------|
| Statistical and avalanche tests | Outputs look statistically random, with about 50% avalanche. These tests did not reveal the structural break. |
| Exhaustive inputs up to three bytes | No full Phase-2 or 64-bit digest collision was found. This bounded result says nothing about long structured inputs. |
| Round-reduction tests | Measured output statistics remain nearly unchanged from one to 100 rounds. More rounds do not address the Phase-2 flaw. |
| Phase-3 structural analysis | Phase 3 is invertible for a fixed schedule. Solver-generated equal schedules also showed sparse internal differential trails. |
| Phase-4 extractor analysis | The former separable XOR extractor was replaced by the additive MAR extractor. The current extractor has no individually dead state bits, but it cannot repair an earlier Phase-2 collision. |
| Exact SMT modelling | The Phase-2 model and Phase-3 inverse match the C implementation. Short reconvergence searches were negative, while three-byte equal-schedule pairs were found. |
| Uniform-byte rotor search | `00^P` and `aa^P` return the complete continuation state to its initial value. This produces practical collisions, second preimages, and infinite collision families. |

The central lesson is that good randomness and avalanche measurements are not
evidence of collision resistance. A structured input family can remain
invisible to large random test campaigns.

## Algorithm outline

1. Initialize a 16×16 grid with prime-derived values.
2. Convert each input byte into four directions and update the visited cells,
   colours, prime indices, and cursor position.
3. Apply the message-dependent grid operations for the configured rounds.
4. Compress the final 16,384-bit value state into 64–512 output bits with the
   additive multiply-add-rotate extractor.

Default parameters:

| Parameter | Value |
|-----------|-------|
| Grid | 16×16 cells |
| Cell width | 64 bits |
| Output | 512 bits |
| Rounds | 10 |
| Prime table | 88,801 entries |

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- -j
```

The main executable is `build/Secasy.exe` on Windows and `build/Secasy` on
Unix-like systems.

## Usage

```bash
# File
./build/Secasy -f document.pdf

# String
./build/Secasy -s "Hello, World!"

# Raw hexadecimal bytes
./build/Secasy -x "0x45,0x47,0x78"

# Different output size and round count
./build/Secasy -s "test" -n 256 -r 20
```

Important options:

| Option | Meaning |
|--------|---------|
| `-f` | Hash a file |
| `-s` | Hash a string |
| `-x` | Hash hexadecimal bytes |
| `-n` | Output size in bits |
| `-r` | Processing rounds |
| `-i` | Maximum prime index |
| `-p` | Read lines from standard input |
| `-h` | Show help |

## Reproduce the break

Find and verify the rotor cycle:

```powershell
python scripts/python/up_rotor_cycle_search.py `
  --write-dir build/collision-proof `
  --oracle build/Secasy.exe `
  --result build/collision-proof/result.json
```

Create and verify a second preimage for an arbitrary file:

```powershell
python scripts/python/secasy_second_preimage.py `
  --file path/to/message.bin `
  --output build/message-second-preimage.bin `
  --result build/message-second-preimage.json
```

Use `--prefix-byte aa` to select the second known neutral prefix.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

The repository contains statistical, differential, structural, collision,
fuzzing, performance, and test-vector targets. The attack tools verify their
candidates independently against the production executable.

## Further details

- [Algorithm description](docs/en/ALGORITHM.md)
- [Security notes](docs/en/SECURITY_NOTES.md)
- [Differential-analysis brief](docs/en/DIFFERENTIAL_ANALYSIS_BRIEF.md)
- [Attack research plan](tests/analysis/ATTACK_RESEARCH_PLAN.md)
- [Analysis tools](scripts/README.md)
- [Reference test vectors](TEST_VECTORS.md)
