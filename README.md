# Secasy

## _A Grid-Based Cryptographic Hash Function_

## Abstract

Secasy is a cryptographic hash function designed to combine security with simplicity. Unlike traditional hash functions based on the Merkle–Damgård construction, Secasy introduces a novel grid-based approach using a two-dimensional 16×16 field (256 cells) as its core state structure. Instead of processing input in sequential blocks, the algorithm employs spatial diffusion through directional movement across the grid, where each input byte influences the traversal path and cell operations.

The algorithm operates as a deterministic chaotic system, where minimal input variations produce unpredictable output changes—a property essential for cryptographic security. This spatial approach provides natural resistance against common attack vectors by ensuring that input differences propagate through multiple dimensions of the state space. Empirical evaluation demonstrates statistical properties comparable to established hash functions (SHA-256, BLAKE2b), with a 99.41% Strict Avalanche Criterion acceptance rate and near-ideal bit distribution (49.93% ones).

**Keywords:** Hash function, grid-based cryptography, spatial diffusion, avalanche effect, collision resistance


## 1. Introduction

Traditional cryptographic hash functions like SHA-256 and MD5 rely on the Merkle–Damgård construction, processing input as sequential blocks through iterative compression functions. While proven effective, this linear approach has known structural weaknesses, including length extension attacks.

Secasy explores an alternative paradigm: a **grid-based state structure** where diffusion occurs spatially across a two-dimensional field rather than sequentially through blocks.

```
Traditional:               Secasy:
                        
Input → [Block1] →         ┌─────────────────┐
        [Block2] →         │ ● → ← ↓ ↑ → ... │  16×16 Grid
        [Block3] →         │ ↓   ↑   →   ↓   │  with prime-driven
        ...                │ ...             │  traversal
        → Hash             └─────────────────┘
                                 ↓
                               Hash
```

This approach offers several theoretical advantages:

- **Multi-dimensional diffusion**: Changes propagate in four directions (up, down, left, right)
- **Position-dependent operations**: Six distinct operations (ADD, SUB, XOR, AND, OR, INVERT) applied based on cell state
- **Prime-number driven initialization**: Deterministic chaos seeded by prime number sequences

## 2. Algorithm Design

### 2.1 Grid Architecture

The core state consists of a 16×16 grid (256 cells), where each cell holds an integer value. Input bytes determine:
1. **Traversal direction** through the grid
2. **Operation selection** at each cell
3. **Value modifications** affecting neighboring cells

### 2.2 Grid Operations

The 2D field is updated per tile using a color (operation) associated with each tile value:

| Code        | Operation                                 | Edge Handling |
|-------------|-------------------------------------------|---------------|
| ADD         | Add neighbour (above) or +1 at top row    | Top row: +1   |
| SUB         | Subtract neighbour (below) or -1 at bottom| Bottom: -1    |
| XOR         | XOR with left neighbour or XOR 1 at left edge | Left edge: ^=1 |
| BITWISE_AND | Bitwise AND with right neighbour          | Right edge: unchanged |
| BITWISE_OR  | Bitwise OR with left neighbour            | Left edge: \|=1 |
| INVERT      | Bitwise NOT (value = ~value)              | N/A           |

**Notes:**
- Negative intermediate values are allowed (SUB, INVERT) and intentionally not clamped
- Wrapping in movement logic uses bit masks (`FIELD_SIZE` must remain a power of two)

### 2.3 Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| Hash Length (n) | 512 bits | Output size (power of two, ≥64) |
| Prime Index (i) | 16,000,000 | Upper bound for prime generation |
| Rounds (r) | 100,000 | Processing iterations |

## 3. Implementation

### 3.1 Compilation

**CMake (recommended):**
```bash
cmake -S . -B build
cmake --build build --config Release -- -j
```
Produces executables: `Secasy`, `SecasyAvalanche`, `SecasyCollision`, `SecasyPreimage`

