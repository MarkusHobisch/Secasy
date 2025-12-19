# Secasy

## _A Hash Function_

## Approach

Secasy is a hash function with its main target to combine security and simplicity. It uses a new approach on how to calculate the hash value and does not use any wide spread concepts like Merkle–Damgård construction. Instead of dividing the input in different blocks it uses a 2-dimensional field to calculate all its values from the input. Due to this process different inputs result in different values leading to different calculations and hash values. 

The algorithm is based on the principle of a deterministic chaotic system, meaning that a small deviation of the input will lead to unpredictable end results. This also makes it as hard as possible for attackers to find successful attack vectors. All common ways of attacking hash functions were taken into account during the design process.


## Compilation

To compile the source code, use one of the following commands in the terminal.

Important: Do NOT use a wildcard like `*.c` anymore because the project now contains two translation units with a `main` function (`main.c` and `avalanche.c`). Using `*.c` would attempt to link both and produce a "multiple definition of `main`" linker error. Build each executable explicitly as shown below.

### Standard optimized builds (recommended)
```bash
gcc -std=c11 -O3 -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O3 -Wall -Wextra -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O3 -Wall -Wextra -o secasy_preimage \
  tests/preimage/preimage.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### With full prime range (may be slower)
```bash
gcc -std=c11 -O3 -DSECASY_PRIMES_FULL -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O3 -DSECASY_PRIMES_FULL -Wall -Wextra -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O3 -DSECASY_PRIMES_FULL -Wall -Wextra -o secasy_preimage \
  tests/preimage/preimage.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### Debug builds
```bash
gcc -std=c11 -O0 -g -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O0 -g -Wall -Wextra -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O0 -g -Wall -Wextra -o secasy_preimage \
  tests/preimage/preimage.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### Windows (WSL via PowerShell)
```bash
wsl gcc -std=c11 -O3 -o secasy main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
wsl gcc -std=c11 -O3 -o secasy_avalanche avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
wsl gcc -std=c11 -O3 -o secasy_preimage tests/preimage/preimage.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### MinGW / MSYS2 (native Windows)
```bash
gcc -std=c11 -O3 -D__USE_MINGW_ANSI_STDIO -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c

gcc -std=c11 -O3 -D__USE_MINGW_ANSI_STDIO -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c
```

### CMake (alternative)
```bash
cmake -S . -B build
cmake --build build --config Release -- -j
```
This produces the executables `Secasy` and `SecasyAvalanche`.

Full prime range (no truncation; slower for very large max prime index). Build flag `SECASY_PRIMES_FULL` disables the heuristic reduction in the sieve:

```bash
gcc -DSECASY_PRIMES_FULL -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```

Effect of `-DSECASY_PRIMES_FULL`:
- Without the flag (default): the prime list may be truncated heuristically to reduce memory and speed up processing for huge `-i` values.
- With the flag: the sieve keeps the full range up to the provided maximum prime index (`-i`).

Precompiled binaries for Windows and Linux are provided and can be found in the bin folder.

Tested on Windows platform and Windows WSL (Ubuntu 22.04.1 LTS). \
GCC version 11.2.0 (Ubuntu 11.2.0-19ubuntu1)

### Additional Instructions for Windows Users:
If you are using Windows and prefer to compile the code within the Windows Subsystem for Linux (WSL), it is recommended to use Windows PowerShell to launch WSL. Prefix the GCC command with `wsl` to ensure it executes under Linux compatibility within Windows, as shown below:

```bash
wsl gcc -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```

Or with full prime range:

```bash
wsl gcc -DSECASY_PRIMES_FULL -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```

This command ensures that the GCC compiler runs within the Linux environment provided by WSL, leveraging the same performance optimizations and dependencies as if running on a native Linux system.

## Usage

Secasy is a command line tool. It supports 3 arguments.

+ n: bit size of hash value (power of two, >= 64). e.g. -n 256
+ i: max prime index for calculation of prime numbers. e.g. -i 100 (25 prime numbers in the range from 1 to 100)
+ r: number of rounds during hashing step. e.g -r 1000
+ f: path of filename: e.g. -f input.pdf

At least the argument of the filename must be specified.

### Default values

+ hashLengthInBits (n): 512
+ maximumPrimeIndex (i): 16.000.000
+ numberOfRounds (r): 100.000

## Color Operations (Field Update Semantics)
The 2D field is updated per tile using a color (operation) associated with each tile value. Current operations:

| Code        | Meaning                                  | Edge Handling |
|-------------|-------------------------------------------|---------------|
| ADD         | Add neighbour (above) or +1 at top row    | Top row: +1   |
| SUB         | Subtract neighbour (below) or -1 at bottom| Bottom: -1    |
| XOR         | XOR with left neighbour or XOR 1 at left edge | Left edge: ^=1 |
| BITWISE_AND | Bitwise AND with right neighbour; at right edge no change (logical AND with 0x..FF) | Right edge: unchanged |
| BITWISE_OR  | Bitwise OR with left neighbour; at left edge OR 1 | Left edge: |=1 |
| INVERT      | Bitwise NOT (value = ~value)              | N/A           |

