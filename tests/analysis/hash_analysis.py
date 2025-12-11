#!/usr/bin/env python3
"""Umfassende Hash-Analyse für Secasy"""

import subprocess
import os
import random
from collections import Counter

def get_hash(data: bytes) -> str:
    """Berechnet Hash für gegebene Daten"""
    with open("_test.bin", "wb") as f:
        f.write(data)
    result = subprocess.run(["./secasy.exe", "-f", "_test.bin"], 
                          capture_output=True, text=True)
    for line in result.stdout.split("\n"):
        if "HASH VALUE:" in line:
            return line.split("HASH VALUE:")[1].strip()[:64]
    return None

def hex_to_bits(hex_str: str) -> list:
    """Konvertiert Hex-String zu Bit-Liste"""
    bits = []
    for c in hex_str:
        val = int(c, 16)
        for i in range(4):
            bits.append((val >> (3-i)) & 1)
    return bits

def hamming_distance(h1: str, h2: str) -> int:
    """Berechnet Hamming-Distanz zwischen zwei Hashes"""
    b1 = hex_to_bits(h1)
    b2 = hex_to_bits(h2)
    return sum(a != b for a, b in zip(b1, b2))

def flip_bit(data: bytes, bit_pos: int) -> bytes:
    """Flippt ein Bit in den Daten"""
    data = bytearray(data)
    byte_idx = bit_pos // 8
    bit_idx = bit_pos % 8
    data[byte_idx] ^= (1 << bit_idx)
    return bytes(data)

print("=" * 60)
print("        SECASY UMFASSENDE HASH-ANALYSE")
print("=" * 60)

# Generiere Test-Daten
print("\nGeneriere 50 Test-Hashes...")
test_data = []
hashes = []
for i in range(50):
    data = bytes([i, (i * 7) % 256, (i * 13) % 256, (i * 31) % 256])
    test_data.append(data)
    h = get_hash(data)
    if h:
        hashes.append(h)
        print(f"  [{i+1}/50] {data.hex()} -> {h[:16]}...")

print(f"\n{len(hashes)} Hashes generiert.")

# ============================================================
# 1. BIT-VERTEILUNG
# ============================================================
print("\n" + "=" * 60)
print("1. BIT-VERTEILUNG")
print("=" * 60)

all_bits = []
for h in hashes:
    all_bits.extend(hex_to_bits(h))

ones = sum(all_bits)
total = len(all_bits)
pct = ones * 100 / total

print(f"   Einsen:  {ones:,} / {total:,} ({pct:.2f}%)")
print(f"   Nullen:  {total - ones:,} / {total:,} ({100 - pct:.2f}%)")
print(f"   Ideal:   50.00%")
print(f"   Abweichung: {abs(pct - 50):.2f}%")

if abs(pct - 50) < 2:
    print("   BEWERTUNG: ✅ SEHR GUT")
elif abs(pct - 50) < 5:
    print("   BEWERTUNG: ⚠️  GUT")
else:
    print("   BEWERTUNG: ❌ SCHLECHT")

# ============================================================
# 2. BYTE-VERTEILUNG (Nibble-Verteilung)
# ============================================================
print("\n" + "=" * 60)
print("2. BYTE/NIBBLE-VERTEILUNG")
print("=" * 60)

all_nibbles = []
for h in hashes:
    for c in h:
        all_nibbles.append(int(c, 16))

nibble_counts = Counter(all_nibbles)
expected = len(all_nibbles) / 16

print(f"   Gesamt Nibbles: {len(all_nibbles)}")
print(f"   Erwartete Häufigkeit pro Wert (0-F): {expected:.1f}")
print()
print("   Wert  Anzahl  Abweichung")
print("   " + "-" * 30)

max_dev = 0
for i in range(16):
    count = nibble_counts.get(i, 0)
    dev = abs(count - expected) / expected * 100
    max_dev = max(max_dev, dev)
    bar = "█" * int(count / expected * 10)
    print(f"   {i:X}     {count:4d}    {dev:5.1f}%  {bar}")

print()
if max_dev < 20:
    print("   BEWERTUNG: ✅ SEHR GUT (max. Abweichung < 20%)")
elif max_dev < 40:
    print("   BEWERTUNG: ⚠️  GUT (max. Abweichung < 40%)")