**Direct GCC (alternative):**
```bash
gcc -std=c11 -O3 -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

**Windows users:** Prefix commands with `wsl` when using WSL (e.g., `wsl gcc ...`)

### 3.2 Usage

Secasy is a command line tool supporting the following arguments:

| Flag | Description | Example |
|------|-------------|---------|
| `-n` | Hash output size in bits (power of two, ≥64) | `-n 256` |
| `-i` | Maximum prime index | `-i 100` |
| `-r` | Number of processing rounds | `-r 1000` |
| `-f` | Input file path | `-f input.pdf` |

At least the filename (`-f`) must be specified.

## 4. Security Analysis

> **Disclaimer:** This implementation has not been reviewed by security professionals and is intended for research purposes only. The hash construction is experimental and MUST NOT be used for production security applications (password hashing, digital signatures, integrity guarantees). Formal collision and preimage resistance have not been proven.

Comprehensive empirical testing was performed to evaluate the cryptographic properties of Secasy. A detailed 10-phase evaluation plan is documented in `SECURITY_ANALYSIS_PLAN.md`.

### 4.1 Statistical Quality Results

#### Collision Resistance

| Test | Samples | Collisions |
|------|---------|------------|
| 2-Byte exhaustive | 16,384 | **0** |
| 3-Byte random | 1,000 | **0** |

**Anti-Commutativity:** Cross-coordinate coupling ensures directional movements produce unique paths (+1, +2, +3, +4 offsets per direction).

#### Comparative Analysis

Secasy was compared against industry-standard hash algorithms:

| Algorithm | Hash Bits | Bit Distribution | Avalanche Effect | Deviation from Ideal |
|-----------|-----------|------------------|------------------|---------------------|
| BLAKE2b | 256 | 50.01% | 50.0% | 0.03% |
| scrypt | 256 | 51.07% | 50.0% | 0.04% |
| MD5 | 128 | 50.91% | 50.0% | 0.04% |
| SHA512 | 512 | 50.18% | 49.9% | 0.06% |
| SHA3-256 | 256 | 50.28% | 49.9% | 0.06% |
| PBKDF2 | 256 | 49.86% | 49.9% | 0.07% |
| **Secasy** | **256** | **49.93%** | **50.1%** | **0.10%** |
| SHA256 | 256 | 49.87% | 50.2% | 0.21% |

**Key Metrics:**
- **Bit Distribution:** 49.93% ones (ideal: 50.00%) — 0.07% deviation
- **Avalanche Effect:** 50.1% bit changes per 1-bit input flip (ideal: 50%)
- **Hamming Distance:** 128.2 bits average (ideal: 128 for 256-bit output)
- **SAC Acceptance:** 99.41% of bit pairs within [0.48, 0.52] range

### 4.2 Performance Characteristics

Secasy operates as a **slow hash** by design, similar to password hashing functions:

| Algorithm | Hashes/sec | Category |
|-----------|------------|----------|
| SHA256 | ~1,164,000 | Fast |
| MD5 | ~1,181,000 | Fast |
| **Secasy** | **~11** | Slow (configurable) |

### 4.3 Summary of Findings

| Property | Status | Evidence |
|----------|--------|----------|
| Bit Distribution | ✅ Excellent | 49.93% (0.07% from ideal) |
| Avalanche Effect | ✅ Strong | 50.1% mean flip rate |
| Collision Resistance | ✅ Empirical | 0 collisions in tested space |
| SAC Compliance | ✅ High | 99.41% acceptance rate |
| Formal Proofs | ❌ None | Not formally analyzed |
| Peer Review | ❌ None | Awaiting review |
| NIST Certification | ❌ No | Not submitted |

### 4.4 Recommended Use Cases

| Application | Suitability |
|-------------|-------------|
| Educational study | ✅ Recommended |
| Algorithm research | ✅ Recommended |
| Non-critical integrity checks | ✅ Suitable |
| File deduplication | ✅ Suitable |
| Password hashing | ⚠️ Use established alternatives |
| Digital signatures | ❌ Not recommended |
| Production security | ❌ Not recommended |

## 5. Test Tools

### 5.1 Avalanche Test (SecasyAvalanche)

Measures diffusion quality: how strongly hash output changes when a single input bit flips. Target: ~50% of output bits should invert.

**Strict Avalanche Criterion (SAC) Results:**
| Metric | Value | Target |
|--------|-------|--------|
| Acceptance Rate | 99.41% | ≥95% |
| Mean Flip Probability | 0.5002 | 0.5 |
| Maximum Bit Bias | 0.028 | <0.05 |

### Usage
```
./SecasyAvalanche -m <messages> -l <lenBytes> -B <bitFlipsPerMessage> -r <rounds> -n <bits> -i <maxPrimeIndex> -s <seed> -S <file>
```
Parameter descriptions:
- `-m` Number of random base messages (default 50)
- `-l` Length of each input (bytes, default 64)
- `-B` Bit flips per message (0 = flip all bits sequentially; default 64). Sampling reduces runtime.
- `-r` Rounds in the core (same meaning as main tool)
- `-n` Hash output bit size (power of two, >= 64). e.g. 64, 128, 256, 512
- `-i` Maximum prime index
- `-s` Seed (omit for time-based)
- `-S` **NEW:** Export Strict Avalanche Criterion (SAC) matrix to CSV file. Measures per-input-bit → per-output-bit flip probabilities for detailed diffusion analysis.

### Output (Core Metric)
Mean avalanche rate = flipped output bits / total compared bits (ideal ≈ 0.5). Extended mode `-X` adds per‑bit statistics and multi‑bit trial summaries.

For a concise empirical summary and current analytical limitations see `AVALANCHE_RESULTS.md` (short report). The hash remains experimental; positive avalanche results alone DO NOT imply full cryptographic security.

### Extended Avalanche Mode (-X)
Adds per-bit flip distribution, (experimental) entropy sampler, and multi-bit (k=2,4,8) diffusion checks.

### Quick Extended Run (example)
```bash
./SecasyAvalanche -X -m 5 -l 64 -B 32 -r 5000 -s 123
```

### Interpreting Extended Output
Per-bit bias: flip rates near 0.5 (few out-of-band) desirable. Multi-bit rates should stay ~0.5. Entropy currently placeholder pending sampler fix.

### WSL Batch Script
A helper script is provided at `scripts/run_extended_avalanche_wsl.sh` to build and execute a suite of representative runs.

Usage:
```bash
chmod +x scripts/run_extended_avalanche_wsl.sh
./scripts/run_extended_avalanche_wsl.sh
```
Environment overrides:
```
BUILD_TYPE=Debug BUILD_DIR=build-debug JOBS=4 ./scripts/run_extended_avalanche_wsl.sh
```

### Typical Flags Recap
| Flag | Meaning |
|------|---------|
| -m | Number of base messages |
| -l | Input length (bytes) |
| -B | Bit flips per message (0 = all bits sequentially) |
| -r | Rounds of the core hash |
| -n | Hash output bit size (power of two, >= 64) |
| -i | Maximum prime index |
| -s | Seed (deterministic RNG) |
| -H | Histogram buckets (classic avalanche ratio) |
| -X | Extended mode features (bias, entropy, multi-bit) |
| **-S** | **SAC matrix export to CSV (input×output bit flip probabilities)** |

### SAC Matrix Analysis (Advanced)
The Strict Avalanche Criterion matrix provides detailed insight into diffusion quality:

```bash
# Production-level SAC analysis (recommended for final assessment)
./SecasyAvalanche -m 100 -l 16 -B 0 -r 10000 -S sac_analysis.csv