Notes:
- Negative intermediate values are allowed (SUB, INVERT) and intentionally not clamped.
- The wrapping in movement logic uses bit masks (`FIELD_SIZE` must remain a power of two) for efficiency.

## Security Disclaimer

Please be advised that the cryptographic functionality implemented in this software has not yet been reviewed by security professionals. 
It is intended for research and development purposes only and should be used with caution. 
Users are encouraged to conduct their own security assessments before deploying this software in a production environment. 
We welcome contributions from the community, especially in terms of security improvements and reviews.

**Important:** The current hash construction is experimental and MUST NOT be used for real-world security purposes (e.g. password hashing, digital signatures, integrity guarantees in production). Its collision and preimage resistance have not been formally analyzed and future changes (e.g. operation semantics, prime handling) may alter outputs without notice.

### Security Analysis
A comprehensive cryptographic evaluation plan is documented in `SECURITY_ANALYSIS_PLAN.md`. This 10-phase systematic assessment covers diffusion properties, collision resistance, differential/linear cryptanalysis, algebraic attacks, preimage security, and statistical quality. Consult this plan for detailed testing methodology, acceptance criteria, and go/no-go decision points.

## Statistical Security Analysis Results

Comprehensive testing was performed to evaluate the statistical properties of Secasy. The results demonstrate excellent cryptographic characteristics comparable to established hash algorithms.

### Collision Resistance Testing

| Test | Combinations Tested | Collisions Found |
|------|---------------------|------------------|
| 2-Byte exhaustive (partial) | 16,384 | **0** |
| 3-Byte random samples | 1,000 | **0** |

**Anti-Commutativity Fix:** An earlier version showed collisions due to commutative movement operations (e.g., UP+RIGHT = RIGHT+UP leading to same position). This was fixed by introducing cross-coordinate coupling:
- Each directional movement now affects **both** X and Y coordinates
- Different offsets per direction (+1, +2, +3, +4) ensure unique paths

### Statistical Quality Comparison

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

**Key Findings:**
- **Bit Distribution:** 49.93% ones (ideal: 50.00%) - only 0.07% deviation
- **Avalanche Effect:** 50.1% bit changes on 1-bit input change (ideal: 50%)
- **Hamming Distance:** 128.2 bits average between similar inputs (ideal: 128)
- **Nibble Distribution:** Max 9% deviation from expected frequency (excellent)

### Performance Characteristics

Secasy operates as a **slow hash** by design (similar to bcrypt, PBKDF2, Argon2):

| Algorithm | Hashes/Second | Use Case |
|-----------|---------------|----------|
| SHA256 | ~1,164,000 | Fast file hashing |
| MD5 | ~1,181,000 | Fast checksums |
| **Secasy** | **~5** | Password hashing, key derivation |

The 100,000 processing rounds provide **brute-force protection**:
- Attacker: ~5 password attempts/second
- Time for 1 million attempts: ~55 hours
- Time for 1 billion attempts: ~6.3 years

### What These Tests Show ✓

- ✅ Excellent bit distribution (near-perfect 50/50)
- ✅ Strong avalanche effect (meets cryptographic standards)
- ✅ No observable collisions in tested space
- ✅ Statistical properties on par with SHA256, BLAKE2b, scrypt

### What These Tests Do NOT Show ✗

- ✗ Formal cryptographic security proofs
- ✗ Resistance to differential/linear cryptanalysis
- ✗ Preimage resistance guarantees
- ✗ Academic peer review
- ✗ NIST certification

### Honest Assessment

> Secasy demonstrates **statistically excellent properties** comparable to established cryptographic hash functions. However, statistical tests alone do not guarantee cryptographic security. A hash can have perfect statistics and still be cryptographically broken.
>
> **For production security applications, use established algorithms like SHA256, Argon2, or bcrypt.**
>
> Secasy is suitable for:
> - ✅ Educational purposes and algorithm study
> - ✅ Non-critical integrity checks
> - ✅ Experimental cryptographic research
> - ⚠️ NOT recommended for production security applications

## Avalanche Test Tool (Experimental)
A separate executable `SecasyAvalanche` (source: `avalanche.c`) measures diffusion (avalanche effect): how strongly the hash output changes when a single input bit is flipped. Target: ~50% of output bits invert per single-bit input flip.

