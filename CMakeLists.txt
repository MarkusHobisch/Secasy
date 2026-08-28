cmake_minimum_required(VERSION 3.16)
project(Secasy C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Include path for all targets ──────────────────────────────────────
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── Core library sources (shared by all targets) ──────────────────────
set(CORE_SOURCES
        src/Calculations.c
        src/CommandLine.c
        src/InitializationPhase.c
        src/Printing.c
        src/ProcessingPhase.c
        src/SieveOfEratosthenes.c
        src/util.c)

# ── Main executable ───────────────────────────────────────────────────
add_executable(Secasy src/main.c ${CORE_SOURCES})

# ── Test registration (CTest) ─────────────────────────────────────────
enable_testing()

# Default per-test wall-clock timeout (seconds). Generous, since several
# analysis/security tests are intentionally long-running. Override per test
# via the optional third argument to add_secasy_test().
set(SECASY_DEFAULT_TEST_TIMEOUT 600)

# ── Helper: add a test executable with standard config ────────────────
# Usage: add_secasy_test(<target> <source> [<timeout-seconds>])
# Builds the executable, registers it as a CTest test (run from the build
# directory so generated CSV/output files land where the tools expect them),
# and records the target in a global list for bulk property assignment.
function(add_secasy_test TARGET SOURCE)
    add_executable(${TARGET} ${SOURCE} ${CORE_SOURCES})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    set(_timeout ${SECASY_DEFAULT_TEST_TIMEOUT})
    if(ARGV2)
        set(_timeout ${ARGV2})
    endif()

    add_test(NAME ${TARGET} COMMAND ${TARGET}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(${TARGET} PROPERTIES TIMEOUT ${_timeout})

    set_property(GLOBAL APPEND PROPERTY SECASY_TEST_TARGETS ${TARGET})
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
add_secasy_test(SecasySingleBitCollision     tests/collision/single_bit_collision_test.c)
add_secasy_test(SecasyClassCollision         tests/collision/class_collision_test.c)
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
add_secasy_test(SecasyHashrate               tests/performance/hashrate_test.c)

# ── Round-reduction analysis ──────────────────────────────────────────
add_secasy_test(SecasyRoundReduction         tests/round_reduction/round_reduction_test.c)

# ── Fuzzing / stress testing ──────────────────────────────────────────
add_secasy_test(SecasyFuzzTest               tests/fuzzing/fuzz_test.c)
# ── Test vector generator & verifier ────────────────────────────────
add_secasy_test(SecasyTestVectors            tests/testvectors/generate_test_vectors.c)
add_secasy_test(SecasyVerifyVectors          tests/testvectors/verify_test_vectors.c)
# ── Color operation isolation / diffusion analysis ──────────────────
add_secasy_test(SecasyColorIsolation         tests/analysis/color_op_isolation.c)
# ── Field-size diffusion & symmetry sweep ────────────────────────────
add_secasy_test(SecasyFieldSizeSweep         tests/analysis/field_size_sweep.c)
# ── Collision diagnosis (debug tool) ────────────────────────────────
add_secasy_test(SecasyDebugCollision         tests/analysis/debug_collision.c)
# ── Direction collision trace & analysis ────────────────────────────
add_secasy_test(SecasyTraceCollision         tests/analysis/trace_collision.c)
# ── Cell divergence growth analysis ─────────────────────────────────
add_secasy_test(SecasyCellDivergence          tests/analysis/cell_divergence.c)
# ── Brute-force collision scan (1/2/3 byte) ─────────────────────────
add_secasy_test(SecasyBruteCollisionScan      tests/analysis/brute_collision_scan.c)
# ── Phase-2 internal-state collision scan (neutral-loop / path probe) ─
add_secasy_test(SecasyPhase2Collision         tests/analysis/phase2_collision_scan.c)
# ── Targeted attack suite (linearity / differential / cross-length) ─
add_secasy_test(SecasyLinearityAttack         tests/analysis/linearity_attack.c)
# ── Differential replay (follow-up to suspect bit in attack 2) ────
add_secasy_test(SecasyDifferentialReplay      tests/analysis/differential_replay.c)
# ── White-box structural attack (internal-state instrumentation) ──
add_secasy_test(SecasyStructuralAttack        tests/analysis/structural_attack.c)
# ── Algebraic analysis (degree growth / cube / linear, reduced model) ─
add_secasy_test(SecasyAlgebraicAnalysis       tests/analysis/algebraic_analysis.c)
# ── Regression check for the retired n=64 XOR-extractor attack ─
add_secasy_test(SecasyAlgebraicAttackN64      tests/analysis/algebraic_attack_n64.c)
# ── Message-level diffusion diagnostic for n=64 ─
add_secasy_test(SecasyMessageAttackN64        tests/analysis/message_attack_n64.c)
# Collect all test target names for bulk property assignment
# (populated automatically by add_secasy_test, so it never drifts out of sync)
get_property(ALL_TEST_TARGETS GLOBAL PROPERTY SECASY_TEST_TARGETS)

# ── Per-target properties (applied in bulk) ───────────────────────────
set(WARN_FLAGS -Wall -Wextra -Wundef -Wshadow -Wpointer-arith
               -Wstrict-prototypes -Wconversion -Wno-unused-parameter)

if (MSVC)
    target_compile_options(Secasy PRIVATE /W4 /permissive-)
else()
    target_compile_options(Secasy PRIVATE ${WARN_FLAGS})
endif()

foreach(T ${ALL_TEST_TARGETS})
    target_compile_definitions(${T} PRIVATE DEBUG_MODE=0)
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

# ── Documentation (PDF via Pandoc) ───────────────────────────────────
find_program(PANDOC pandoc)
if(PANDOC)
    set(PDOC_OPTS --pdf-engine=xelatex "-V" "papersize=a4" "-V" "mainfont=Segoe UI" "-V" "monofont=Consolas")
    set(CITE_OPTS --citeproc --bibliography=${CMAKE_SOURCE_DIR}/docs/references.bib --csl=${CMAKE_SOURCE_DIR}/docs/ieee.csl)
    set(DE_PDFS  ${CMAKE_SOURCE_DIR}/docs/de/pdfs)
    set(EN_PDFS  ${CMAKE_SOURCE_DIR}/docs/en/pdfs)

    add_custom_target(docs
        # ── German ──────────────────────────────────────────────────
        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/de/SECURITY_CONCEPTS.md
                -o ${DE_PDFS}/SECURITY_CONCEPTS.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc

        # ── English — prose documents ────────────────────────────────
        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/SECURITY_CONCEPTS.md
                -o ${EN_PDFS}/SECURITY_CONCEPTS.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/SECURITY_NOTES.md
                -o ${EN_PDFS}/SECURITY_NOTES.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/ROUND_REDUCTION_ANALYSIS.md
                -o ${EN_PDFS}/ROUND_REDUCTION_ANALYSIS.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=1.5cm" "-V" "fontsize=10pt" --toc

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/DIFFERENTIAL_ANALYSIS_BRIEF.md
                -o ${EN_PDFS}/DIFFERENTIAL_ANALYSIS_BRIEF.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/de/ALGORITHM.md
                -o ${DE_PDFS}/ALGORITHMUS.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc
                --resource-path=${CMAKE_SOURCE_DIR}/docs/de
                --resource-path=${CMAKE_SOURCE_DIR}/docs/en

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/ALGORITHM.md
                -o ${EN_PDFS}/ALGORITHM.pdf
                ${PDOC_OPTS} ${CITE_OPTS} "-V" "geometry:margin=2.5cm" "-V" "fontsize=11pt" --toc
                --resource-path=${CMAKE_SOURCE_DIR}/docs/en

        COMMAND ${PANDOC} ${CMAKE_SOURCE_DIR}/docs/en/SCRIPTS_CHEATSHEET.md
                -o ${EN_PDFS}/SCRIPTS_CHEATSHEET.pdf
                ${PDOC_OPTS} "-V" "geometry:margin=2cm" "-V" "fontsize=10pt" --toc

        COMMENT "Generating documentation PDFs into docs/*/pdfs/"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    message(STATUS "Pandoc not found — 'docs' target unavailable")
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