# Quick SAC test
./SecasyAvalanche -m 20 -l 8 -r 1000 -S sac_quick.csv
```

**SAC Matrix Output:**
- CSV format: rows = input bits, columns = output bits
- Values: flip probabilities (0.0 to 1.0, ideal ≈ 0.5)
- Statistics: mean, min, max, acceptance rate ([0.48, 0.52] band)
- **Verified Status:** 99.41% acceptance rate achieved with 5000+ trials (exceeds 95% target)

**Note on Sample Size:** SAC measurements require statistically significant sample sizes (≥1000 trials recommended) for accurate results. Small sample sizes may produce misleading variance.

### Notes
- Large `-l` with `-B 0` is slow (flips = 8 × length)
- Multi-bit trial count small for speed
- SAC measurements require ≥1000 trials for statistical significance

### 5.2 Collision Test (SecasyCollision)

Evaluates collision behaviour and distribution quality of the hash output.

#### Core Idea
Generate `m` random messages (fixed length), hash each, insert the hexadecimal string into an open-addressed table. Inserting an already seen (possibly truncated) hash string counts as a collision. Optional analytical modes (hex frequency, positional frequency, byte distribution, truncation, sweep) provide diagnostics.

### Invocation (primary flags)
```
./SecasyCollision \
  -m <messages>      # Number of random messages (default 5000)\n  -l <lenBytes>      # Message length in bytes (default 64)\n  -r <rounds>        # Core rounds (same meaning as main hash)\n  -n <bits>          # Hash output bit size (power of two, >= 64)\n  -s <seed>          # Seed for deterministic RNG\n  -T <truncBits>     # Use only first <truncBits> bits for collision detection\n  -F                 # Global hex symbol frequency + Chi^2 (0-f)\n  -P                 # Positional hex frequency + Chi^2 per position\n  -p <pos>           # Detailed single position output\n  -B <nBytes>        # Leading byte distribution + Chi^2 (256 classes)\n  -X <list>          # Sweep: multiple truncation bit sizes (comma separated)
