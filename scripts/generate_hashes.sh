#!/bin/bash
# Generate random hashes for statistical testing
# Usage: ./generate_hashes.sh [count] [input_length] [rounds]

COUNT=${1:-1000000}
INPUT_LEN=${2:-64}
ROUNDS=${3:-100000}

# Ensure build exists
if [ ! -f "../build/Secasy" ]; then
    echo "Error: Secasy binary not found. Build first with:"
    echo "  cmake -S .. -B ../build && cmake --build ../build"
    exit 1
fi

# Generate random inputs and hash them
for i in $(seq 1 $COUNT); do
    # Generate random bytes
    head -c $INPUT_LEN /dev/urandom > /tmp/secasy_input_$$.bin
    
    # Hash and output (assuming binary output mode or pipe hex)
    ../build/Secasy -f /tmp/secasy_input_$$.bin -r $ROUNDS -n 256 2>/dev/null
    
    # Progress indicator every 10000 hashes
    if [ $((i % 10000)) -eq 0 ]; then
        echo "Generated $i / $COUNT hashes..." >&2
    fi
done

# Cleanup
rm -f /tmp/secasy_input_$$.bin

echo "Hash generation complete: $COUNT hashes" >&2
