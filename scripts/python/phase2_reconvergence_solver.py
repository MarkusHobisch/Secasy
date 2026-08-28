#!/usr/bin/env python3
"""Exact bounded Phase-2 reconvergence search for Secasy.

The model keeps the cursor, per-cell prime index, and per-cell color index.
Cell values need not be separate symbolic variables: throughout Phase 2 each
reachable value is exactly the prime selected by that cell's prime index.
"""

from __future__ import annotations

import argparse
import json
import random
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
LOCAL_Z3 = ROOT / "build" / "python-deps"

try:
    import z3  # type: ignore
    if not hasattr(z3, "Solver"):
        raise ImportError("the imported z3 namespace is not z3-solver")
except ImportError:
    sys.modules.pop("z3", None)
    if LOCAL_Z3.is_dir():
        sys.path.insert(0, str(LOCAL_Z3))
    try:
        import z3  # type: ignore
        if not hasattr(z3, "Solver"):
            raise ImportError("the imported z3 namespace is not z3-solver")
    except ImportError as exc:
        raise SystemExit(
            "z3-solver is required; install it with "
            "'python -m pip install z3-solver --target build/python-deps'"
        ) from exc


FIELD_SIZE = 16
NUM_CELLS = FIELD_SIZE * FIELD_SIZE
NUM_COLORS = 6
CURSOR_BITS = 4
DIRECTION_BITS = 2
INDEX_BITS = 17
VISIT_BITS = 16
WORD_MASK = (1 << 64) - 1
ROUND_CONSTANT = 0x9E3779B97F4A7C15
UP, RIGHT, LEFT, DOWN = range(4)


@dataclass(frozen=True)
class Phase2State:
    prime_indices: tuple[int, ...]
    colors: tuple[int, ...]
    cursor_x: int
    cursor_y: int


@dataclass(frozen=True)
class SymbolicState:
    event_cells: tuple[Any, ...]
    index_deltas: tuple[Any, ...]
    cursor_x: Any
    cursor_y: Any
    directions: tuple[Any, ...]


@dataclass(frozen=True)
class FinalState:
    values: tuple[int, ...]
    digest: str


def load_production_primes() -> list[int]:
    source = (ROOT / "include" / "primes.h").read_text(encoding="ascii")
    match = re.search(
        r"storedPrimesArray\s*\[\s*\]\s*=\s*\{(?P<body>.*?)\};",
        source,
        flags=re.DOTALL,
    )
    if match is None:
        raise RuntimeError("could not locate storedPrimesArray in include/primes.h")
    primes = [int(token) for token in re.findall(r"\d+", match.group("body"))]
    if not primes or primes[0] != 2:
        raise RuntimeError("production prime table is malformed")
    return primes


def directions_from_bytes(data: bytes) -> list[int]:
    return [(byte >> shift) & 3 for byte in data for shift in (0, 2, 4, 6)]


def bytes_from_directions(directions: list[int]) -> bytes:
    if len(directions) % 4 != 0:
        raise ValueError("direction count must be divisible by four")
    result = bytearray()
    for offset in range(0, len(directions), 4):
        byte = 0
        for slot, direction in enumerate(directions[offset : offset + 4]):
            byte |= direction << (2 * slot)
        result.append(byte)
    return bytes(result)


def simulate_phase2(data: bytes, primes: list[int]) -> Phase2State:
    prime_indices = [0] * NUM_CELLS
    colors = [0] * NUM_CELLS
    x = 0
    y = 0

    for direction in directions_from_bytes(data):
        cell = x * FIELD_SIZE + y
        old_prime = primes[prime_indices[cell]]
        prime_indices[cell] += 1 + direction
        colors[cell] = (colors[cell] + 1) % NUM_COLORS

        if direction == UP:
            y = (y - old_prime) & (FIELD_SIZE - 1)
        elif direction == RIGHT:
            x = (x + old_prime + 1) & (FIELD_SIZE - 1)
        elif direction == LEFT:
            x = (x - old_prime) & (FIELD_SIZE - 1)
        else:
            y = (y + old_prime + 1) & (FIELD_SIZE - 1)

    if data:
        cell = x * FIELD_SIZE + y
        prime_indices[cell] += 1
        colors[cell] = (colors[cell] + 1) % NUM_COLORS

    return Phase2State(tuple(prime_indices), tuple(colors), x, y)


def bitvector_sum(terms: list[Any], width: int) -> Any:
    result = z3.BitVecVal(0, width)
    for term in terms:
        result = result + term
    return result


def ripple_add_64(solver: Any, left: Any, right: Any, prefix: str) -> Any:
    """Encode one modular 64-bit addition with explicit carry variables."""
    carry: Any = z3.BoolVal(False)
    sum_bits: list[Any] = []
    for bit in range(64):
        left_bit = z3.Extract(bit, bit, left) == z3.BitVecVal(1, 1)
        right_bit = z3.Extract(bit, bit, right) == z3.BitVecVal(1, 1)
        sum_bit = z3.Bool(f"{prefix}_sum_{bit}")
        next_carry = z3.Bool(f"{prefix}_carry_{bit + 1}")
        solver.add(sum_bit == z3.Xor(z3.Xor(left_bit, right_bit), carry))
        solver.add(
            next_carry
            == z3.Or(
                z3.And(left_bit, right_bit),
                z3.And(left_bit, carry),
                z3.And(right_bit, carry),
            )
        )
        sum_bits.append(z3.If(sum_bit, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1)))
        carry = next_carry
    return z3.Concat(*reversed(sum_bits))


def prime_mod_lookup(index: Any, prime_mods: list[int]) -> Any:
    result = z3.BitVecVal(prime_mods[0], CURSOR_BITS)
    for candidate, value in enumerate(prime_mods[1:], start=1):
        result = z3.If(
            index == z3.BitVecVal(candidate, INDEX_BITS),
            z3.BitVecVal(value, CURSOR_BITS),
            result,
        )
    return result


def build_symbolic_message(
    solver: Any, prefix: str, byte_count: int, prime_mods: list[int]
) -> SymbolicState:
    directions = tuple(
        z3.BitVec(f"{prefix}_d_{step}", DIRECTION_BITS)
        for step in range(4 * byte_count)
    )
    x = z3.BitVecVal(0, CURSOR_BITS)
    y = z3.BitVecVal(0, CURSOR_BITS)
    visited_cells: list[Any] = []

    for step, direction in enumerate(directions):
        cell = z3.Concat(x, y)
        old_index = bitvector_sum(
            [
                z3.If(
                    visited_cells[prior] == cell,
                    z3.ZeroExt(INDEX_BITS - DIRECTION_BITS, directions[prior]) + 1,
                    z3.BitVecVal(0, INDEX_BITS),
                )
                for prior in range(step)
            ],
            INDEX_BITS,
        )
        jump = prime_mod_lookup(old_index, prime_mods)
        visited_cells.append(cell)

        x = z3.If(
            direction == RIGHT,
            x + jump + 1,
            z3.If(direction == LEFT, x - jump, x),
        )
        y = z3.If(
            direction == UP,
            y - jump,
            z3.If(direction == DOWN, y + jump + 1, y),
        )

    event_cells = list(visited_cells)
    index_deltas: list[Any] = [
        z3.ZeroExt(INDEX_BITS - DIRECTION_BITS, direction) + 1
        for direction in directions
    ]
    if directions:
        event_cells.append(z3.Concat(x, y))
        index_deltas.append(z3.BitVecVal(1, INDEX_BITS))

    return SymbolicState(tuple(event_cells), tuple(index_deltas), x, y, directions)


def constrain_reconvergence(
    solver: Any,
    left: SymbolicState,
    right: SymbolicState,
    mode: str,
    first_difference: int | None,
    difference_a: int | None,
    difference_b: int | None,
) -> None:
    solver.add(left.cursor_x == right.cursor_x, left.cursor_y == right.cursor_y)
    if mode == "reconvergence":
        solver.add(
            bitvector_sum(list(left.index_deltas), INDEX_BITS)
            == bitvector_sum(list(right.index_deltas), INDEX_BITS)
        )
    for candidate in left.event_cells + right.event_cells:
        left_index = bitvector_sum(
            [
                z3.If(cell == candidate, delta, z3.BitVecVal(0, INDEX_BITS))
                for cell, delta in zip(left.event_cells, left.index_deltas)
            ],
            INDEX_BITS,
        )
        right_index = bitvector_sum(
            [
                z3.If(cell == candidate, delta, z3.BitVecVal(0, INDEX_BITS))
                for cell, delta in zip(right.event_cells, right.index_deltas)
            ],
            INDEX_BITS,
        )
        left_visits = bitvector_sum(
            [
                z3.If(
                    cell == candidate,
                    z3.BitVecVal(1, VISIT_BITS),
                    z3.BitVecVal(0, VISIT_BITS),
                )
                for cell in left.event_cells
            ],
            VISIT_BITS,
        )
        right_visits = bitvector_sum(
            [
                z3.If(
                    cell == candidate,
                    z3.BitVecVal(1, VISIT_BITS),
                    z3.BitVecVal(0, VISIT_BITS),
                )
                for cell in right.event_cells
            ],
            VISIT_BITS,
        )
        if mode == "reconvergence":
            solver.add(left_index == right_index)
        solver.add(
            z3.URem(left_visits, z3.BitVecVal(NUM_COLORS, VISIT_BITS))
            == z3.URem(right_visits, z3.BitVecVal(NUM_COLORS, VISIT_BITS))
        )

    if first_difference is not None:
        if first_difference >= min(len(left.directions), len(right.directions)):
            raise ValueError("first difference lies outside the shorter message")
        for step in range(first_difference):
            solver.add(left.directions[step] == right.directions[step])
        if difference_a is not None and difference_b is not None:
            solver.add(
                left.directions[first_difference] == difference_a,
                right.directions[first_difference] == difference_b,
            )
        else:
            solver.add(
                z3.ULT(
                    left.directions[first_difference],
                    right.directions[first_difference],
                )
            )
    elif len(left.directions) == len(right.directions):
        if left.directions:
            solver.add(z3.ULT(z3.Concat(*left.directions), z3.Concat(*right.directions)))
        else:
            solver.add(z3.BoolVal(False))


