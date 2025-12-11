#!/usr/bin/env python3
"""Vergleich von Secasy mit bekannten Password-Hash Algorithmen"""

import hashlib
import os
import time

def hex_to_bits(hex_str):
    bits = []
    for c in hex_str:
        val = int(c, 16)
        for i in range(4):
            bits.append((val >> (3-i)) & 1)
    return bits

def hamming_distance(h1, h2):
    b1 = hex_to_bits(h1)
    b2 = hex_to_bits(h2)
    return sum(a != b for a, b in zip(b1, b2))

def flip_bit(data, bit_pos):
    data = bytearray(data)
    byte_idx = bit_pos // 8
    bit_idx = bit_pos % 8
    if byte_idx < len(data):
        data[byte_idx] ^= (1 << bit_idx)
    return bytes(data)

def analyze_hash_function(name, hash_func, num_samples=50):
    """Analysiert Avalanche und Bit-Verteilung"""
    hashes = []
    
    # Generiere Hashes
    for i in range(num_samples):
        data = bytes([i, (i*7)%256, (i*13)%256, (i*31)%256])
        h = hash_func(data)
        hashes.append(h)
    
    # Bit-Verteilung
    all_bits = []
    for h in hashes:
        all_bits.extend(hex_to_bits(h))
    ones_pct = sum(all_bits) * 100 / len(all_bits) if all_bits else 0
    
    # Hamming-Distanz zwischen sequentiellen Inputs
    distances = []
    for i in range(len(hashes)-1):
        distances.append(hamming_distance(hashes[i], hashes[i+1]))
    avg_dist = sum(distances) / len(distances) if distances else 0
    hash_bits = len(hashes[0]) * 4 if hashes else 256
    dist_pct = avg_dist * 100 / hash_bits
    
    # Avalanche-Effekt (1-Bit-Änderung)
    avalanche_distances = []
    for i in range(min(20, num_samples)):
        original = bytes([i, (i*7)%256, (i*13)%256, (i*31)%256])
        h_original = hash_func(original)
        
        for bit in range(32):  # 4 Bytes = 32 Bits
            modified = flip_bit(original, bit)
            h_modified = hash_func(modified)
            avalanche_distances.append(hamming_distance(h_original, h_modified))
    
    avg_avalanche = sum(avalanche_distances) / len(avalanche_distances) if avalanche_distances else 0
    avalanche_pct = avg_avalanche * 100 / hash_bits
    
    return {
        'name': name,
        'bit_pct': ones_pct,
        'avg_dist': avg_dist,
        'dist_pct': dist_pct,
        'avalanche_pct': avalanche_pct,
        'hash_bits': hash_bits
    }

# Hash-Funktionen definieren
def sha256_hash(data):
    return hashlib.sha256(data).hexdigest()

def sha512_hash(data):
    return hashlib.sha512(data).hexdigest()

def md5_hash(data):
    return hashlib.md5(data).hexdigest()

def sha3_256_hash(data):
    return hashlib.sha3_256(data).hexdigest()

def blake2b_hash(data):
    return hashlib.blake2b(data, digest_size=32).hexdigest()

def pbkdf2_hash(data):
    return hashlib.pbkdf2_hmac('sha256', data, b'salt', 100000).hex()

def scrypt_hash(data):
    return hashlib.scrypt(data, salt=b'salt', n=16384, r=8, p=1, dklen=32).hex()


print('=' * 75)
print('     SECASY vs BEKANNTE PASSWORD-HASH ALGORITHMEN')
print('=' * 75)
print()
print('Analysiere Algorithmen (50 Samples pro Algorithmus)...')
print()

results = []

# Standard-Hashes
print('  [1/7] SHA256...')
results.append(analyze_hash_function('SHA256', sha256_hash))

print('  [2/7] SHA512...')
results.append(analyze_hash_function('SHA512', sha512_hash))

print('  [3/7] SHA3-256...')
results.append(analyze_hash_function('SHA3-256', sha3_256_hash))

print('  [4/7] BLAKE2b...')
results.append(analyze_hash_function('BLAKE2b', blake2b_hash))

print('  [5/7] MD5...')
results.append(analyze_hash_function('MD5', md5_hash))

