#!/usr/bin/env python3
"""Search exact Phase-2 cycles for all-zero Secasy messages.

An all-zero byte expands to four UP directions. The cursor therefore remains
in column zero. For UP, every visit increments the local prime index and color
by one. One counter modulo lcm(NUMBER_OF_PRIMES, NUM_COLOR_OPERATIONS) thus
encodes the complete collision-relevant state of each of the 16 reachable
cells. Brent's algorithm detects a repeated byte-boundary state with constant
memory.

If states at byte counts a and b repeat, the non-empty messages 00^a and 00^b
receive the same final-cell update and therefore enter Phase 3 identically.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIELD_SIZE = 16
NUM_COLORS = 6


def load_production_primes() -> list[int]:
    source = (ROOT / "include" / "primes.h").read_text(encoding="ascii")
    match = re.search(
        r"storedPrimesArray\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\};",
        source,
        flags=re.DOTALL,
    )
    if match is None:
        raise RuntimeError("could not locate storedPrimesArray")
    primes = [int(token) for token in re.findall(r"\d+", match.group("body"))]
    if not primes or primes[0] != 2:
        raise RuntimeError("production prime table is malformed")
    return primes


@dataclass
class RotorState:
    cursor_y: int
    counters: list[int]

    def copy(self) -> "RotorState":
        return RotorState(self.cursor_y, self.counters.copy())


def same_state(left: RotorState, right: RotorState) -> bool:
    return left.cursor_y == right.cursor_y and left.counters == right.counters


def advance_zero_byte(state: RotorState, primes: list[int], counter_period: int) -> None:
    advance_uniform_byte(state, primes, counter_period, 0)


def advance_uniform_byte(
    state: RotorState, primes: list[int], counter_period: int, direction: int
) -> None:
    index_delta = 1 + direction
    positive = direction in (1, 3)
    for _ in range(4):
        cell = state.cursor_y
        counter = state.counters[cell]
        jump = primes[(counter * index_delta) % len(primes)]
        state.counters[cell] = (counter + 1) % counter_period
        if positive:
            state.cursor_y = (cell + jump + 1) & (FIELD_SIZE - 1)
        else:
            state.cursor_y = (cell - jump) & (FIELD_SIZE - 1)


def find_cycle(primes: list[int], max_bytes: int, progress_every: int) -> tuple[int, int, int]:
    counter_period = math.lcm(len(primes), NUM_COLORS)
    initial = RotorState(0, [0] * FIELD_SIZE)
    tortoise = initial.copy()
    hare = initial.copy()
    advance_zero_byte(hare, primes, counter_period)

    power = 1
    period = 1
    evaluated = 1
    started = time.perf_counter()
    while not same_state(tortoise, hare):
        if evaluated >= max_bytes:
            raise RuntimeError(f"no cycle detected within {max_bytes} byte transitions")
        if power == period:
            tortoise = hare.copy()
            power *= 2
            period = 0
        advance_zero_byte(hare, primes, counter_period)
        period += 1
        evaluated += 1
        if progress_every and evaluated % progress_every == 0:
            elapsed = time.perf_counter() - started
            print(
                f"searched {evaluated:,} byte states in {elapsed:.1f}s "
                f"({evaluated / max(elapsed, 1e-9):,.0f} states/s)",
                flush=True,
            )

    tortoise = initial.copy()
    hare = initial.copy()
    for _ in range(period):
        advance_zero_byte(hare, primes, counter_period)

    preperiod = 0
    while not same_state(tortoise, hare):
        if preperiod >= max_bytes:
            raise RuntimeError("cycle entry exceeded search limit")
        advance_zero_byte(tortoise, primes, counter_period)
        advance_zero_byte(hare, primes, counter_period)
        preperiod += 1

    return preperiod, period, evaluated


def state_after(byte_count: int, primes: list[int]) -> RotorState:
    return uniform_state_after(byte_count, primes, 0)


def uniform_state_after(byte_count: int, primes: list[int], direction: int) -> RotorState:
    counter_period = math.lcm(len(primes), NUM_COLORS)
    state = RotorState(0, [0] * FIELD_SIZE)
    for _ in range(byte_count):
        advance_uniform_byte(state, primes, counter_period, direction)
    return state


def finalized_state(state: RotorState, counter_period: int) -> RotorState:
    result = state.copy()
    cell = result.cursor_y
    result.counters[cell] = (result.counters[cell] + 1) % counter_period
    return result


def write_repeated_prefix_file(
    path: Path, value: int, repeat_count: int, suffix: bytes = b""
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    chunk = bytes([value]) * (1024 * 1024)
    remaining = repeat_count
    with path.open("wb") as output:
        while remaining:
            amount = min(remaining, len(chunk))
            output.write(chunk[:amount])
            remaining -= amount
        output.write(suffix)


def hash_file(executable: Path, path: Path) -> str:
    completed = subprocess.run(
        [str(executable), "-f", str(path), "-n", "512", "-r", "10"],
        check=True,
        capture_output=True,
    )
    pieces = re.findall(
        rb"(?<![0-9a-f])[0-9a-f]{12,}(?![0-9a-f])", completed.stdout
    )
    digest = b"".join(pieces)
    if len(digest) != 128:
        raise RuntimeError(f"could not parse a 512-bit digest from {executable}")
    return digest.decode("ascii")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-bytes", type=int, default=20_000_000)
    parser.add_argument("--progress-every", type=int, default=1_000_000)
    parser.add_argument(
        "--suffix-hex",
        help="build S and 00^period || S instead of two all-zero cycle multiples",
    )
    parser.add_argument(
        "--same-length-byte-hex",
        help="build 00^period and XX^period; XX must encode one repeated direction",
    )
    parser.add_argument("--write-dir", type=Path)
    parser.add_argument("--oracle", type=Path)
    parser.add_argument("--result", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    primes = load_production_primes()
    started = time.perf_counter()
    preperiod, period, evaluated = find_cycle(primes, args.max_bytes, args.progress_every)
    initial = RotorState(0, [0] * FIELD_SIZE)
    cycle_returns_to_initial = same_state(state_after(period, primes), initial)
    if preperiod == 0 and not cycle_returns_to_initial:
        raise RuntimeError("reported zero-preperiod cycle does not return to the initial state")

    suffix: bytes | None = None
    if args.suffix_hex is not None:
        try:
            suffix = bytes.fromhex(args.suffix_hex)
        except ValueError as exc:
            raise SystemExit("--suffix-hex must contain complete hexadecimal bytes") from exc
        if not suffix:
            raise SystemExit("--suffix-hex must be non-empty")

    second_pattern: int | None = None
    second_direction: int | None = None
    if args.same_length_byte_hex is not None:
        if suffix is not None:
            raise SystemExit("--suffix-hex and --same-length-byte-hex are exclusive")
        try:
            encoded_pattern = bytes.fromhex(args.same_length_byte_hex)
        except ValueError as exc:
            raise SystemExit("--same-length-byte-hex must be one hexadecimal byte") from exc
        if len(encoded_pattern) != 1:
            raise SystemExit("--same-length-byte-hex must be one hexadecimal byte")
        second_pattern = encoded_pattern[0]
        directions = {(second_pattern >> shift) & 3 for shift in (0, 2, 4, 6)}
        if len(directions) != 1:
            raise SystemExit("the selected byte does not encode one repeated direction")
        second_direction = directions.pop()
        second_state = uniform_state_after(period, primes, second_direction)
        if not same_state(second_state, initial):
            raise RuntimeError("the selected repeated-direction byte is not neutral at this period")

    counter_period = math.lcm(len(primes), NUM_COLORS)
    if second_pattern is not None:
        message_a_bytes = period
        message_b_bytes = period
        prefix_a = 0
        prefix_b = second_pattern
        prefix_count_a = period
        prefix_count_b = period
        phase2_equal = True
    elif suffix is not None:
        if preperiod != 0 or not cycle_returns_to_initial:
            raise RuntimeError("a neutral-prefix collision requires a cycle from the initial state")
        prefix_a = 0
        prefix_b = 0
        prefix_count_a = 0
        prefix_count_b = period
        message_a_bytes = len(suffix)
        message_b_bytes = period + len(suffix)
        phase2_equal = True
    else:
        # Both messages must be non-empty because processBuffer skips
        # finalization for the empty message. Move one cycle forward if needed.
        message_a_bytes = preperiod if preperiod > 0 else period
        message_b_bytes = message_a_bytes + period
        prefix_a = 0
        prefix_b = 0
        prefix_count_a = message_a_bytes
        prefix_count_b = message_b_bytes
        state_a = state_after(message_a_bytes, primes)
        state_b = state_after(message_b_bytes, primes)
        phase2_equal = same_state(
            finalized_state(state_a, counter_period),
            finalized_state(state_b, counter_period),
        )
    if not phase2_equal:
        raise RuntimeError("cycle candidate failed exact finalized-state replay")

    result: dict[str, object] = {
        "command": "up-rotor-cycle-search",
        "counter_period": counter_period,
        "elapsed_seconds": time.perf_counter() - started,
        "evaluated_byte_states": evaluated,
        "message_a_bytes": message_a_bytes,
        "message_a_pattern_hex": f"{prefix_a:02x}",
        "message_a_pattern_repetitions": prefix_count_a,
        "message_b_bytes": message_b_bytes,
        "message_b_pattern_hex": f"{prefix_b:02x}",
        "message_b_pattern_repetitions": prefix_count_b,
        "neutral_prefix": cycle_returns_to_initial and preperiod == 0,
        "phase2_equal": True,
        "preperiod_bytes": preperiod,
        "prime_count": len(primes),
        "period_bytes": period,
    }
    if suffix is not None:
        result["suffix_hex"] = suffix.hex()

    if args.write_dir is not None:
        message_a = args.write_dir / "secasy-collision-a.bin"
        message_b = args.write_dir / "secasy-collision-b.bin"
        write_repeated_prefix_file(message_a, prefix_a, prefix_count_a, suffix or b"")
        write_repeated_prefix_file(message_b, prefix_b, prefix_count_b, suffix or b"")
        result["message_a_path"] = str(message_a)
        result["message_b_path"] = str(message_b)

        if args.oracle is not None:
            digest_a = hash_file(args.oracle, message_a)
            digest_b = hash_file(args.oracle, message_b)
            result["digest_a"] = digest_a
            result["digest_b"] = digest_b
            result["oracle_verified"] = digest_a == digest_b
            if digest_a != digest_b:
                raise RuntimeError("production oracle rejected the collision candidate")

    encoded = json.dumps(result, indent=2, sort_keys=True)
    print(encoded)
    if args.result is not None:
        args.result.parent.mkdir(parents=True, exist_ok=True)
        args.result.write_text(encoded + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