def evaluate_directions(model: Any, directions: tuple[Any, ...]) -> list[int]:
    return [model.eval(direction, model_completion=True).as_long() for direction in directions]


def constrain_blocked_pairs(
    solver: Any,
    left: SymbolicState,
    right: SymbolicState,
    blocked_pairs: list[str],
    bytes_a: int,
    bytes_b: int,
) -> None:
    for specification in blocked_pairs:
        try:
            hex_a, hex_b = specification.split(":", maxsplit=1)
            data_a = bytes.fromhex(hex_a)
            data_b = bytes.fromhex(hex_b)
        except ValueError as exc:
            raise SystemExit(f"invalid blocked pair '{specification}'") from exc
        if len(data_a) != bytes_a or len(data_b) != bytes_b:
            raise SystemExit(f"blocked pair '{specification}' has the wrong message length")
        directions_a = directions_from_bytes(data_a)
        directions_b = directions_from_bytes(data_b)
        solver.add(
            z3.Or(
                *[
                    variable != value
                    for variable, value in zip(left.directions, directions_a)
                ],
                *[
                    variable != value
                    for variable, value in zip(right.directions, directions_b)
                ],
            )
        )


def constrain_message_prefix(
    solver: Any, state: SymbolicState, prefix_hex: str | None, total_bytes: int
) -> None:
    if prefix_hex is None:
        return
    try:
        prefix = bytes.fromhex(prefix_hex)
    except ValueError as exc:
        raise SystemExit(f"invalid message prefix '{prefix_hex}'") from exc
    if len(prefix) > total_bytes:
        raise SystemExit(f"prefix '{prefix_hex}' is longer than the symbolic message")
    for variable, value in zip(state.directions, directions_from_bytes(prefix)):
        solver.add(variable == value)


def evaluate_symbolic_state(model: Any, state: SymbolicState) -> Phase2State:
    indices = [0] * NUM_CELLS
    visits = [0] * NUM_CELLS
    for cell_expression, delta_expression in zip(state.event_cells, state.index_deltas):
        cell = model.eval(cell_expression, model_completion=True).as_long()
        delta = model.eval(delta_expression, model_completion=True).as_long()
        indices[cell] += delta
        visits[cell] += 1
    return Phase2State(
        tuple(indices),
        tuple(visit % NUM_COLORS for visit in visits),
        model.eval(state.cursor_x, model_completion=True).as_long(),
        model.eval(state.cursor_y, model_completion=True).as_long(),
    )


def parse_oracle_state(executable: Path, data: bytes, primes: list[int]) -> Phase2State:
    argument = data.hex() if data else "-"
    completed = subprocess.run(
        [str(executable), "--dump-hex", argument],
        check=True,
        capture_output=True,
        text=True,
    )
    indices = [0] * NUM_CELLS
    colors = [0] * NUM_CELLS
    cursor_x = cursor_y = -1
    seen = 0
    for line in completed.stdout.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "cursor" and len(fields) == 3:
            cursor_x, cursor_y = int(fields[1]), int(fields[2])
        elif fields[0] == "cell" and len(fields) == 6:
            x, y, value, index, color = map(int, fields[1:])
            cell = x * FIELD_SIZE + y
            if primes[index] != value:
                raise RuntimeError(
                    f"oracle invariant failed at ({x},{y}): value {value}, index {index}"
                )
            indices[cell] = index
            colors[cell] = color
            seen += 1
    if seen != NUM_CELLS or cursor_x < 0 or cursor_y < 0:
        raise RuntimeError("oracle returned an incomplete Phase-2 state")
    return Phase2State(tuple(indices), tuple(colors), cursor_x, cursor_y)


def parse_oracle_final(executable: Path, data: bytes) -> FinalState:
    argument = data.hex() if data else "-"
    completed = subprocess.run(
        [str(executable), "--dump-final-hex", argument],
        check=True,
        capture_output=True,
        text=True,
    )
    values = [0] * NUM_CELLS
    digest = ""
    seen = 0
    for line in completed.stdout.splitlines():
        fields = line.split()
        if not fields:
            continue
        if fields[0] == "hash" and len(fields) == 2:
            digest = fields[1]
        elif fields[0] == "cell" and len(fields) == 6:
            x, y, value = map(int, fields[1:4])
            values[x * FIELD_SIZE + y] = value
            seen += 1
    if seen != NUM_CELLS or len(digest) != 128:
        raise RuntimeError("oracle returned an incomplete final state")
    return FinalState(tuple(values), digest)


def parse_oracle_phase4(executable: Path, values: list[int]) -> str:
    encoded = "".join(f"{value & WORD_MASK:016x}" for value in values)
    completed = subprocess.run(
        [str(executable), "--hash-values-hex", encoded],
        check=True,
        capture_output=True,
        text=True,
    )
    for line in completed.stdout.splitlines():
        fields = line.split()
        if len(fields) == 2 and fields[0] == "hash" and len(fields[1]) == 128:
            return fields[1]
    raise RuntimeError("oracle returned an incomplete Phase-4 digest")


def rotate_left_64(value: int, amount: int) -> int:
    return ((value << amount) | (value >> (64 - amount))) & WORD_MASK


def rotate_right_64(value: int, amount: int) -> int:
    return ((value >> amount) | (value << (64 - amount))) & WORD_MASK


def phase3_round(
    values: list[int], colors: tuple[int, ...], cursor_x: int, cursor_y: int, round_index: int
) -> None:
    round_key = (ROUND_CONSTANT * (round_index + 1)) & WORD_MASK
    for x in range(FIELD_SIZE):
        for y in range(FIELD_SIZE):
            color_x = (cursor_x + x) & (FIELD_SIZE - 1)
            color_y = (cursor_y + y) & (FIELD_SIZE - 1)
            color = colors[color_x * FIELD_SIZE + color_y]
            cell = x * FIELD_SIZE + y
            value = values[cell]
            if color == 0:
                value += 1 if y == 0 else values[cell - 1]
            elif color == 1:
                value -= 1 if y == FIELD_SIZE - 1 else values[cell + 1]
            elif color == 2:
                value ^= 1 if x == 0 else values[cell - FIELD_SIZE]
            elif color == 3:
                neighbour = 1 if x == FIELD_SIZE - 1 else values[cell + FIELD_SIZE]
                value = rotate_left_64(value, 13) ^ neighbour
            elif color == 4:
                neighbour = 1 if x == 0 else values[cell - FIELD_SIZE]
                value = rotate_right_64(value, 7) + neighbour
            elif color == 5:
                value = ~value
            else:
                raise RuntimeError(f"invalid color index {color}")
            values[cell] = (value + round_key + cell) & WORD_MASK


def phase3_inverse_round(
    values: list[int], colors: tuple[int, ...], cursor_x: int, cursor_y: int, round_index: int
) -> None:
    round_key = (ROUND_CONSTANT * (round_index + 1)) & WORD_MASK
    for x in range(FIELD_SIZE - 1, -1, -1):
        for y in range(FIELD_SIZE - 1, -1, -1):
            color_x = (cursor_x + x) & (FIELD_SIZE - 1)
            color_y = (cursor_y + y) & (FIELD_SIZE - 1)
            color = colors[color_x * FIELD_SIZE + color_y]
            cell = x * FIELD_SIZE + y
            value = (values[cell] - round_key - cell) & WORD_MASK
            if color == 0:
                value -= 1 if y == 0 else values[cell - 1]
            elif color == 1:
                value += 1 if y == FIELD_SIZE - 1 else values[cell + 1]
            elif color == 2:
                value ^= 1 if x == 0 else values[cell - FIELD_SIZE]
            elif color == 3:
                neighbour = 1 if x == FIELD_SIZE - 1 else values[cell + FIELD_SIZE]
                value = rotate_right_64(value ^ neighbour, 13)
            elif color == 4:
                neighbour = 1 if x == 0 else values[cell - FIELD_SIZE]
                value = rotate_left_64((value - neighbour) & WORD_MASK, 7)
            elif color == 5:
                value = ~value
            else:
                raise RuntimeError(f"invalid color index {color}")
            values[cell] = value & WORD_MASK


def phase3_inverse_values(
    final_values: list[int],
    colors: tuple[int, ...],
    cursor_x: int,
    cursor_y: int,
    rounds: int = 10,
) -> list[int]:
    cursors: list[tuple[int, int]] = []
    current_x = cursor_x
    current_y = cursor_y
    for _ in range(rounds):
        cursors.append((current_x, current_y))
        current_x += 1
        if current_x == FIELD_SIZE:
            current_x = 0
            current_y = (current_y + 1) & (FIELD_SIZE - 1)

    values = list(final_values)
    for round_index in range(rounds - 1, -1, -1):
        round_x, round_y = cursors[round_index]
        phase3_inverse_round(values, colors, round_x, round_y, round_index)
    return values


def phase4_digest(values: list[int]) -> str:
    blocks: list[str] = []
    for block in range(8):
        accumulation = 0
        for cell, value in enumerate(values):
            position = cell + 1 + block * NUM_CELLS
            weight = 2 * position + 1
            accumulation = (accumulation + value * weight) & WORD_MASK
            accumulation = rotate_left_64(accumulation, 7)
        blocks.append(f"{accumulation:016x}")
    return "".join(blocks)


def difference_metrics(left: list[int], right: list[int]) -> dict[str, Any]:
    differences = [a ^ b for a, b in zip(left, right)]
    active_cells = []
    for cell, (value_a, value_b, xor_difference) in enumerate(
        zip(left, right, differences)
    ):
        if xor_difference != 0:
            active_cells.append(
                {
                    "additive_difference": f"{(value_b - value_a) & WORD_MASK:016x}",
                    "x": cell // FIELD_SIZE,
                    "xor_difference": f"{xor_difference:016x}",
                    "y": cell % FIELD_SIZE,
                }
            )
    return {
        "active_cells": active_cells,
        "different_cells": sum(value != 0 for value in differences),
        "hamming_bits": sum(value.bit_count() for value in differences),
    }