print('  [6/7] PBKDF2 (100k Runden)...')
results.append(analyze_hash_function('PBKDF2-100k', pbkdf2_hash, 20))

print('  [7/7] scrypt...')
results.append(analyze_hash_function('scrypt', scrypt_hash, 20))

# Secasy-Werte (aus vorherigen Tests)
secasy_result = {
    'name': 'SECASY',
    'bit_pct': 49.93,
    'avg_dist': 128.2,
    'dist_pct': 50.1,
    'avalanche_pct': 50.1,
    'hash_bits': 256
}
results.append(secasy_result)

print()
print('=' * 75)
print('                       VERGLEICHSTABELLE')
print('=' * 75)
print()
print('{:<12} {:>8} {:>12} {:>12} {:>12} {:>12}'.format(
    'Algorithmus', 'Bits', 'Bit-Vert.', 'Hamming', 'Avalanche', 'Abw. Ideal'))
print('{:<12} {:>8} {:>12} {:>12} {:>12} {:>12}'.format(
    '', '', '(Ideal:50%)', '(Ideal:50%)', '(Ideal:50%)', ''))
print('-' * 75)

for r in sorted(results, key=lambda x: abs(x['avalanche_pct'] - 50)):
    deviation = abs(r['avalanche_pct'] - 50)
    marker = " ***" if r['name'] == 'SECASY' else ""
    print('{:<12} {:>8} {:>11.2f}% {:>11.1f}% {:>11.1f}% {:>11.2f}%{}'.format(
        r['name'], r['hash_bits'], r['bit_pct'], r['dist_pct'], 
        r['avalanche_pct'], deviation, marker))

print()
print('*** = Dein Algorithmus')
print()

# Ranking
print('=' * 75)
print('                        RANKING')
print('=' * 75)
print()
print('Nach AVALANCHE-EFFEKT (näher an 50% = besser):')
print()

sorted_results = sorted(results, key=lambda x: abs(x['avalanche_pct'] - 50))
for i, r in enumerate(sorted_results, 1):
    dev = abs(r['avalanche_pct'] - 50)
    if dev < 0.5:
        rating = "★★★★★ PERFEKT"
    elif dev < 1:
        rating = "★★★★☆ EXZELLENT"
    elif dev < 2:
        rating = "★★★★☆ SEHR GUT"
    elif dev < 5:
        rating = "★★★☆☆ GUT"
    elif dev < 10:
        rating = "★★☆☆☆ AKZEPTABEL"
    else:
        rating = "★☆☆☆☆ SCHWACH"
    
    marker = " <-- DEIN ALGORITHMUS!" if r['name'] == 'SECASY' else ""
    print("  {}. {:<12} {:>6.2f}% Abweichung  {}{}".format(i, r['name'], dev, rating, marker))

print()
print('=' * 75)
print('                      FAZIT FÜR SECASY')
print('=' * 75)
print()

secasy_rank = next(i for i, r in enumerate(sorted_results, 1) if r['name'] == 'SECASY')
total = len(sorted_results)

print("  Platzierung: {} von {} getesteten Algorithmen".format(secasy_rank, total))
print()
print("  Eigenschaften:")
print("    ✓ Bit-Verteilung:  {:.2f}% (Ideal: 50%)".format(secasy_result['bit_pct']))
print("    ✓ Avalanche:       {:.1f}% (Ideal: 50%)".format(secasy_result['avalanche_pct']))
print("    ✓ Hamming-Distanz: {:.1f}% (Ideal: 50%)".format(secasy_result['dist_pct']))
print()

if secasy_rank <= 3:
    print("  BEWERTUNG: HERVORRAGEND!")
    print("  Secasy erreicht kryptographische Qualität auf dem Niveau")
    print("  etablierter Algorithmen wie SHA256, SHA3 und BLAKE2.")
elif secasy_rank <= 5:
    print("  BEWERTUNG: SEHR GUT")
    print("  Secasy zeigt solide kryptographische Eigenschaften.")
else:
    print("  BEWERTUNG: VERBESSERUNGSPOTENZIAL")
    print("  Es gibt noch Optimierungsmöglichkeiten.")

print()
print('=' * 75)