```

### Sweep Mode (-X)
`-X` evaluates multiple truncation widths in a single run (hashes computed once then reused). Example:
```
./SecasyCollision -m 20000 -l 32 -r 5 -n 256 -X 16,20,24,28,32 -s 12345
```
Example output (abridged):
```
Sweep results (messages=20000 lenBytes=32 rounds=5 hashParamBits=256):
  Bits= 16  Collisions=2838     Unique=17162    Rate=0.14190000  Expected~3052   Birthday~321
  Bits= 20  Collisions=176      Unique=19824    Rate=0.00880000  Expected~191    Birthday~1283
  Bits= 24  Collisions=15       Unique=19985    Rate=0.00075000  Expected~11.9   Birthday~5134
  Bits= 28  Collisions=0        Unique=20000    Rate=0.00000000  Expected~0.75   Birthday~20534
  Bits= 32  Collisions=0        Unique=20000    Rate=0.00000000  Expected~0.05   Birthday~82137
```
Columns:
* Bits: Effective examined space (first bits of hash; 4 per hex symbol)\n* Collisions: Observed collision count (same truncated string)\n* Unique: Number of distinct truncated hashes\n* Rate: Collisions / messages (heuristic)\n* Expected: m(m-1)/(2·2^k) (birthday approximation)\n* Birthday: Approx threshold ~ sqrt(pi * 2^k / 2)

### Theoretical Background (brief)
For uniform outputs (size 2^k) and m draws:

  E[Collisions] ≈ m(m-1) / (2 · 2^k)

Variance ≈ expectation (Poisson for small p). Observed collisions should typically fall within ±2√E; systematic deviation signals bias.

### Why Truncation (-T / Sweep)?
Full 256-bit collisions are practically unobservable for m ≤ 10^6. Truncation (e.g. 24–32 bits) produces a denser collision space enabling statistical verification.

### Additional Analysis Flags
* `-F`: Global symbol distribution, Chi^2 vs uniform (df=15)
* `-P`: Positional Chi^2 per hex index (finds local hotspots)
* `-p pos`: Detailed single-position breakdown with Z-scores
* `-B n`: Leading byte distribution (256 classes) + Chi^2 (df=255)

### Example Workflows
1. Full-space sanity (expect 0 collisions):
```
./SecasyCollision -m 200000 -l 64 -r 5 -n 256 -s 42
```
2. Truncated 32-bit collision run:
```
./SecasyCollision -m 1000000 -l 32 -r 5 -n 256 -T 32 -s 99
```
3. Multi-width sweep:
```
./SecasyCollision -m 30000 -l 48 -r 5 -n 256 -X 20,24,28,32,36 -s 777
```
4. Positional & byte bias check:
```
./SecasyCollision -m 5000 -l 32 -r 5 -n 256 -P -p 0 -B 8 -s 17
```

### Interpretation & Heuristics
| Signal | Meaning | Action |
|--------|---------|--------|
| Collisions within ~±2√E | OK | None |
| Too many collisions | Space bias / correlation | Revisit finalization/mix |
| Too few collisions | Uneven space usage | Investigate distribution |
| High Chi^2 subset | Local formatting/finalization issue | Inspect finalizer |
| Missing / rare nibble | Structural bias (prefix/mix) | Rework formatting/mix |



### 5.3 Preimage Test (SecasyPreimage)

Evaluates resistance against preimage attacks (finding input for given hash) and second-preimage attacks (finding different input with same hash).

#### Concept
**Preimage Resistance:** Given hash H, computationally infeasible to find input M such that hash(M) = H  
**Second-Preimage Resistance:** Given input M1, computationally infeasible to find different M2 such that hash(M1) = hash(M2)

The tool uses brute-force search with randomized inputs to empirically measure resistance strength. For n-bit hash output, ideal resistance requires ~2^n attempts.

### Usage
```bash
./SecasyPreimage [options]
```

**Key Parameters:**
- `-a <num>` Maximum attempts per test (default: 1,000,000)
- `-l <bytes>` Input length in bytes (default: 16)
- `-r <rounds>` Hash computation rounds (default: 10)
- `-i <index>` Maximum prime index (default: 200)
- `-s <seed>` Random seed for reproducible tests
- `-P` Test only preimage resistance
- `-S` Test only second-preimage resistance  
- `-o <file>` Export results to CSV file

### Example Commands
```bash
# Quick test (recommended for development)
./SecasyPreimage -a 10000 -l 8 -i 100 -o quick_preimage.csv

