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
A separate executable `SecasyAvalanche` (source: `avalanche.c`) is included to measure diffusion (avalanche effect): How strongly does the hash output change when a single input bit is flipped? An ideal cryptographic hash changes ~50% of output bits when exactly one input bit is inverted.

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

### Output Interpretation
The tool reports among other values:
- `Mean avalanche rate`: total flipped output bits / total compared bits (target ≈ 0.5)
- Qualitative assessment band.

### Limitations
- Currently only an overall mean; no Strict Avalanche Criterion (SAC) matrix per input bit.
- No variance/Chi² test against a binomial model (unless you extended the tool locally).
- Output length is not strictly fixed by `-n` (acts as buffer cap), so measurements may be slightly biased.
- Re-initializing state per hash increases runtime.

### Planned Improvements
- SAC & per output-bit flip probabilities
- Pairwise correlation (bit independence)
- Chi² goodness-of-fit vs. Binomial(n, 0.5)
- Strict separation of desired output bit length vs. hex storage capacity

## Profiling with `gprof`

To analyze the performance of the program and identify potential bottlenecks, we use `gprof`, a performance analysis tool for Unix applications. To enable profiling, the program must be compiled with the `-pg` flag, which allows `gprof` to collect data on the program's execution. Additional flags optimize performance further and tailor the build to the specific architecture of the machine.

### Steps for Profiling:

#### 1. Compile the program with necessary options:
Compile your program with the `-pg` flag alongside other optimization flags to integrate profiling support into the executable.

**Compile command:**
```bash
gcc -pg -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```

#### 2. Run the program:
Execute the compiled program as usual. This run will generate a file named `gmon.out` in the same directory, containing profiling information.

**Run command:**
```bash
./secasy -f fileToHash
```

#### 3. Analyze the profiling data:
Use `gprof` to read the `gmon.out` file and produce an analysis report. You can redirect this output to a text file for easier examination.

**Analysis command:**
```bash
gprof ./secasy gmon.out > analysis.txt
```

### Reading the Report:
The `gprof` output in `analysis.txt` will provide a list of the functions called during the execution, along with the time spent in each function and the number of times each function was called. This information is crucial for identifying performance bottlenecks and understanding the behavior of the program.

## Contact Information

For any questions or inquiries regarding this software, please contact me at markus.hobisch@gmx.at.