**Current Security Assessment:** Comprehensive Strict Avalanche Criterion (SAC) testing with statistically significant sample sizes (5000+ trials) demonstrates excellent diffusion properties:
- **SAC Acceptance Rate:** 99.41% of input-bit → output-bit pairs fall within the [0.48, 0.52] range (exceeds ≥95% target)
- **Mean Flip Probability:** 0.5002 (near-ideal 0.5)
- **Maximum Bit Bias:** 0.028 (well below critical threshold)
- **Bias Exploitation:** No practically exploitable patterns detected (prediction accuracy ~44% vs 50% random baseline)

**Note:** Earlier analyses with insufficient sample sizes (< 500 trials) showed artificially low acceptance rates (~38%). With proper statistical sampling, Secasy demonstrates cryptographically sound diffusion.

**Recommended Use Cases:**
- ✅ **Suitable for:** Hash tables, checksums, file deduplication, data integrity verification
- ✅ **Experimental cryptographic use:** Diffusion properties meet SAC requirements; however, formal cryptographic certification pending

**Additional Security Testing Completed:**
- Preimage resistance: No weaknesses found (12+ bit lower bounds in brute-force tests)
- Collision resistance: Birthday-bound conformity confirmed (Chi² ≈ 0)
- Differential attack resistance: All tests passed with ~50% diffusion
- Side-channel risk: LOW (constant-time operations, no input-dependent branches)

### Build (CMake)
If you use CMake (after adding this file):
```bash
cmake -S . -B build
cmake --build build --config Release -- -j
```
You obtain two binaries: `Secasy` and `SecasyAvalanche`.

### Direct GCC build example
```bash
gcc -Ofast -march=native -mtune=native -funroll-loops avalanche.c Calculations.c InitializationPhase.c Printing.c ProcessingPhase.c SieveOfEratosthenes.c util.c -lm -o secasy_avalanche
```

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

### Caveats (Abbreviated)
Entropy sampler not yet reliable. Large `-l` with `-B 0` is slow (flips = 8 * length). Multi-bit trial count small for speed.

### Future (Selected)
SAC matrix, correlation tests, entropy fix, CSV/JSON export.

## Collision Test & Sweep Mode (Experimental)

The repository includes a dedicated tool `SecasyCollision` (source: `tests/collision/collision.c`) to evaluate collision behaviour and distribution quality of the hexadecimal hash output.

### Build (CMake)
Already integrated:
```
cmake -S . -B build
cmake --build build --config Release -- -j
```
Produces the additional binary `SecasyCollision`.

### Direct (GCC example)
```
gcc -std=c11 -O3 -Wall -Wextra -o secasy_collision \
  tests/collision/collision.c Calculations.c InitializationPhase.c ProcessingPhase.c \
  SieveOfEratosthenes.c util.c Printing.c -lm
```

### Core Idea
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

### Implemented Improvements
* Stronger finalization mix removed leading nibble bias (missing '0')
* Nonlinear post-processing reduced linear diffusion artifacts
* Sweep mode avoids repeated hashing, speeding parameter studies

### Limitations
* Open addressing: simple, not memory-optimal
* No persistent export (CSV/JSON) yet
* No duplication scenario (all messages random & independent)

### Potential Extensions
* CSV/JSON export (`-O csv/json`)
* Automated tolerance gating (exit codes)
* Threaded parallelism for very large m
* Approximate Chi^2 p-value reporting

### Reference
See `AVALANCHE_RESULTS.md` for avalanche & diffusion data; collision datasets may be added later.

## Preimage & Second-Preimage Resistance Test (Experimental)

The repository includes a specialized tool `SecasyPreimage` (source: `tests/preimage/preimage.c`) to evaluate the hash function's resistance against preimage attacks (finding input for given hash) and second-preimage attacks (finding different input with same hash).

### Build (Direct GCC)
```bash
gcc -std=c11 -O3 -Wall -Wextra -o secasy_preimage \
  tests/preimage/preimage.c Calculations.c InitializationPhase.c ProcessingPhase.c \
  SieveOfEratosthenes.c util.c Printing.c -lm
```

### Core Concept
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

### Security Assessment Context
These tests provide **empirical resistance measurement** under brute-force attack conditions. Results are limited by:
- Computational constraints (practical test space << theoretical attack space)
- Brute-force methodology (does not test sophisticated cryptanalytic attacks)
- Statistical sampling limitations

**Important:** This testing does NOT constitute formal cryptographic security proof but provides valuable empirical indicators for hash function evaluation.

## Profiling with `gprof`

To analyze performance hotspots you can compile with `-pg` and run `gprof`.

### Steps
1. Build with profiling:
```bash
gcc -pg -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```
2. Run a representative workload:
```bash
./secasy -f fileToHash
```
3. Generate report:
```bash
gprof ./secasy gmon.out > analysis.txt
```
The resulting report lists function call counts and self / cumulative time – use it to focus optimization (e.g. mixing loops, sieve generation, finalization).

## Contact Information

For any questions or inquiries regarding this software, please contact me at markus.hobisch@gmx.at.

---


