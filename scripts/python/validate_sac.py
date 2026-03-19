#!/usr/bin/env python3
"""
Validate SAC (Strict Avalanche Criterion) matrix results.
Reads CSV matrix and checks acceptance criteria.

Usage:
    python validate_sac.py <sac_matrix.csv> [--threshold 0.95]
"""

import sys
import csv
import argparse
from typing import List, Tuple

def load_sac_matrix(filepath: str) -> List[List[float]]:
    """Load SAC matrix from CSV file."""
    matrix = []
    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        # Skip header if present
        first_row = next(reader)
        try:
            matrix.append([float(x) for x in first_row])
        except ValueError:
            # Was header, skip
            pass
        
        for row in reader:
            matrix.append([float(x) for x in row])
    
    return matrix

def analyze_sac_matrix(matrix: List[List[float]], 
                       lower_bound: float = 0.48, 
                       upper_bound: float = 0.52) -> Tuple[float, int, int]:
    """
    Analyze SAC matrix for acceptance.
    
    Returns:
        (acceptance_rate, in_band_count, total_cells)
    """
    total_cells = 0
    in_band = 0
    
    for row in matrix:
        for value in row:
            total_cells += 1
            if lower_bound <= value <= upper_bound:
                in_band += 1
    
    acceptance_rate = in_band / total_cells if total_cells > 0 else 0.0
    return acceptance_rate, in_band, total_cells

def print_statistics(matrix: List[List[float]]):
    """Print detailed statistics."""
    all_values = [val for row in matrix for val in row]
    
    if not all_values:
        print("Empty matrix!")
        return
    
    mean = sum(all_values) / len(all_values)
    sorted_vals = sorted(all_values)
    median = sorted_vals[len(sorted_vals) // 2]
    min_val = min(all_values)
    max_val = max(all_values)
    
    # Standard deviation
    variance = sum((x - mean) ** 2 for x in all_values) / len(all_values)
    std_dev = variance ** 0.5
    
    print(f"\nMatrix Statistics:")
    print(f"  Dimensions: {len(matrix)} × {len(matrix[0]) if matrix else 0}")
    print(f"  Mean:       {mean:.6f}")
    print(f"  Median:     {median:.6f}")
    print(f"  Std Dev:    {std_dev:.6f}")
    print(f"  Min:        {min_val:.6f}")
    print(f"  Max:        {max_val:.6f}")

def main():
    parser = argparse.ArgumentParser(description='Validate SAC matrix')
    parser.add_argument('matrix_file', help='Path to SAC matrix CSV')
    parser.add_argument('--threshold', type=float, default=0.95,
                       help='Acceptance threshold (default: 0.95 = 95%%)')
    parser.add_argument('--lower', type=float, default=0.48,
                       help='Lower acceptance bound (default: 0.48)')
    parser.add_argument('--upper', type=float, default=0.52,
                       help='Upper acceptance bound (default: 0.52)')
    
    args = parser.parse_args()
    
    print(f"Loading SAC matrix from: {args.matrix_file}")
    matrix = load_sac_matrix(args.matrix_file)
    
    print_statistics(matrix)
    
    acceptance_rate, in_band, total = analyze_sac_matrix(
        matrix, args.lower, args.upper
    )
    
    print(f"\nSAC Acceptance Analysis:")
    print(f"  Acceptance band: [{args.lower}, {args.upper}]")
    print(f"  Cells in band:   {in_band} / {total}")
    print(f"  Acceptance rate: {acceptance_rate:.4f} ({acceptance_rate*100:.2f}%)")
    print(f"  Required:        {args.threshold:.4f} ({args.threshold*100:.2f}%)")
    
    if acceptance_rate >= args.threshold:
        print(f"\n✓ PASS: SAC acceptance criterion met!")
        return 0
    else:
        print(f"\n✗ FAIL: SAC acceptance below threshold!")
        print(f"  Deficit: {(args.threshold - acceptance_rate)*100:.2f}%")
        return 1

if __name__ == '__main__':
    sys.exit(main())