# Standard evaluation 
./SecasyPreimage -a 100000 -l 12 -r 1000 -o preimage_standard.csv

# Focused preimage-only test
./SecasyPreimage -P -a 500000 -l 16 -o preimage_only.csv
```

### Interpreting Results
**Success (Attack Found):** Indicates weakness - security estimate based on attempts required  
**No Success:** Lower bound on security strength (log2 of attempts performed)  

**Example Output:**
```
Preimage Test: NOT FOUND after 100000 attempts (234.5 sec)
  Lower bound: 16.6 bits
Second-Preimage Test: NOT FOUND after 100000 attempts (245.1 sec)  
  Lower bound: 16.6 bits
```

### CSV Export Format
Results exported include test type, attempts, success status, time elapsed, success rate, and theoretical security bits.

**Note:** These tests provide empirical resistance measurement under brute-force conditions. Results do not constitute formal cryptographic security proof.

## 6. Profiling

Analyze performance with `gprof`:
```bash
gcc -pg -O3 -o secasy main.c Calculations.c InitializationPhase.c ProcessingPhase.c \
  SieveOfEratosthenes.c util.c Printing.c -lm
./secasy -f fileToHash
gprof ./secasy gmon.out > analysis.txt
```

## 7. Conclusion

Secasy demonstrates that grid-based hash function design is a viable alternative to traditional Merkle–Damgård constructions. Empirical testing shows:

- **Statistical quality** comparable to SHA-256, BLAKE2b, and scrypt
- **Strong avalanche properties** (99.41% SAC acceptance)
- **No practical vulnerabilities** detected in exploit testing
- **Configurable security/performance tradeoff** via round parameter

Future work includes formal security proofs, differential cryptanalysis, and peer review.

## 8. References

1. Merkle, R. (1989). "A Certified Digital Signature." CRYPTO '89.
2. Damgård, I. (1989). "A Design Principle for Hash Functions." CRYPTO '89.
3. Bertoni, G. et al. (2011). "The Keccak SHA-3 submission." NIST.
4. Aumasson, J.P. et al. (2013). "BLAKE2: Simpler, Smaller, Fast." ACNS.

## Contact

Markus Hobisch — markus.hobisch@gmx.at

---


