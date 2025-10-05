#!/bin/bash
# Run comprehensive security test battery
# Usage: ./run_security_tests.sh [quick|full]

MODE=${1:-quick}
BUILD_DIR="../build"
RESULTS_DIR="../results"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

mkdir -p "$RESULTS_DIR"

echo "========================================="
echo "Secasy Security Test Battery"
echo "Mode: $MODE"
echo "========================================="

# Check build
if [ ! -f "$BUILD_DIR/Secasy" ]; then
    echo -e "${RED}Error: Build not found. Building now...${NC}"
    cmake -S .. -B "$BUILD_DIR" && cmake --build "$BUILD_DIR" --config Release
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
fi

PASS_COUNT=0
FAIL_COUNT=0

run_test() {
    local name=$1
    local command=$2
    local validator=$3
    
    echo ""
    echo "-----------------------------------"
    echo "Running: $name"
    echo "-----------------------------------"
    
    eval "$command" > "$RESULTS_DIR/${name}.log" 2>&1
    
    if [ -n "$validator" ]; then
        eval "$validator"
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ PASS${NC}: $name"
            ((PASS_COUNT++))
        else
            echo -e "${RED}✗ FAIL${NC}: $name"
            ((FAIL_COUNT++))
        fi
    else
        echo "  (Manual validation required)"
    fi
}

# Test 1: Avalanche (Basic)
if [ "$MODE" == "quick" ]; then
    run_test "avalanche_basic" \
        "$BUILD_DIR/SecasyAvalanche -m 10 -l 64 -B 32 -r 50000 -s 42" \
        ""
else
    run_test "avalanche_extended" \
        "$BUILD_DIR/SecasyAvalanche -X -m 20 -l 64 -B 0 -r 100000 -s 42" \
        ""
fi

# Test 2: Collision (Truncated)
if [ "$MODE" == "quick" ]; then
    run_test "collision_24bit" \
        "$BUILD_DIR/SecasyCollision -m 50000 -l 32 -r 50000 -T 24 -s 123" \
        "python validate_collisions.py $RESULTS_DIR/collision_24bit.log"
else
    run_test "collision_sweep" \
        "$BUILD_DIR/SecasyCollision -m 100000 -l 64 -r 100000 -X 24,28,32,36 -s 456" \
        "python validate_collisions.py $RESULTS_DIR/collision_sweep.log"
fi

# Test 3: Distribution Quality
run_test "distribution_hex_freq" \
    "$BUILD_DIR/SecasyCollision -m 10000 -l 64 -r 50000 -F -s 789" \
    ""

run_test "distribution_byte_freq" \
    "$BUILD_DIR/SecasyCollision -m 10000 -l 64 -r 50000 -B 4 -s 321" \
    ""

# Test 4: Structured Inputs (if implemented)
# run_test "structured_low_hamming" \
#     "./test_structured_inputs.sh low_hamming" \
#     ""

# Summary
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo -e "Passed: ${GREEN}$PASS_COUNT${NC}"
echo -e "Failed: ${RED}$FAIL_COUNT${NC}"
echo ""
echo "Results saved to: $RESULTS_DIR/"
echo ""

if [ $FAIL_COUNT -gt 0 ]; then
    echo -e "${RED}Some tests failed. Review logs for details.${NC}"
    exit 1
else
    echo -e "${GREEN}All validated tests passed!${NC}"
    exit 0
fi