def phase3_forward_values(
    initial_values: list[int],
    colors: tuple[int, ...],
    cursor_x: int,
    cursor_y: int,
    rounds: int = 10,
) -> list[int]:
    values = list(initial_values)
    for round_index in range(rounds):
        phase3_round(values, colors, cursor_x, cursor_y, round_index)
        cursor_x += 1
        if cursor_x == FIELD_SIZE:
            cursor_x = 0
            cursor_y = (cursor_y + 1) & (FIELD_SIZE - 1)
    return values


def phase3_values(state: Phase2State, primes: list[int], rounds: int = 10) -> list[int]:
    return phase3_forward_values(
        [primes[index] for index in state.prime_indices],
        state.colors,
        state.cursor_x,
        state.cursor_y,
        rounds,
    )


def phase3_affine_tail(
    state: Phase2State, primes: list[int], tail_cells: int
) -> tuple[list[int], list[list[int]]]:
    first_tail = NUM_CELLS - tail_cells
    source_values = [primes[index] for index in state.prime_indices]
    zero_tail = source_values[:first_tail] + [0] * tail_cells
    constant = phase3_forward_values(
        zero_tail, state.colors, state.cursor_x, state.cursor_y
    )
    coefficients = [[0] * tail_cells for _ in range(tail_cells)]

    for column in range(tail_cells):
        basis = list(zero_tail)
        basis[first_tail + column] = 1
        output = phase3_forward_values(
            basis, state.colors, state.cursor_x, state.cursor_y
        )
        for cell in range(first_tail):
            if output[cell] != constant[cell]:
                raise RuntimeError("selected Phase-3 tail is not a closed subspace")
        for row in range(tail_cells):
            cell = first_tail + row
            coefficients[row][column] = (output[cell] - constant[cell]) & WORD_MASK

    reconstructed = list(constant)
    for row in range(tail_cells):
        cell = first_tail + row
        reconstructed[cell] = (
            constant[cell]
            + sum(
                coefficients[row][column] * source_values[first_tail + column]
                for column in range(tail_cells)
            )
        ) & WORD_MASK
    expected = phase3_forward_values(
        source_values, state.colors, state.cursor_x, state.cursor_y
    )
    if reconstructed != expected:
        raise RuntimeError("selected Phase-3 tail is not affine")
    return constant, coefficients


def balanced_bv_lookup(index: Any, values: list[int]) -> Any:
    if not values or len(values) & (len(values) - 1):
        raise ValueError("balanced lookup requires a power-of-two value count")
    level: list[Any] = [z3.BitVecVal(value, 64) for value in values]
    bit = 0
    while len(level) > 1:
        level = [
            z3.If(z3.Extract(bit, bit, index) == 0, level[offset], level[offset + 1])
            for offset in range(0, len(level), 2)
        ]
        bit += 1
    return level[0]


def write_result(result: dict[str, Any], path: Path | None) -> None:
    rendered = json.dumps(result, indent=2, sort_keys=True)
    print(rendered)
    if path is not None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(rendered + "\n", encoding="ascii")


def run_validation(args: argparse.Namespace, primes: list[int]) -> int:
    rng = random.Random(args.seed)
    started = time.perf_counter()
    for trial in range(args.trials):
        length = rng.randint(0, args.max_bytes)
        data = bytes(rng.randrange(256) for _ in range(length))
        expected = simulate_phase2(data, primes)
        actual = parse_oracle_state(args.oracle, data, primes)
        if expected != actual:
            print(f"validation mismatch at trial {trial}, message={data.hex()}", file=sys.stderr)
            return 1

    symbolic_rng = random.Random(args.seed ^ 0x5A17B01C)
    for trial in range(args.symbolic_trials):
        length = symbolic_rng.randint(0, args.symbolic_max_bytes)
        data = bytes(symbolic_rng.randrange(256) for _ in range(length))
        max_index = 16 * length + 1
        prime_mods = [prime & (FIELD_SIZE - 1) for prime in primes[: max_index + 1]]
        solver = z3.SolverFor("QF_BV")
        symbolic = build_symbolic_message(solver, f"validation_{trial}", length, prime_mods)
        concrete_directions = directions_from_bytes(data)
        for variable, value in zip(symbolic.directions, concrete_directions):
            solver.add(variable == value)
        if solver.check() != z3.sat:
            print(f"symbolic validation was not SAT at trial {trial}", file=sys.stderr)
            return 1
        symbolic_state = evaluate_symbolic_state(solver.model(), symbolic)
        expected = simulate_phase2(data, primes)
        actual = parse_oracle_state(args.oracle, data, primes)
        if symbolic_state != expected or symbolic_state != actual:
            print(
                f"symbolic validation mismatch at trial {trial}, message={data.hex()}",
                file=sys.stderr,
            )
            return 1
    result = {
        "command": "validate",
        "elapsed_seconds": time.perf_counter() - started,
        "max_bytes": args.max_bytes,
        "oracle": str(args.oracle),
        "seed": args.seed,
        "status": "pass",
        "symbolic_max_bytes": args.symbolic_max_bytes,
        "symbolic_trials": args.symbolic_trials,
        "trials": args.trials,
        "z3_version": z3.get_version_string(),
    }
    write_result(result, args.result)
    return 0


def run_search(args: argparse.Namespace, primes: list[int]) -> int:
    max_steps = 4 * max(args.bytes_a, args.bytes_b)
    max_index = 4 * max_steps + 1
    if max_index >= len(primes):
        raise SystemExit("bounded model would reach prime-table wraparound")

    event_count_a = 0 if args.bytes_a == 0 else 4 * args.bytes_a + 1
    event_count_b = 0 if args.bytes_b == 0 else 4 * args.bytes_b + 1
    if (event_count_a - event_count_b) % NUM_COLORS != 0:
        result = {
            "bytes_a": args.bytes_a,
            "bytes_b": args.bytes_b,
            "command": "search",
            "difference_a": args.difference_a,
            "difference_b": args.difference_b,
            "elapsed_seconds": 0.0,
            "first_difference": args.first_difference,
            "mode": args.mode,
            "precheck": "color-event-sum-mod-6",
            "reason_unknown": None,
            "status": "unsat",
            "timeout_ms": args.timeout_ms,
            "z3_version": z3.get_version_string(),
        }
        write_result(result, args.result)
        return 0

    solver = z3.SolverFor("QF_BV")
    solver.set(timeout=args.timeout_ms)
    prime_mods = [prime & (FIELD_SIZE - 1) for prime in primes[: max_index + 1]]
    left = build_symbolic_message(solver, "a", args.bytes_a, prime_mods)
    right = build_symbolic_message(solver, "b", args.bytes_b, prime_mods)
    constrain_message_prefix(solver, left, args.prefix_a, args.bytes_a)
    constrain_message_prefix(solver, right, args.prefix_b, args.bytes_b)
    constrain_blocked_pairs(
        solver,
        left,
        right,
        args.block_pair,
        args.bytes_a,
        args.bytes_b,
    )
    constrain_reconvergence(
        solver,
        left,
        right,
        args.mode,
        args.first_difference,
        args.difference_a,
        args.difference_b,
    )

    started = time.perf_counter()
    status = solver.check()
    elapsed = time.perf_counter() - started
    result: dict[str, Any] = {
        "bytes_a": args.bytes_a,
        "bytes_b": args.bytes_b,
        "blocked_pairs": len(args.block_pair),
        "command": "search",
        "difference_a": args.difference_a,
        "difference_b": args.difference_b,
        "elapsed_seconds": elapsed,
        "first_difference": args.first_difference,
        "mode": args.mode,
        "prefix_a": args.prefix_a,
        "prefix_b": args.prefix_b,
        "reason_unknown": solver.reason_unknown() if status == z3.unknown else None,
        "status": str(status),
        "timeout_ms": args.timeout_ms,
        "z3_version": z3.get_version_string(),
    }

    if status == z3.sat:
        model = solver.model()
        data_a = bytes_from_directions(evaluate_directions(model, left.directions))
        data_b = bytes_from_directions(evaluate_directions(model, right.directions))
        concrete_a = simulate_phase2(data_a, primes)
        concrete_b = simulate_phase2(data_b, primes)
        same_schedule = (
            concrete_a.colors == concrete_b.colors
            and concrete_a.cursor_x == concrete_b.cursor_x
            and concrete_a.cursor_y == concrete_b.cursor_y
        )
        if args.mode == "reconvergence" and concrete_a != concrete_b:
            raise RuntimeError("SAT model did not reproduce a concrete reconvergence")
        if args.mode == "equal-schedule" and not same_schedule:
            raise RuntimeError("SAT model did not reproduce a concrete equal schedule")
        oracle_verified = None
        if args.oracle is not None:
            oracle_a = parse_oracle_state(args.oracle, data_a, primes)
            oracle_b = parse_oracle_state(args.oracle, data_b, primes)
            oracle_pair_matches = (
                oracle_a == oracle_b
                if args.mode == "reconvergence"
                else (
                    oracle_a.colors == oracle_b.colors
                    and oracle_a.cursor_x == oracle_b.cursor_x
                    and oracle_a.cursor_y == oracle_b.cursor_y
                )
            )
            oracle_verified = (
                oracle_a == concrete_a and oracle_b == concrete_b and oracle_pair_matches
            )
            if not oracle_verified:
                raise RuntimeError("SAT model did not reproduce in the production C oracle")
        different_value_cells = sum(
            left_index != right_index
            for left_index, right_index in zip(
                concrete_a.prime_indices, concrete_b.prime_indices
            )
        )
        result.update(
            {
                "different_value_cells": different_value_cells,
                "exact_reconvergence": concrete_a == concrete_b,
                "message_a_hex": data_a.hex(),
                "message_b_hex": data_b.hex(),
                "oracle_verified": oracle_verified,
            }
        )

    write_result(result, args.result)
    return 0 if status in (z3.sat, z3.unsat) else 3


