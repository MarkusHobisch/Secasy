#!/usr/bin/env python3
"""
Validate collision test results against expected birthday bounds.

Usage:
    python validate_collisions.py <collision_log.txt>
"""

import sys
import re
import math
import argparse

def parse_collision_log(filepath: str):
    """Parse collision test log file."""
    results = []
    
    with open(filepath, 'r') as f:
        for line in f:
            # Look for pattern like: "Bits= 32  Collisions=116  ... Expected~116.4"
            match = re.search(
                r'Bits=\s*(\d+)\s+Collisions=(\d+)\s+.*Expected~([\d.]+)',
                line
            )
            if match:
                bits = int(match.group(1))
                observed = int(match.group(2))
                expected = float(match.group(3))
                results.append((bits, observed, expected))
    
    return results

def validate_collision_count(observed: int, expected: float, sigma_multiplier: float = 3.0) -> bool:
    """
    Validate collision count against birthday bound.
    
    Poisson approximation: variance ≈ expectation
    So σ ≈ sqrt(expected)
    Acceptance: observed within expected ± sigma_multiplier * sqrt(expected)
    """
    if expected < 1.0:
        # Too few expected collisions for meaningful statistical test
        # Just check we don't have way too many
        return observed < expected * 10
    
    sigma = math.sqrt(expected)
    lower_bound = expected - sigma_multiplier * sigma
    upper_bound = expected + sigma_multiplier * sigma
    
    return lower_bound <= observed <= upper_bound

def main():
    parser = argparse.ArgumentParser(description='Validate collision test results')
    parser.add_argument('log_file', help='Path to collision test log')
    parser.add_argument('--sigma', type=float, default=3.0,
                       help='Sigma multiplier for acceptance band (default: 3)')
    
    args = parser.parse_args()
    
    print(f"Parsing collision log: {args.log_file}")
    results = parse_collision_log(args.log_file)
    
    if not results:
        print("Error: No collision results found in log file!")
        print("Expected format: 'Bits= XX  Collisions=YY  ... Expected~ZZ'")
        return 1
    
    print(f"\nFound {len(results)} collision test results:\n")
    
    all_pass = True
    
    for bits, observed, expected in results:
        sigma = math.sqrt(expected) if expected >= 1.0 else 1.0
        lower = max(0, expected - args.sigma * sigma)
        upper = expected + args.sigma * sigma
        
        passed = validate_collision_count(observed, expected, args.sigma)
        status = "✓ PASS" if passed else "✗ FAIL"
        
        print(f"{status} | {bits:2d} bits: Observed={observed:6d}  "
              f"Expected={expected:8.2f}  "
              f"Bounds=[{lower:8.2f}, {upper:8.2f}]")
        
        if not passed:
            all_pass = False
            deviation = abs(observed - expected) / sigma if sigma > 0 else 0
            print(f"       ! Deviation: {deviation:.2f}σ")
    
    print("\n" + "="*70)
    
    if all_pass:
        print("✓ All collision tests PASSED (within ±{}σ)".format(args.sigma))
        return 0
    else:
        print("✗ Some collision tests FAILED")
        print("  → Possible causes: bias in output distribution, weak mixing")
        print("  → Recommended action: strengthen finalization or increase rounds")
        return 1

if __name__ == '__main__':
    sys.exit(main())
