cmake_minimum_required(VERSION 3.16)
project(Secasy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Core library sources (shared by all targets) ──────────────────────
set(CORE_SOURCES
        Calculations.c
        InitializationPhase.c
        Printing.c
        ProcessingPhase.c
        SieveOfEratosthenes.c
        util.c)

# ── Main executable ───────────────────────────────────────────────────
add_executable(Secasy main.c ${CORE_SOURCES})

# ── Helper: add a test executable with standard config ────────────────
function(add_secasy_test TARGET SOURCE)
    add_executable(${TARGET} ${SOURCE} ${CORE_SOURCES})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# ── Security tests ────────────────────────────────────────────────────
add_secasy_test(SecasyComprehensiveSecurity  tests/security/comprehensive_security_test.c)
add_secasy_test(SecasyDeepSecurity           tests/security/test_deep_security.c)
add_secasy_test(SecasyExtendedSecurity       tests/security/extended_security_test.c)
add_secasy_test(SecasyPracticalExploit       tests/security/practical_exploit_test.c)

# ── Avalanche / diffusion ────────────────────────────────────────────
add_secasy_test(SecasyAvalanche              tests/avalanche/avalanche.c)

# ── Collision resistance ──────────────────────────────────────────────
add_secasy_test(SecasyCollision              tests/collision/collision.c)
add_secasy_test(SecasyTruncCollisionPoC      tests/collision/trunc_collision_poc.c)

# ── Differential cryptanalysis ────────────────────────────────────────
add_secasy_test(SecasyDifferential           tests/differential/differential_test.c)

# ── Statistical randomness ────────────────────────────────────────────
add_secasy_test(SecasyStatisticalRandomness  tests/statistical/statistical_randomness_test.c)
add_secasy_test(SecasyStatRigor              tests/statistical/stat_rigor_test.c)
add_secasy_test(SecasyHashPattern            tests/statistical/hash_pattern_test.c)

# ── Performance / benchmarking ────────────────────────────────────────
add_secasy_test(SecasyBenchmark              tests/performance/benchmark_rounds.c)
add_secasy_test(SecasyPreciseTiming          tests/performance/precise_timing.c)
add_secasy_test(SecasyProfiling              tests/performance/profiling.c)

# ── Round-reduction analysis ──────────────────────────────────────────
add_secasy_test(SecasyRoundReduction         tests/round_reduction/round_reduction_test.c)

# ── Fuzzing / stress testing ──────────────────────────────────────────
add_secasy_test(SecasyFuzzTest               tests/fuzzing/fuzz_test.c)
# ── Test vector generator & verifier ────────────────────────────────
add_secasy_test(SecasyTestVectors            tests/testvectors/generate_test_vectors.c)
add_secasy_test(SecasyVerifyVectors          tests/testvectors/verify_test_vectors.c)
# Collect all test target names for bulk property assignment
set(ALL_TEST_TARGETS
    SecasyComprehensiveSecurity SecasyDeepSecurity SecasyExtendedSecurity
    SecasyPracticalExploit SecasyAvalanche SecasyCollision SecasyTruncCollisionPoC
    SecasyDifferential SecasyStatisticalRandomness SecasyStatRigor SecasyHashPattern
    SecasyBenchmark SecasyPreciseTiming SecasyProfiling SecasyRoundReduction
    SecasyFuzzTest SecasyTestVectors SecasyVerifyVectors)

# ── Per-target properties (applied in bulk) ───────────────────────────
set(WARN_FLAGS -Wall -Wextra -Wundef -Wshadow -Wpointer-arith
               -Wstrict-prototypes -Wconversion -Wno-unused-parameter)

if (MSVC)
    target_compile_options(Secasy PRIVATE /W4 /permissive-)
else()
    target_compile_options(Secasy PRIVATE ${WARN_FLAGS})
endif()

foreach(T ${ALL_TEST_TARGETS})
    if (NOT WIN32)
        target_compile_definitions(${T} PRIVATE _FILE_OFFSET_BITS=64)
        target_link_libraries(${T} PRIVATE m)
    endif()
    if (MSVC)
        target_compile_options(${T} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${T} PRIVATE ${WARN_FLAGS})
        if (MINGW)
            target_compile_definitions(${T} PRIVATE __USE_MINGW_ANSI_STDIO=1)
        endif()
    endif()
endforeach()

# Main target platform flags
if (NOT WIN32)
    target_compile_definitions(Secasy PRIVATE _FILE_OFFSET_BITS=64)
    target_link_libraries(Secasy PRIVATE m)
elseif (MINGW)
    target_compile_definitions(Secasy PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()

# ── Optional: GTK3 ────────────────────────────────────────────────────
option(SECASY_WITH_GTK3 "Build with GTK3 (not required for core hash)" OFF)
if (SECASY_WITH_GTK3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(Secasy PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_directories(Secasy PRIVATE ${GTK3_LIBRARY_DIRS})
    target_compile_definitions(Secasy PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(Secasy PRIVATE ${GTK3_LIBRARIES})
endif()