def run_pair_analysis(args: argparse.Namespace, primes: list[int]) -> int:
    data_a = bytes.fromhex(args.message_a)
    data_b = bytes.fromhex(args.message_b)
    state_a = simulate_phase2(data_a, primes)
    state_b = simulate_phase2(data_b, primes)
    same_schedule = (
        state_a.colors == state_b.colors
        and state_a.cursor_x == state_b.cursor_x
        and state_a.cursor_y == state_b.cursor_y
    )
    if not same_schedule:
        raise SystemExit("messages do not produce the same color map and cursor")

    oracle_phase2_a = parse_oracle_state(args.oracle, data_a, primes)
    oracle_phase2_b = parse_oracle_state(args.oracle, data_b, primes)
    if oracle_phase2_a != state_a or oracle_phase2_b != state_b:
        raise RuntimeError("Phase-2 pair did not reproduce in the C oracle")

    values_a = [primes[index] for index in state_a.prime_indices]
    values_b = [primes[index] for index in state_b.prime_indices]
    phase2_differences = []
    for cell, (index_a, index_b) in enumerate(
        zip(state_a.prime_indices, state_b.prime_indices)
    ):
        if index_a != index_b:
            phase2_differences.append(
                {
                    "index_a": index_a,
                    "index_b": index_b,
                    "value_a": primes[index_a],
                    "value_b": primes[index_b],
                    "x": cell // FIELD_SIZE,
                    "y": cell % FIELD_SIZE,
                }
            )

    rounds = [{"round": 0, **difference_metrics(values_a, values_b)}]
    cursor_x = state_a.cursor_x
    cursor_y = state_a.cursor_y
    for round_index in range(10):
        phase3_round(values_a, state_a.colors, cursor_x, cursor_y, round_index)
        phase3_round(values_b, state_b.colors, cursor_x, cursor_y, round_index)
        rounds.append(
            {"round": round_index + 1, **difference_metrics(values_a, values_b)}
        )
        cursor_x += 1
        if cursor_x == FIELD_SIZE:
            cursor_x = 0
            cursor_y = (cursor_y + 1) & (FIELD_SIZE - 1)

    digest_a = phase4_digest(values_a)
    digest_b = phase4_digest(values_b)
    oracle_final_a = parse_oracle_final(args.oracle, data_a)
    oracle_final_b = parse_oracle_final(args.oracle, data_b)
    oracle_verified = (
        oracle_final_a.values == tuple(values_a)
        and oracle_final_b.values == tuple(values_b)
        and oracle_final_a.digest == digest_a
        and oracle_final_b.digest == digest_b
    )
    if not oracle_verified:
        raise RuntimeError("Phase-3/4 analysis did not reproduce in the C oracle")

    result = {
        "command": "analyze-pair",
        "color_histogram": {
            name: state_a.colors.count(index)
            for index, name in enumerate(("ADD", "SUB", "XOR", "RLX", "RRA", "INVERT"))
        },
        "digest_a": digest_a,
        "digest_b": digest_b,
        "digest_hamming_bits": (int(digest_a, 16) ^ int(digest_b, 16)).bit_count(),
        "message_a_hex": data_a.hex(),
        "message_b_hex": data_b.hex(),
        "oracle_verified": True,
        "phase2_differences": phase2_differences,
        "rounds": rounds,
        "same_schedule": True,
    }
    write_result(result, args.result)
    return 0


def run_family_scan(args: argparse.Namespace, primes: list[int]) -> int:
    difference = bytes.fromhex(args.difference)
    if len(difference) != 3:
        raise SystemExit("--difference must contain exactly three bytes")
    if args.fixed_last.lower() == "all":
        last_values = range(256)
        fixed_last_label = "all"
    else:
        fixed_last = int(args.fixed_last, 16)
        if not 0 <= fixed_last <= 0xFF:
            raise SystemExit("--fixed-last must be one byte or 'all'")
        last_values = (fixed_last,)
        fixed_last_label = f"{fixed_last:02x}"

    started = time.perf_counter()
    candidates: list[dict[str, Any]] = []
    schedule_matches = 0
    for fixed_last in last_values:
        for first in range(128):
            for second in range(256):
                data_a = bytes((first, second, fixed_last))
                data_b = bytes(value ^ delta for value, delta in zip(data_a, difference))
                state_a = simulate_phase2(data_a, primes)
                state_b = simulate_phase2(data_b, primes)
                if not (
                    state_a.colors == state_b.colors
                    and state_a.cursor_x == state_b.cursor_x
                    and state_a.cursor_y == state_b.cursor_y
                ):
                    continue
                schedule_matches += 1
                phase2_cells = sum(
                    left != right
                    for left, right in zip(state_a.prime_indices, state_b.prime_indices)
                )
                final_a = phase3_values(state_a, primes)
                final_b = phase3_values(state_b, primes)
                phase3_metrics = difference_metrics(final_a, final_b)
                digest_a = phase4_digest(final_a)
                digest_b = phase4_digest(final_b)
                candidates.append(
                    {
                        "digest_hamming_bits": (
                            int(digest_a, 16) ^ int(digest_b, 16)
                        ).bit_count(),
                        "message_a_hex": data_a.hex(),
                        "message_b_hex": data_b.hex(),
                        "phase2_different_cells": phase2_cells,
                        "phase3_different_cells": phase3_metrics["different_cells"],
                        "phase3_hamming_bits": phase3_metrics["hamming_bits"],
                    }
                )

    candidates.sort(
        key=lambda candidate: (
            candidate["phase3_different_cells"],
            candidate["phase3_hamming_bits"],
            candidate["digest_hamming_bits"],
        )
    )
    histogram: dict[str, int] = {}
    for candidate in candidates:
        key = str(candidate["phase3_different_cells"])
        histogram[key] = histogram.get(key, 0) + 1

    oracle_verified = False
    if candidates:
        best = candidates[0]
        data_a = bytes.fromhex(best["message_a_hex"])
        data_b = bytes.fromhex(best["message_b_hex"])
        state_a = simulate_phase2(data_a, primes)
        state_b = simulate_phase2(data_b, primes)
        final_a = phase3_values(state_a, primes)
        final_b = phase3_values(state_b, primes)
        oracle_a = parse_oracle_final(args.oracle, data_a)
        oracle_b = parse_oracle_final(args.oracle, data_b)
        oracle_verified = (
            oracle_a.values == tuple(final_a)
            and oracle_b.values == tuple(final_b)
            and oracle_a.digest == phase4_digest(final_a)
            and oracle_b.digest == phase4_digest(final_b)
        )
        if not oracle_verified:
            raise RuntimeError("best family candidate did not reproduce in the C oracle")

    result = {
        "best_digest_candidate": (
            min(candidates, key=lambda candidate: candidate["digest_hamming_bits"])
            if candidates
            else None
        ),
        "best_internal_candidate": candidates[0] if candidates else None,
        "candidate_count": len(candidates),
        "command": "scan-family",
        "difference_hex": difference.hex(),
        "elapsed_seconds": time.perf_counter() - started,
        "fixed_last_hex": fixed_last_label,
        "oracle_verified": oracle_verified,
        "phase3_active_cell_histogram": histogram,
        "schedule_matches": schedule_matches,
        "tested_pairs": 128 * 256 * len(last_values),
        "top_candidates": candidates[: args.top],
    }
    write_result(result, args.result)
    return 0


def run_suffix_scan(args: argparse.Namespace, primes: list[int]) -> int:
    base_a = bytes.fromhex(args.message_a)
    base_b = bytes.fromhex(args.message_b)
    if not 1 <= args.suffix_bytes <= 2:
        raise SystemExit("--suffix-bytes must be 1 or 2")
    if args.independent and args.suffix_bytes != 1:
        raise SystemExit("--independent currently supports exactly one suffix byte")

    started = time.perf_counter()
    candidates: list[dict[str, Any]] = []
    if args.independent:
        suffix_pairs = (
            (bytes((suffix_a,)), bytes((suffix_b,)))
            for suffix_a in range(256)
            for suffix_b in range(256)
        )
        total = 256 * 256
    else:
        suffix_pairs = (
            (suffix, suffix)
            for suffix in (
                number.to_bytes(args.suffix_bytes, byteorder="big")
                for number in range(256**args.suffix_bytes)
            )
        )
        total = 256**args.suffix_bytes
    for suffix_a, suffix_b in suffix_pairs:
        data_a = base_a + suffix_a
        data_b = base_b + suffix_b
        state_a = simulate_phase2(data_a, primes)
        state_b = simulate_phase2(data_b, primes)
        if not (
            state_a.colors == state_b.colors
            and state_a.cursor_x == state_b.cursor_x
            and state_a.cursor_y == state_b.cursor_y
        ):
            continue
        phase2_cells = sum(
            left != right
            for left, right in zip(state_a.prime_indices, state_b.prime_indices)
        )
        final_a = phase3_values(state_a, primes)
        final_b = phase3_values(state_b, primes)
        phase3_metrics = difference_metrics(final_a, final_b)
        digest_a = phase4_digest(final_a)
        digest_b = phase4_digest(final_b)
        candidates.append(
            {
                "digest_hamming_bits": (
                    int(digest_a, 16) ^ int(digest_b, 16)
                ).bit_count(),
                "exact_reconvergence": state_a == state_b,
                "message_a_hex": data_a.hex(),
                "message_b_hex": data_b.hex(),
                "phase2_different_cells": phase2_cells,
                "phase3_different_cells": phase3_metrics["different_cells"],
                "phase3_hamming_bits": phase3_metrics["hamming_bits"],
                "suffix_a_hex": suffix_a.hex(),
                "suffix_b_hex": suffix_b.hex(),
            }
        )

    candidates.sort(
        key=lambda candidate: (
            candidate["phase3_different_cells"],
            candidate["phase3_hamming_bits"],
            candidate["digest_hamming_bits"],
        )
    )
    histogram: dict[str, int] = {}
    for candidate in candidates:
        key = str(candidate["phase3_different_cells"])
        histogram[key] = histogram.get(key, 0) + 1

    oracle_verified = False
    if candidates:
        best = candidates[0]
        data_a = bytes.fromhex(best["message_a_hex"])
        data_b = bytes.fromhex(best["message_b_hex"])
        state_a = simulate_phase2(data_a, primes)
        state_b = simulate_phase2(data_b, primes)
        final_a = phase3_values(state_a, primes)
        final_b = phase3_values(state_b, primes)
        oracle_a = parse_oracle_final(args.oracle, data_a)
        oracle_b = parse_oracle_final(args.oracle, data_b)
        oracle_verified = (
            oracle_a.values == tuple(final_a)
            and oracle_b.values == tuple(final_b)
            and oracle_a.digest == phase4_digest(final_a)
            and oracle_b.digest == phase4_digest(final_b)
        )
        if not oracle_verified:
            raise RuntimeError("best suffix candidate did not reproduce in the C oracle")

    result = {
        "base_message_a_hex": base_a.hex(),
        "base_message_b_hex": base_b.hex(),
        "best_digest_candidate": (
            min(candidates, key=lambda candidate: candidate["digest_hamming_bits"])
            if candidates
            else None
        ),
        "best_internal_candidate": candidates[0] if candidates else None,
        "candidate_count": len(candidates),
        "command": "scan-suffix",
        "elapsed_seconds": time.perf_counter() - started,
        "independent_suffixes": args.independent,
        "oracle_verified": oracle_verified,
        "phase3_active_cell_histogram": histogram,
        "suffix_bytes": args.suffix_bytes,
        "tested_suffixes": total,
        "top_candidates": candidates[: args.top],
    }
    write_result(result, args.result)
    return 0