else:
    print(f"   BEWERTUNG: ❌ SCHLECHT (max. Abweichung {max_dev:.1f}%)")

# ============================================================
# 3. AVALANCHE-EFFEKT
# ============================================================
print("\n" + "=" * 60)
print("3. AVALANCHE-EFFEKT (1-Bit-Änderung)")
print("=" * 60)

avalanche_distances = []
test_count = 20

for i in range(test_count):
    # Zufällige Testdaten (4 Bytes)
    original = bytes([random.randint(0, 255) for _ in range(4)])
    h_original = get_hash(original)
    
    if not h_original:
        continue
    
    # Flippe jedes Bit und messe Hamming-Distanz
    for bit in range(32):  # 4 Bytes = 32 Bits
        modified = flip_bit(original, bit)
        h_modified = get_hash(modified)
        
        if h_modified:
            dist = hamming_distance(h_original, h_modified)
            avalanche_distances.append(dist)

if avalanche_distances:
    avg_dist = sum(avalanche_distances) / len(avalanche_distances)
    min_dist = min(avalanche_distances)
    max_dist = max(avalanche_distances)
    
    # 256-bit Hash -> Ideal ist 128 Bits Änderung (50%)
    ideal = 128
    pct_of_ideal = avg_dist / ideal * 100
    
    print(f"   Tests durchgeführt: {len(avalanche_distances)}")
    print(f"   Durchschnittliche Bit-Änderungen: {avg_dist:.1f} / 256 ({avg_dist/256*100:.1f}%)")
    print(f"   Minimum: {min_dist} Bits")
    print(f"   Maximum: {max_dist} Bits")
    print(f"   Ideal: 128 Bits (50%)")
    print(f"   Erreichter Prozentsatz vom Ideal: {pct_of_ideal:.1f}%")
    
    if 40 <= avg_dist/256*100 <= 60:
        print("   BEWERTUNG: ✅ SEHR GUT (40-60% Änderung)")
    elif 30 <= avg_dist/256*100 <= 70:
        print("   BEWERTUNG: ⚠️  GUT (30-70% Änderung)")
    else:
        print("   BEWERTUNG: ❌ SCHLECHT")

# ============================================================
# 4. HAMMING-DISTANZ zwischen ähnlichen Inputs
# ============================================================
print("\n" + "=" * 60)
print("4. HAMMING-DISTANZ (ähnliche Inputs)")
print("=" * 60)

similar_distances = []

# Teste sequentielle Inputs
for i in range(49):
    if i < len(hashes) and i+1 < len(hashes):
        dist = hamming_distance(hashes[i], hashes[i+1])
        similar_distances.append(dist)

if similar_distances:
    avg_sim = sum(similar_distances) / len(similar_distances)
    min_sim = min(similar_distances)
    max_sim = max(similar_distances)
    
    print(f"   Verglichene Paare: {len(similar_distances)}")
    print(f"   Durchschnittliche Distanz: {avg_sim:.1f} Bits ({avg_sim/256*100:.1f}%)")
    print(f"   Minimum: {min_sim} Bits")
    print(f"   Maximum: {max_sim} Bits")
    
    if avg_sim/256*100 >= 40:
        print("   BEWERTUNG: ✅ SEHR GUT (≥40% Unterschied)")
    elif avg_sim/256*100 >= 30:
        print("   BEWERTUNG: ⚠️  GUT (≥30% Unterschied)")
    else:
        print("   BEWERTUNG: ❌ SCHLECHT (<30% Unterschied)")

# ============================================================
# ZUSAMMENFASSUNG
# ============================================================
print("\n" + "=" * 60)
print("                    ZUSAMMENFASSUNG")
print("=" * 60)
print("""
   Der Secasy Hash-Algorithmus wurde auf 4 Kriterien getestet:
   
   1. Bit-Verteilung    - Sind 0en und 1en gleichmäßig?
   2. Nibble-Verteilung - Sind Hex-Werte gleichmäßig?
   3. Avalanche-Effekt  - Ändert 1 Bit Input viele Output-Bits?
   4. Hamming-Distanz   - Sind ähnliche Inputs unterschiedlich?
""")

# Cleanup
if os.path.exists("_test.bin"):
    os.remove("_test.bin")

print("Analyse abgeschlossen!")
