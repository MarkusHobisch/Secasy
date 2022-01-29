# Secasy

## _A Cryptographic Hash Function_

Secasy is free open source software. You can download, read, use and modify every bit of source code.

## Compilation
+ gcc -Ofast *.c *.h -lm -o secasy

Tested on Windows platform and Windows WSL (Ubuntu 20.04 LTS).

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