def run_inverse_validation(args: argparse.Namespace, primes: list[int]) -> int:
    rng = random.Random(args.seed)
    started = time.perf_counter()

    for trial in range(args.raw_trials):
        original = [rng.getrandbits(64) for _ in range(NUM_CELLS)]
        colors = tuple(rng.randrange(NUM_COLORS) for _ in range(NUM_CELLS))
        cursor_x = rng.randrange(FIELD_SIZE)
        cursor_y = rng.randrange(FIELD_SIZE)
        rounds = rng.randint(1, 10)
        forward = list(original)
        current_x = cursor_x
        current_y = cursor_y
        for round_index in range(rounds):
            phase3_round(forward, colors, current_x, current_y, round_index)
            current_x += 1
            if current_x == FIELD_SIZE:
                current_x = 0
                current_y = (current_y + 1) & (FIELD_SIZE - 1)
        recovered = phase3_inverse_values(
            forward, colors, cursor_x, cursor_y, rounds
        )
        if recovered != original:
            raise RuntimeError(f"raw Phase-3 inverse validation failed at trial {trial}")

    for trial in range(args.reachable_trials):
        length = rng.randint(1, args.max_message_bytes)
        data = bytes(rng.randrange(256) for _ in range(length))
        phase2 = simulate_phase2(data, primes)
        expected = [primes[index] for index in phase2.prime_indices]
        final = parse_oracle_final(args.oracle, data)
        recovered = phase3_inverse_values(
            list(final.values),
            phase2.colors,
            phase2.cursor_x,
            phase2.cursor_y,
            10,
        )
        if recovered != expected:
            raise RuntimeError(
                f"reachable Phase-3 inverse validation failed at trial {trial}"
            )

    result = {
        "command": "validate-inverse",
        "elapsed_seconds": time.perf_counter() - started,
        "max_message_bytes": args.max_message_bytes,
        "oracle": str(args.oracle),
        "raw_trials": args.raw_trials,
        "reachable_trials": args.reachable_trials,
        "seed": args.seed,
        "status": "pass",
    }
    write_result(result, args.result)
    return 0


def run_phase4_collision(args: argparse.Namespace, primes: list[int]) -> int:
    if not 2 <= args.tail_cells <= NUM_CELLS:
        raise SystemExit("--tail-cells must be between 2 and 256")

    data = bytes.fromhex(args.message)
    phase2 = simulate_phase2(data, primes)
    source = parse_oracle_final(args.oracle, data)
    source_values = list(source.values)
    if source.digest != phase4_digest(source_values):
        raise RuntimeError("source message does not reproduce in the Python Phase-4 model")

    first_tail = NUM_CELLS - args.tail_cells
    candidate_prefix = [
        z3.BitVec(f"phase4_tail_{cell}", 64)
        for cell in range(first_tail, NUM_CELLS - 1)
    ]
    solver = z3.SolverFor("QF_BV")
    solver.set(random_seed=args.seed)

    target_blocks = [
        int(source.digest[offset : offset + 16], 16)
        for offset in range(0, 128, 16)
    ]
    required_last_values: list[Any] = []
    for block, target in enumerate(target_blocks):
        accumulation = 0
        for cell in range(first_tail):
            position = cell + 1 + block * NUM_CELLS
            weight = 2 * position + 1
            accumulation = rotate_left_64(
                (accumulation + source_values[cell] * weight) & WORD_MASK, 7
            )

        symbolic_accumulation: Any = z3.BitVecVal(accumulation, 64)
        for offset, value in enumerate(candidate_prefix):
            cell = first_tail + offset
            position = cell + 1 + block * NUM_CELLS
            weight = z3.BitVecVal(2 * position + 1, 64)
            symbolic_accumulation = z3.RotateLeft(
                symbolic_accumulation + value * weight, 7
            )

        last_position = NUM_CELLS + block * NUM_CELLS
        last_weight = 2 * last_position + 1
        inverse_weight = pow(last_weight, -1, 1 << 64)
        target_before_rotation = rotate_right_64(target, 7)
        required_last_values.append(
            (z3.BitVecVal(target_before_rotation, 64) - symbolic_accumulation)
            * z3.BitVecVal(inverse_weight, 64)
        )

    for required in required_last_values[1:]:
        solver.add(required == required_last_values[0])
    solver.add(
        z3.Or(
            *(
                value != z3.BitVecVal(source_values[cell], 64)
                for cell, value in zip(
                    range(first_tail, NUM_CELLS - 1), candidate_prefix
                )
            ),
            required_last_values[0]
            != z3.BitVecVal(source_values[NUM_CELLS - 1], 64),
        )
    )

    started = time.perf_counter()
    status = solver.check()
    elapsed = time.perf_counter() - started
    result: dict[str, Any] = {
        "command": "phase4-collision",
        "elapsed_seconds": elapsed,
        "message_hex": data.hex(),
        "solver_status": str(status),
        "tail_cells": args.tail_cells,
        "timeout_ms": args.timeout_ms,
    }

    if status == z3.sat:
        model = solver.model()
        candidate_values = source_values[:first_tail] + [
            model.eval(value, model_completion=True).as_long()
            for value in candidate_prefix
        ]
        candidate_values.append(
            model.eval(required_last_values[0], model_completion=True).as_long()
        )
        candidate_digest = phase4_digest(candidate_values)
        oracle_source = parse_oracle_phase4(args.oracle, source_values)
        oracle_candidate = parse_oracle_phase4(args.oracle, candidate_values)
        if not (
            candidate_values != source_values
            and candidate_digest == source.digest
            and oracle_source == source.digest
            and oracle_candidate == source.digest
        ):
            raise RuntimeError("Phase-4 collision did not reproduce in the production oracle")

        recovered = phase3_inverse_values(
            candidate_values,
            phase2.colors,
            phase2.cursor_x,
            phase2.cursor_y,
            10,
        )
        expected_phase2 = [primes[index] for index in phase2.prime_indices]
        prime_values = set(primes)
        result.update(
            {
                "candidate_digest": candidate_digest,
                "candidate_values_hex": "".join(
                    f"{value:016x}" for value in candidate_values
                ),
                "final_difference": difference_metrics(
                    source_values, candidate_values
                ),
                "inverse_phase3_difference": difference_metrics(
                    expected_phase2, recovered
                ),
                "inverse_phase3_prime_cells": sum(
                    value in prime_values for value in recovered
                ),
                "inverse_phase3_values_hex": "".join(
                    f"{value:016x}" for value in recovered
                ),
                "oracle_verified": True,
            }
        )

    write_result(result, args.result)
    return 0 if status == z3.sat else 1


