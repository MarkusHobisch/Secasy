# Secasy

## _A Cryptographic Hash Function_

## Approach

Secasy is a cryptographic hash function with its main target to combine security and simplicity. It uses a new approach on how to calculate the hash value and does not use any wide spread concepts like Merkle–Damgård construction. Instead of dividing the input in different blocks it uses a 2-dimensional field to calculate all its values from the input. Due to this process different inputs result in different values leading to different calculations and hash values. 

The algorithm is based on the principle of a deterministic chaotic system, meaning that a small deviation of the input will lead to unpredictable end results. This also makes it as hard as possible for attackers to find successful attack vectors. All common ways of attacking hash functions were taken into account during the design process.


## Compilation

+ gcc -Ofast *.c *.h -lm -o secasy

Tested on Windows platform and Windows WSL (Ubuntu 22.04.1 LTS). \
GCC version 11.2.0 (Ubuntu 11.2.0-19ubuntu1)

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

