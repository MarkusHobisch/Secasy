# Secasy

## _A Cryptographic Hash Function_

## Approach

Secasy is a cryptographic hash function with its main target to combine security and simplicity. It uses a new approach on how to calculate the hash value and does not use any wide spread concepts like Merkle–Damgård construction. Instead of dividing the input in different blocks it uses a 2-dimensional field to calculate all its values from the input. Due to this process different inputs result in different values leading to different calculations and hash values. 

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
```

### With full prime range (may be slower)
```bash
gcc -std=c11 -O3 -DSECASY_PRIMES_FULL -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O3 -DSECASY_PRIMES_FULL -Wall -Wextra -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### Debug builds
```bash
gcc -std=c11 -O0 -g -Wall -Wextra -o secasy \
  main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm

gcc -std=c11 -O0 -g -Wall -Wextra -o secasy_avalanche \
  avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
```

### Windows (WSL via PowerShell)
```bash
wsl gcc -std=c11 -O3 -o secasy main.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
wsl gcc -std=c11 -O3 -o secasy_avalanche avalanche.c Calculations.c InitializationPhase.c ProcessingPhase.c SieveOfEratosthenes.c util.c Printing.c -lm
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

+ n: bit size of hash value. e.g. -n 1024
+ i: max prime index for calculation of prime numbers. e.g. -i 100 (25 prime numbers in the range from 1 to 100)
+ r: number of rounds during hashing step. e.g -r 1000
+ f: path of filename: e.g. -f input.pdf

At least the argument of the filename must be specified.

### Default values

+ numberOfBits (n): 512
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

## Avalanche Test Tool (Experimental)
A separate executable `SecasyAvalanche` (source: `avalanche.c`) measures diffusion (avalanche effect): how strongly the hash output changes when a single input bit is flipped. Target: ~50% of output bits invert per single-bit input flip.

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
./SecasyAvalanche -m <messages> -l <lenBytes> -B <bitFlipsPerMessage> -r <rounds> -n <hashBufferChars> -i <maxPrimeIndex> -s <seed>
```
Parameter descriptions:
- `-m` Number of random base messages (default 50)
- `-l` Length of each input (bytes, default 64)
- `-B` Bit flips per message (0 = flip all bits sequentially; default 64). Sampling reduces runtime.
- `-r` Rounds in the core (same meaning as main tool)
- `-n` Size of the internal hash character buffer (not a strict bit length; legacy naming). Recommended: 512, 1024, ...
- `-i` Maximum prime index
- `-s` Seed (omit for time-based)

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
| -n | Internal hash buffer size (characters) |
| -i | Maximum prime index |
| -s | Seed (deterministic RNG) |
| -H | Histogram buckets (classic avalanche ratio) |
| -X | Extended mode features (bias, entropy, multi-bit) |

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
  -m <messages>      # Number of random messages (default 5000)\n  -l <lenBytes>      # Message length in bytes (default 64)\n  -r <rounds>        # Core rounds (same meaning as main hash)\n  -n <hashBufBits>   # Internal hash buffer (chars *4 ≈ upper bit bound)\n  -s <seed>          # Seed for deterministic RNG\n  -T <truncBits>     # Use only first <truncBits> bits for collision detection\n  -F                 # Global hex symbol frequency + Chi^2 (0-f)\n  -P                 # Positional hex frequency + Chi^2 per position\n  -p <pos>           # Detailed single position output\n  -B <nBytes>        # Leading byte distribution + Chi^2 (256 classes)\n  -X <list>          # Sweep: multiple truncation bit sizes (comma separated)
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