def run_phase4_tail_collision(args: argparse.Namespace, primes: list[int]) -> int:
    if not 2 <= args.tail_cells <= NUM_CELLS:
        raise SystemExit("--tail-cells must be between 2 and 256")

    data = bytes.fromhex(args.message)
    phase2 = simulate_phase2(data, primes)
    source = parse_oracle_final(args.oracle, data)
    source_values = list(source.values)
    first_tail = NUM_CELLS - args.tail_cells
    left_tail = [
        z3.BitVec(f"phase4_left_{cell}", 64)
        for cell in range(first_tail, NUM_CELLS)
    ]
    right_prefix = [
        z3.BitVec(f"phase4_right_{cell}", 64)
        for cell in range(first_tail, NUM_CELLS - 1)
    ]

    solver = z3.Then(
        "simplify", "propagate-values", "solve-eqs", "bit-blast", "sat"
    ).solver()
    solver.set(random_seed=args.seed)
    required_right_last: list[Any] = []
    for block in range(8):
        concrete_accumulation = 0
        for cell in range(first_tail):
            position = cell + 1 + block * NUM_CELLS
            weight = 2 * position + 1
            concrete_accumulation = rotate_left_64(
                (concrete_accumulation + source_values[cell] * weight) & WORD_MASK,
                7,
            )

        left_accumulation: Any = z3.BitVecVal(concrete_accumulation, 64)
        for offset, value in enumerate(left_tail[:-1]):
            cell = first_tail + offset
            position = cell + 1 + block * NUM_CELLS
            weight = z3.BitVecVal(2 * position + 1, 64)
            left_accumulation = z3.RotateLeft(
                left_accumulation + value * weight, 7
            )

        right_accumulation: Any = z3.BitVecVal(concrete_accumulation, 64)
        for offset, value in enumerate(right_prefix):
            cell = first_tail + offset
            position = cell + 1 + block * NUM_CELLS
            weight = z3.BitVecVal(2 * position + 1, 64)
            right_accumulation = z3.RotateLeft(
                right_accumulation + value * weight, 7
            )

        last_position = NUM_CELLS + block * NUM_CELLS
        last_weight = 2 * last_position + 1
        inverse_weight = pow(last_weight, -1, 1 << 64)
        left_before_final_rotation = (
            left_accumulation
            + left_tail[-1] * z3.BitVecVal(last_weight, 64)
        )
        required_right_last.append(
            (left_before_final_rotation - right_accumulation)
            * z3.BitVecVal(inverse_weight, 64)
        )

    solver.add(left_tail[0] != right_prefix[0])

    started = time.perf_counter()
    stage_results: list[dict[str, Any]] = []
    best_model: Any | None = None
    matched_blocks = 0
    constrained_bits = 0
    status: Any = z3.unknown
    solver.set(timeout=min(5_000, args.timeout_ms))
    stage_started = time.perf_counter()
    status = solver.check()
    stage_results.append(
        {
            "block": 0,
            "required_bits": 64,
            "elapsed_seconds": time.perf_counter() - stage_started,
            "status": str(status),
        }
    )
    if status == z3.sat:
        best_model = solver.model()
        matched_blocks = 1
        constrained_bits = 64

    stop = status != z3.sat
    for block in range(1, 8):
        if stop:
            break
        for low_bit in range(0, 64, args.chunk_bits):
            high_bit = min(64, low_bit + args.chunk_bits) - 1
            solver.add(
                z3.Extract(high_bit, low_bit, required_right_last[block])
                == z3.Extract(high_bit, low_bit, required_right_last[0])
            )
            remaining_ms = args.timeout_ms - int(
                (time.perf_counter() - started) * 1000
            )
            if remaining_ms <= 0:
                status = z3.unknown
                stop = True
                break
            stage_timeout = min(60_000, remaining_ms)
            solver.set(timeout=stage_timeout)
            stage_started = time.perf_counter()
            status = solver.check()
            stage_results.append(
                {
                    "block": block,
                    "required_bits": high_bit + 1,
                    "elapsed_seconds": time.perf_counter() - stage_started,
                    "status": str(status),
                    "timeout_ms": stage_timeout,
                }
            )
            if status != z3.sat:
                stop = True
                break
            best_model = solver.model()
            constrained_bits = block * 64 + high_bit + 1
            if high_bit == 63:
                matched_blocks = block + 1

    elapsed = time.perf_counter() - started
    result: dict[str, Any] = {
        "command": "phase4-tail-collision",
        "constrained_bits": constrained_bits,
        "elapsed_seconds": elapsed,
        "matched_blocks": matched_blocks,
        "message_prefix_source_hex": data.hex(),
        "solver_status": str(status),
        "stages": stage_results,
        "tail_cells": args.tail_cells,
        "timeout_ms": args.timeout_ms,
    }

    full_collision = False
    if best_model is not None:
        model = best_model
        left_values = source_values[:first_tail] + [
            model.eval(value, model_completion=True).as_long()
            for value in left_tail
        ]
        right_values = source_values[:first_tail] + [
            model.eval(value, model_completion=True).as_long()
            for value in right_prefix
        ]
        right_values.append(
            model.eval(required_right_last[0], model_completion=True).as_long()
        )
        left_digest = phase4_digest(left_values)
        right_digest = phase4_digest(right_values)
        oracle_left = parse_oracle_phase4(args.oracle, left_values)
        oracle_right = parse_oracle_phase4(args.oracle, right_values)
        prefix_length = matched_blocks * 16
        partial_block_bits = constrained_bits - matched_blocks * 64
        partial_block_verified = True
        if partial_block_bits > 0:
            left_block = int(
                oracle_left[prefix_length : prefix_length + 16], 16
            )
            right_block = int(
                oracle_right[prefix_length : prefix_length + 16], 16
            )
            low_mask = (1 << partial_block_bits) - 1
            partial_block_verified = (
                (rotate_right_64(left_block, 7) ^ rotate_right_64(right_block, 7))
                & low_mask
            ) == 0
        full_collision = matched_blocks == 8 and left_digest == right_digest
        if not (
            left_values != right_values
            and oracle_left == left_digest
            and oracle_right == right_digest
            and left_digest[:prefix_length] == right_digest[:prefix_length]
            and partial_block_verified
        ):
            raise RuntimeError("tail collision stage did not reproduce in production C")

        recovered_left = phase3_inverse_values(
            left_values,
            phase2.colors,
            phase2.cursor_x,
            phase2.cursor_y,
            10,
        )
        recovered_right = phase3_inverse_values(
            right_values,
            phase2.colors,
            phase2.cursor_x,
            phase2.cursor_y,
            10,
        )
        prime_values = set(primes)
        result.update(
            {
                "full_collision": full_collision,
                "left_digest": left_digest,
                "partial_block_low_bits": partial_block_bits,
                "partial_block_verified": partial_block_verified,
                "right_digest": right_digest,
                "final_difference": difference_metrics(left_values, right_values),
                "inverse_phase3_difference": difference_metrics(
                    recovered_left, recovered_right
                ),
                "inverse_phase3_left_prime_cells": sum(
                    value in prime_values for value in recovered_left
                ),
                "inverse_phase3_right_prime_cells": sum(
                    value in prime_values for value in recovered_right
                ),
                "left_values_hex": "".join(
                    f"{value:016x}" for value in left_values
                ),
                "right_values_hex": "".join(
                    f"{value:016x}" for value in right_values
                ),
                "oracle_verified": True,
            }
        )

    write_result(result, args.result)
    return 0 if full_collision else 1


