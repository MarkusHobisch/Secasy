#!/usr/bin/env bash
set -euo pipefail

# This script builds the SecasyAvalanche target under WSL and runs several extended (-X) analyses.
# It assumes you have run: sudo apt update && sudo apt install -y build-essential cmake
# Optional: export BUILD_TYPE=Release (default), export BUILD_DIR=build-wsl

BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_DIR=${BUILD_DIR:-build-wsl}
GENERATOR=${GENERATOR:-}
JOBS=${JOBS:-$(nproc)}

info() { echo -e "[INFO] $*"; }
warn() { echo -e "[WARN] $*" >&2; }

if [[ ! -f CMakeLists.txt ]]; then
  warn "Run this script from the project root (where CMakeLists.txt is)." 
  exit 1
fi

if [[ -n "${GENERATOR}" ]]; then
  GEN_ARG=(-G "${GENERATOR}")
else
  GEN_ARG=()
fi

info "Configuring (type=${BUILD_TYPE}, dir=${BUILD_DIR}) ..."
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${GEN_ARG[@]}"

info "Building SecasyAvalanche ..."
cmake --build "${BUILD_DIR}" --target SecasyAvalanche -j"${JOBS}" --config "${BUILD_TYPE}" 

BIN="${BUILD_DIR}/SecasyAvalanche"
if [[ ! -x "${BIN}" ]]; then
  warn "Executable not found at ${BIN}"; exit 2; fi

run_case() {
  local desc="$1"; shift
  info "Running: ${desc} -> $*"
  "${BIN}" "$@" || warn "Case failed: ${desc}" 
}

echo "===================================================="
echo " Secasy Avalanche Extended Test Batch (WSL)"
echo " Build Type: ${BUILD_TYPE}" 
echo " Timestamp : $(date -Iseconds)"
echo "===================================================="

# Short quick sanity run
run_case "Sanity 64B small" -X -m 3 -l 64 -B 32 -r 4000 -s 111

# All bits on medium input
run_case "All bits 128B" -X -m 2 -l 128 -B 0 -r 8000 -s 222

# Larger input with sampling
run_case "Larger input 512B sample" -X -m 2 -l 512 -B 128 -r 12000 -s 333

# Multi-bit stress (focus on multi-bit trials)
run_case "Multi-bit emphasis" -X -m 3 -l 96 -B 48 -r 10000 -s 444

# Hash buffer variation
run_case "Hash buf 256" -X -m 2 -l 64 -B 32 -r 8000 -n 256 -s 555
run_case "Hash buf 1024" -X -m 2 -l 64 -B 32 -r 8000 -n 1024 -s 556

# Prime index variation
run_case "Prime idx 1000" -X -m 2 -l 64 -B 32 -r 8000 -i 1000 -s 600
run_case "Prime idx 5000" -X -m 2 -l 64 -B 32 -r 8000 -i 5000 -s 601

# Extended all-bits big diffusion (longer) - can be commented out
run_case "All bits 256B heavier" -X -m 1 -l 256 -B 0 -r 15000 -s 777

info "Batch complete. Review above extended sections for bias, entropy, and multi-bit stability."
