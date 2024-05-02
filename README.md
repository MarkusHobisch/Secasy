# Secasy

## _A Cryptographic Hash Function_

## Approach

Secasy is a cryptographic hash function with its main target to combine security and simplicity. It uses a new approach on how to calculate the hash value and does not use any wide spread concepts like Merkle–Damgård construction. Instead of dividing the input in different blocks it uses a 2-dimensional field to calculate all its values from the input. Due to this process different inputs result in different values leading to different calculations and hash values. 

The algorithm is based on the principle of a deterministic chaotic system, meaning that a small deviation of the input will lead to unpredictable end results. This also makes it as hard as possible for attackers to find successful attack vectors. All common ways of attacking hash functions were taken into account during the design process.


## Compilation

To compile the source code, use the following command in the terminal:

```bash
gcc -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
```

Precompiled binaries for Windows and Linux are provided and can be found in the bin folder.

Tested on Windows platform and Windows WSL (Ubuntu 22.04.1 LTS). \
GCC version 11.2.0 (Ubuntu 11.2.0-19ubuntu1)

### Additional Instructions for Windows Users:
If you are using Windows and prefer to compile the code within the Windows Subsystem for Linux (WSL), it is recommended to use Windows PowerShell to launch WSL. Prefix the GCC command with `wsl` to ensure it executes under Linux compatibility within Windows, as shown below:

```bash
wsl gcc -Ofast -march=native -mtune=native -funroll-loops *.c -lm -o secasy
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

## Security Disclaimer

Please be advised that the cryptographic functionality implemented in this software has not yet been reviewed by security professionals. 
It is intended for research and development purposes only and should be used with caution. 
Users are encouraged to conduct their own security assessments before deploying this software in a production environment. 
We welcome contributions from the community, especially in terms of security improvements and reviews.

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