def run_phase4_prime_tail_collision(
    args: argparse.Namespace, primes: list[int]
) -> int:
    if not 2 <= args.tail_cells <= NUM_CELLS:
        raise SystemExit("--tail-cells must be between 2 and 256")
    if (
        args.prime_limit < 2
        or args.prime_limit > len(primes)
        or args.prime_limit & (args.prime_limit - 1)
    ):
        raise SystemExit("--prime-limit must be a power of two within the prime table")
    if args.output_bits < 64 or args.output_bits > 512 or args.output_bits % 64:
        raise SystemExit("--output-bits must be a multiple of 64 between 64 and 512")

    data = bytes.fromhex(args.message)
    phase2 = simulate_phase2(data, primes)
    source_phase2 = [primes[index] for index in phase2.prime_indices]
    first_tail = NUM_CELLS - args.tail_cells
    constant, coefficients = phase3_affine_tail(
        phase2, primes, args.tail_cells
    )
    candidates = primes[: args.prime_limit]
    index_bits = (args.prime_limit - 1).bit_length()
    left_indices = [
        z3.BitVec(f"prime_left_{cell}", index_bits)
        for cell in range(first_tail, NUM_CELLS)
    ]
    right_indices = [
        z3.BitVec(f"prime_right_{cell}", index_bits)
        for cell in range(first_tail, NUM_CELLS)
    ]
    left_phase2 = [balanced_bv_lookup(index, candidates) for index in left_indices]
    right_phase2 = [balanced_bv_lookup(index, candidates) for index in right_indices]

    def symbolic_final_tail(symbolic_phase2: list[Any]) -> list[Any]:
        output: list[Any] = []
        for row in range(args.tail_cells):
            cell = first_tail + row
            terms = [z3.BitVecVal(constant[cell], 64)]
            terms.extend(
                z3.BitVecVal(coefficients[row][column], 64)
                * symbolic_phase2[column]
                for column in range(args.tail_cells)
                if coefficients[row][column] != 0
            )
            output.append(bitvector_sum(terms, 64))
        return output

    left_final_tail = symbolic_final_tail(left_phase2)
    right_final_tail = symbolic_final_tail(right_phase2)
    solver = z3.Then(
        "simplify", "propagate-values", "solve-eqs", "bit-blast", "sat"
    ).solver()
    solver.set(random_seed=args.seed)
    solver.add(z3.ULT(left_indices[0], right_indices[0]))
    allowed_indices_by_cell: list[list[int]] = []
    for offset in range(args.tail_cells):
        color = phase2.colors[first_tail + offset]
        allowed = [
            index
            for index in range(args.prime_limit)
            if any(
                visits % NUM_COLORS == color
                and visits <= index <= 4 * visits
                for visits in range(index + 1)
            )
        ]
        if not allowed:
            raise RuntimeError("prime prefix contains no locally reachable index")
        allowed_indices_by_cell.append(allowed)
        for variable in (left_indices[offset], right_indices[offset]):
            solver.add(
                z3.Or(
                    *(variable == z3.BitVecVal(index, index_bits) for index in allowed)
                )
            )

    resume_bits = 0
    best_left_indices: list[int] | None = None
    best_right_indices: list[int] | None = None
    if args.resume_result is not None:
        resume = json.loads(args.resume_result.read_text(encoding="utf-8"))
        if not (
            resume.get("command") == "phase4-prime-tail-collision"
            and resume.get("tail_cells") == args.tail_cells
            and resume.get("prime_limit") == args.prime_limit
            and resume.get("output_bits", 512) == args.output_bits
            and resume.get("explicit_carries", False) == args.explicit_carries
            and resume.get("message_schedule_source_hex") == data.hex()
            and resume.get("phase2_local_constraints") is True
        ):
            raise SystemExit("resume result does not match this prime-tail search")
        best_left_indices = [int(value) for value in resume["left_prime_indices"]]
        best_right_indices = [int(value) for value in resume["right_prime_indices"]]
        resume_bits = int(resume["constrained_bits"])

    left_pre_rotations: list[Any] = []
    right_pre_rotations: list[Any] = []
    requested_blocks = args.output_bits // 64
    for block in range(requested_blocks):
        concrete_accumulation = 0
        for cell in range(first_tail):
            position = cell + 1 + block * NUM_CELLS
            weight = 2 * position + 1
            concrete_accumulation = rotate_left_64(
                (concrete_accumulation + constant[cell] * weight) & WORD_MASK,
                7,
            )

        left_accumulation: Any = z3.BitVecVal(concrete_accumulation, 64)
        right_accumulation: Any = z3.BitVecVal(concrete_accumulation, 64)
        for offset in range(args.tail_cells - 1):
            cell = first_tail + offset
            position = cell + 1 + block * NUM_CELLS
            weight = z3.BitVecVal(2 * position + 1, 64)
            left_product = left_final_tail[offset] * weight
            right_product = right_final_tail[offset] * weight
            if args.explicit_carries:
                left_pre_rotation = ripple_add_64(
                    solver, left_accumulation, left_product,
                    f"mar_left_b{block}_c{cell}",
                )
                right_pre_rotation = ripple_add_64(
                    solver, right_accumulation, right_product,
                    f"mar_right_b{block}_c{cell}",
                )
            else:
                left_pre_rotation = left_accumulation + left_product
                right_pre_rotation = right_accumulation + right_product
            left_accumulation = z3.RotateLeft(left_pre_rotation, 7)
            right_accumulation = z3.RotateLeft(right_pre_rotation, 7)

        last_position = NUM_CELLS + block * NUM_CELLS
        last_weight = z3.BitVecVal(2 * last_position + 1, 64)
        left_last_product = left_final_tail[-1] * last_weight
        right_last_product = right_final_tail[-1] * last_weight
        if args.explicit_carries:
            left_pre_rotations.append(
                ripple_add_64(
                    solver, left_accumulation, left_last_product,
                    f"mar_left_b{block}_c{NUM_CELLS - 1}",
                )
            )
            right_pre_rotations.append(
                ripple_add_64(
                    solver, right_accumulation, right_last_product,
                    f"mar_right_b{block}_c{NUM_CELLS - 1}",
                )
            )
        else:
            left_pre_rotations.append(left_accumulation + left_last_product)
            right_pre_rotations.append(right_accumulation + right_last_product)

    started = time.perf_counter()
    stages: list[dict[str, Any]] = []
    constrained_bits = resume_bits
    matched_blocks = resume_bits // 64
    status: Any = z3.unknown
    stop = False
    for block in range(8):
        if stop:
            break
        for low_bit in range(0, 64, args.chunk_bits):
            high_bit = min(64, low_bit + args.chunk_bits) - 1
            solver.add(
                z3.Extract(high_bit, low_bit, left_pre_rotations[block])
                == z3.Extract(high_bit, low_bit, right_pre_rotations[block])
            )
            target_bits = block * 64 + high_bit + 1
            if target_bits <= resume_bits:
                continue
            remaining_ms = args.timeout_ms - int(
                (time.perf_counter() - started) * 1000
            )
            if remaining_ms <= 0:
                stop = True
                break
            stage_timeout = min(60_000, remaining_ms)
            solver.set(timeout=stage_timeout)
            stage_started = time.perf_counter()
            status = solver.check()
            stages.append(
                {
                    "block": block,
                    "pre_rotation_bits": high_bit + 1,
                    "elapsed_seconds": time.perf_counter() - stage_started,
                    "status": str(status),
                    "timeout_ms": stage_timeout,
                }
            )
            if status != z3.sat:
                stop = True
                break
            model = solver.model()
            best_left_indices = [
                model.eval(index, model_completion=True).as_long()
                for index in left_indices
            ]
            best_right_indices = [
                model.eval(index, model_completion=True).as_long()
                for index in right_indices
            ]
            constrained_bits = target_bits
            if high_bit == 63:
                matched_blocks = block + 1

    result: dict[str, Any] = {
        "command": "phase4-prime-tail-collision",
        "constrained_bits": constrained_bits,
        "elapsed_seconds": time.perf_counter() - started,
        "matched_blocks": matched_blocks,
        "output_bits": args.output_bits,
        "explicit_carries": args.explicit_carries,
        "message_schedule_source_hex": data.hex(),
        "phase2_local_allowed_indices": allowed_indices_by_cell,
        "phase2_local_constraints": True,
        "prime_limit": args.prime_limit,
        "resumed_bits": resume_bits,
        "solver_status": str(status),
        "stages": stages,
        "tail_cells": args.tail_cells,
        "timeout_ms": args.timeout_ms,
    }

    full_collision = False
    if best_left_indices is not None and best_right_indices is not None:
        left_prime_indices = best_left_indices
        right_prime_indices = best_right_indices
        left_initial = source_phase2[:first_tail] + [
            candidates[index] for index in left_prime_indices
        ]
        right_initial = source_phase2[:first_tail] + [
            candidates[index] for index in right_prime_indices
        ]
        left_final = phase3_forward_values(
            left_initial, phase2.colors, phase2.cursor_x, phase2.cursor_y
        )
        right_final = phase3_forward_values(
            right_initial, phase2.colors, phase2.cursor_x, phase2.cursor_y
        )
        left_digest = parse_oracle_phase4(args.oracle, left_final)
        right_digest = parse_oracle_phase4(args.oracle, right_final)
        if left_digest != phase4_digest(left_final) or right_digest != phase4_digest(right_final):
            raise RuntimeError("prime-tail candidate did not reproduce in production C")

        prefix_length = matched_blocks * 16
        partial_bits = constrained_bits - matched_blocks * 64
        partial_verified = True
        if partial_bits > 0:
            left_block = int(left_digest[prefix_length : prefix_length + 16], 16)
            right_block = int(right_digest[prefix_length : prefix_length + 16], 16)
            partial_verified = (
                (rotate_right_64(left_block, 7) ^ rotate_right_64(right_block, 7))
                & ((1 << partial_bits) - 1)
            ) == 0
        digest_hex_chars = args.output_bits // 4
        full_collision = (
            matched_blocks == requested_blocks
            and left_digest[:digest_hex_chars] == right_digest[:digest_hex_chars]
        )
        if not (
            left_initial != right_initial
            and left_digest[:prefix_length] == right_digest[:prefix_length]
            and partial_verified
        ):
            raise RuntimeError("prime-tail constraints failed concrete replay")

        result.update(
            {
                "full_collision": full_collision,
                "left_digest": left_digest,
                "left_prime_indices": left_prime_indices,
                "left_prime_values": left_initial[first_tail:],
                "oracle_verified": True,
                "partial_block_low_bits": partial_bits,
                "partial_block_verified": partial_verified,
                "phase2_difference": difference_metrics(left_initial, right_initial),
                "phase3_difference": difference_metrics(left_final, right_final),
                "right_digest": right_digest,
                "right_prime_indices": right_prime_indices,
                "right_prime_values": right_initial[first_tail:],
            }
        )

    write_result(result, args.result)
    return 0 if full_collision else 1


