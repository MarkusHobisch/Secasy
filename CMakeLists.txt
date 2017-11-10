cmake_minimum_required(VERSION 3.8)
project(Secasy)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c BlockOne.c BlockTwo.c SieveOfEratosthenes.c BlockOne.h)
add_executable(Secasy ${SOURCE_FILES})