def run_phase2_target_reachability(
    args: argparse.Namespace, primes: list[int]
) -> int:
    source_data = bytes.fromhex(args.message)
    source = simulate_phase2(source_data, primes)
    artifact = json.loads(args.target_result.read_text(encoding="utf-8"))
    if not (
        artifact.get("command") == "phase4-prime-tail-collision"
        and artifact.get("message_schedule_source_hex") == source_data.hex()
        and artifact.get("phase2_local_constraints") is True
    ):
        raise SystemExit("target result is not a compatible local prime-tail artifact")

    tail_cells = int(artifact["tail_cells"])
    first_tail = NUM_CELLS - tail_cells
    tail_indices = [int(value) for value in artifact[f"{args.side}_prime_indices"]]
    target_indices = list(source.prime_indices[:first_tail]) + tail_indices
    target_colors = source.colors
    event_count = 4 * args.bytes + 1

    possible_totals = {0}
    possible_visits_by_cell: list[list[int]] = []
    for index, color in zip(target_indices, target_colors):
        possible_visits = [
            visits
            for visits in range(index + 1)
            if visits % NUM_COLORS == color and visits <= index <= 4 * visits
        ]
        if not possible_visits:
            raise SystemExit("target violates a local Phase-2 visit/index constraint")
        possible_visits_by_cell.append(possible_visits)
        possible_totals = {
            total + visits
            for total in possible_totals
            for visits in possible_visits
        }

    result: dict[str, Any] = {
        "bytes": args.bytes,
        "command": "phase2-target-reachability",
        "event_count": event_count,
        "side": args.side,
        "target_result": str(args.target_result),
        "timeout_ms": args.timeout_ms,
    }
    if event_count not in possible_totals:
        result.update(
            {
                "solver_status": "precheck-unsat",
                "visit_totals": sorted(possible_totals),
            }
        )
        write_result(result, args.result)
        return 1

    prefix_totals: list[set[int]] = [{0}]
    for options in possible_visits_by_cell:
        prefix_totals.append(
            {total + value for total in prefix_totals[-1] for value in options}
        )
    suffix_totals: list[set[int]] = [set() for _ in range(NUM_CELLS + 1)]
    suffix_totals[NUM_CELLS] = {0}
    for cell in range(NUM_CELLS - 1, -1, -1):
        suffix_totals[cell] = {
            value + total
            for value in possible_visits_by_cell[cell]
            for total in suffix_totals[cell + 1]
        }
    for cell, options in enumerate(possible_visits_by_cell):
        possible_visits_by_cell[cell] = [
            value
            for value in options
            if any(
                prefix + value + suffix == event_count
                for prefix in prefix_totals[cell]
                for suffix in suffix_totals[cell + 1]
            )
        ]

    solver = z3.SolverFor("QF_BV")
    solver.set(timeout=args.timeout_ms)
    max_index = max(target_indices)
    prime_mods = [prime & (FIELD_SIZE - 1) for prime in primes[: max_index + 1]]
    symbolic = build_symbolic_message(
        solver, f"target_{args.side}_{args.bytes}", args.bytes, prime_mods
    )
    solver.add(
        symbolic.cursor_x == source.cursor_x,
        symbolic.cursor_y == source.cursor_y,
    )

    active_cells = [
        cell
        for cell, (index, color) in enumerate(zip(target_indices, target_colors))
        if index != 0 or color != 0
    ]
    active_values = [z3.BitVecVal(cell, 8) for cell in active_cells]
    for event_cell in symbolic.event_cells:
        solver.add(z3.Or(*(event_cell == cell for cell in active_values)))

    for cell in active_cells:
        cell_value = z3.BitVecVal(cell, 8)
        index_sum = bitvector_sum(
            [
                z3.If(
                    event_cell == cell_value,
                    delta,
                    z3.BitVecVal(0, INDEX_BITS),
                )
                for event_cell, delta in zip(
                    symbolic.event_cells, symbolic.index_deltas
                )
            ],
            INDEX_BITS,
        )
        visit_sum = bitvector_sum(
            [
                z3.If(
                    event_cell == cell_value,
                    z3.BitVecVal(1, VISIT_BITS),
                    z3.BitVecVal(0, VISIT_BITS),
                )
                for event_cell in symbolic.event_cells
            ],
            VISIT_BITS,
        )
        solver.add(index_sum == target_indices[cell])
        solver.add(
            z3.URem(visit_sum, z3.BitVecVal(NUM_COLORS, VISIT_BITS))
            == target_colors[cell]
        )
        solver.add(
            z3.Or(
                *(
                    visit_sum == z3.BitVecVal(visits, VISIT_BITS)
                    for visits in possible_visits_by_cell[cell]
                )
            )
        )

    started = time.perf_counter()
    status = solver.check()
    result.update(
        {
            "active_cells": active_cells,
            "elapsed_seconds": time.perf_counter() - started,
            "solver_status": str(status),
            "visit_options": {
                str(cell): possible_visits_by_cell[cell] for cell in active_cells
            },
        }
    )
    if status == z3.sat:
        model = solver.model()
        directions = evaluate_directions(model, symbolic.directions)
        message = bytes_from_directions(directions)
        expected = Phase2State(
            tuple(target_indices),
            target_colors,
            source.cursor_x,
            source.cursor_y,
        )
        concrete = simulate_phase2(message, primes)
        oracle = parse_oracle_state(args.oracle, message, primes)
        if concrete != expected or oracle != expected:
            raise RuntimeError("target-reachability model failed concrete replay")
        result.update(
            {
                "message_hex": message.hex(),
                "oracle_verified": True,
            }
        )

    write_result(result, args.result)
    return 0 if status == z3.sat else 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser("validate", help="compare the concrete model with the C oracle")
    validate.add_argument("--oracle", type=Path, required=True)
    validate.add_argument("--trials", type=int, default=100)
    validate.add_argument("--max-bytes", type=int, default=32)
    validate.add_argument("--seed", type=int, default=0x5EC45A)
    validate.add_argument("--symbolic-trials", type=int, default=20)
    validate.add_argument("--symbolic-max-bytes", type=int, default=8)
    validate.add_argument("--result", type=Path)

    search = subparsers.add_parser("search", help="search for exact Phase-2 reconvergence")
    search.add_argument(
        "--mode",
        choices=("reconvergence", "equal-schedule"),
        default="reconvergence",
        help="require a full Phase-2 match or only equal color map and cursor",
    )
    search.add_argument("--bytes-a", type=int, required=True)
    search.add_argument("--bytes-b", type=int, required=True)
    search.add_argument("--prefix-a", help="fix the leading bytes of message A")
    search.add_argument("--prefix-b", help="fix the leading bytes of message B")
    search.add_argument(
        "--block-pair",
        action="append",
        default=[],
        metavar="HEX_A:HEX_B",
        help="exclude one previously found ordered message pair; repeatable",
    )
    search.add_argument(
        "--first-difference",
        type=int,
        help="constrain the first unequal 2-bit direction to this zero-based step",
    )
    search.add_argument(
        "--difference-a",
        type=int,
        choices=range(4),
        help="fix message A's direction at --first-difference",
    )
    search.add_argument(
        "--difference-b",
        type=int,
        choices=range(4),
        help="fix message B's direction at --first-difference",
    )
    search.add_argument("--timeout-ms", type=int, default=300_000)
    search.add_argument("--oracle", type=Path)
    search.add_argument("--result", type=Path)

    analyze = subparsers.add_parser(
        "analyze-pair", help="trace an equal-schedule pair through Phase 3 and Phase 4"
    )
    analyze.add_argument("--message-a", required=True)
    analyze.add_argument("--message-b", required=True)
    analyze.add_argument("--oracle", type=Path, required=True)
    analyze.add_argument("--result", type=Path)

    family = subparsers.add_parser(
        "scan-family", help="scan a structured three-byte XOR-differential family"
    )
    family.add_argument("--difference", default="8002a0")
    family.add_argument("--fixed-last", default="63")
    family.add_argument("--oracle", type=Path, required=True)
    family.add_argument("--top", type=int, default=20)
    family.add_argument("--result", type=Path)

    suffix = subparsers.add_parser(
        "scan-suffix", help="append and exhaustively scan a common one- or two-byte suffix"
    )
    suffix.add_argument("--message-a", required=True)
    suffix.add_argument("--message-b", required=True)
    suffix.add_argument("--suffix-bytes", type=int, default=1)
    suffix.add_argument("--independent", action="store_true")
    suffix.add_argument("--oracle", type=Path, required=True)
    suffix.add_argument("--top", type=int, default=20)
    suffix.add_argument("--result", type=Path)

    inverse = subparsers.add_parser(
        "validate-inverse", help="validate the exact inverse of Phase 3"
    )
    inverse.add_argument("--oracle", type=Path, required=True)
    inverse.add_argument("--raw-trials", type=int, default=100)
    inverse.add_argument("--reachable-trials", type=int, default=50)
    inverse.add_argument("--max-message-bytes", type=int, default=64)
    inverse.add_argument("--seed", type=int, default=0x1A2B3C4D)
    inverse.add_argument("--result", type=Path)

    phase4 = subparsers.add_parser(
        "phase4-collision",
        help="find a second internal state with the digest of a real message",
    )
    phase4.add_argument("--message", required=True)
    phase4.add_argument("--tail-cells", type=int, default=16)
    phase4.add_argument("--timeout-ms", type=int, default=300_000)
    phase4.add_argument("--seed", type=int, default=0x504834)
    phase4.add_argument("--oracle", type=Path, required=True)
    phase4.add_argument("--result", type=Path)

    phase4_tail = subparsers.add_parser(
        "phase4-tail-collision",
        help="find two colliding internal states with a shared concrete prefix",
    )
    phase4_tail.add_argument("--message", required=True)
    phase4_tail.add_argument("--tail-cells", type=int, default=9)
    phase4_tail.add_argument("--chunk-bits", type=int, choices=(1, 2, 4, 8, 16), default=8)
    phase4_tail.add_argument("--timeout-ms", type=int, default=300_000)
    phase4_tail.add_argument("--seed", type=int, default=0x5441494C)
    phase4_tail.add_argument("--oracle", type=Path, required=True)
    phase4_tail.add_argument("--result", type=Path)

    phase4_prime_tail = subparsers.add_parser(
        "phase4-prime-tail-collision",
        help="search a Phase-4 collision over affine, prime-valued Phase-2 tails",
    )
    phase4_prime_tail.add_argument("--message", required=True)
    phase4_prime_tail.add_argument("--tail-cells", type=int, default=10)
    phase4_prime_tail.add_argument(
        "--prime-limit", type=int, default=16,
        help="power-of-two prefix of the production prime table",
    )
    phase4_prime_tail.add_argument(
        "--chunk-bits", type=int, choices=(1, 2, 4, 8, 16), default=1
    )
    phase4_prime_tail.add_argument(
        "--output-bits", type=int, default=512,
        help="digest prefix to collide (64, 128, ..., 512)",
    )
    phase4_prime_tail.add_argument(
        "--explicit-carries", action="store_true",
        help="encode each MAR addition as a ripple-carry circuit",
    )
    phase4_prime_tail.add_argument("--timeout-ms", type=int, default=300_000)
    phase4_prime_tail.add_argument("--seed", type=int, default=0x5052494D)
    phase4_prime_tail.add_argument("--oracle", type=Path, required=True)
    phase4_prime_tail.add_argument("--resume-result", type=Path)
    phase4_prime_tail.add_argument("--result", type=Path)

    target = subparsers.add_parser(
        "phase2-target-reachability",
        help="search for a message that reaches one prime-tail Phase-2 target",
    )
    target.add_argument("--message", required=True, help="schedule-source message")
    target.add_argument("--target-result", type=Path, required=True)
    target.add_argument("--side", choices=("left", "right"), required=True)
    target.add_argument("--bytes", type=int, required=True)
    target.add_argument("--timeout-ms", type=int, default=300_000)
    target.add_argument("--oracle", type=Path, required=True)
    target.add_argument("--result", type=Path)

    args = parser.parse_args()
    if args.command == "search":
        pair_is_partial = (args.difference_a is None) != (args.difference_b is None)
        if pair_is_partial:
            parser.error("--difference-a and --difference-b must be used together")
        if args.difference_a is not None and args.first_difference is None:
            parser.error("fixed difference directions require --first-difference")
        if args.difference_a is not None and args.difference_a == args.difference_b:
            parser.error("fixed difference directions must be unequal")
    for name in (
        "bytes_a",
        "bytes_b",
        "first_difference",
        "trials",
        "max_bytes",
        "symbolic_trials",
        "symbolic_max_bytes",
        "timeout_ms",
    ):
        if hasattr(args, name) and getattr(args, name) is not None and getattr(args, name) < 0:
            parser.error(f"--{name.replace('_', '-')} must be non-negative")
    return args


def main() -> int:
    args = parse_args()
    primes = load_production_primes()
    if args.command == "validate":
        return run_validation(args, primes)
    if args.command == "analyze-pair":
        return run_pair_analysis(args, primes)
    if args.command == "scan-family":
        return run_family_scan(args, primes)
    if args.command == "scan-suffix":
        return run_suffix_scan(args, primes)
    if args.command == "validate-inverse":
        return run_inverse_validation(args, primes)
    if args.command == "phase4-collision":
        return run_phase4_collision(args, primes)
    if args.command == "phase4-tail-collision":
        return run_phase4_tail_collision(args, primes)
    if args.command == "phase4-prime-tail-collision":
        return run_phase4_prime_tail_collision(args, primes)
    if args.command == "phase2-target-reachability":
        return run_phase2_target_reachability(args, primes)
    return run_search(args, primes)


if __name__ == "__main__":
    raise SystemExit(main())
