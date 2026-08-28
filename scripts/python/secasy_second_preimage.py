#!/usr/bin/env python3
"""Create a Secasy second preimage for arbitrary file, text, or hex input.

For the current production configuration, 2,131,224 copies of byte 0x00
form a neutral Phase-2 prefix. Therefore

    H(message) == H(00^2,131,224 || message)

for the file-hashing interface. Byte 0xAA is an alternative neutral prefix.
The tool streams file inputs, writes the output atomically, and verifies the
full 512-bit collision with the production Secasy executable by default.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator


ROOT = Path(__file__).resolve().parents[2]
NEUTRAL_PREFIX_BYTES = 2_131_224
COPY_CHUNK_BYTES = 1024 * 1024


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate m2 = neutral-prefix || m and verify that the current "
            "Secasy implementation gives m and m2 the same 512-bit digest."
        )
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--file", type=Path, help="arbitrary binary input file m")
    source.add_argument("--text", help="UTF-8 text input m")
    source.add_argument("--hex", dest="hex_data", help="hexadecimal byte input m")
    parser.add_argument("--output", type=Path, required=True, help="output file for m2")
    parser.add_argument(
        "--prefix-byte",
        choices=("00", "aa"),
        default="00",
        help="neutral repeated byte, default: 00",
    )
    parser.add_argument(
        "--oracle",
        type=Path,
        help="Secasy executable, auto-detected under build when omitted",
    )
    parser.add_argument(
        "--skip-verify",
        action="store_true",
        help="create m2 without invoking the production Secasy executable",
    )
    parser.add_argument("--force", action="store_true", help="replace an existing output")
    parser.add_argument("--result", type=Path, help="optional JSON result path")
    return parser.parse_args()


def decode_hex(value: str) -> bytes:
    compact = re.sub(r"(?:0x|[\s,;:_-])", "", value, flags=re.IGNORECASE)
    if len(compact) % 2 != 0:
        raise SystemExit("--hex must contain complete bytes")
    try:
        return bytes.fromhex(compact)
    except ValueError as exc:
        raise SystemExit("--hex contains a non-hexadecimal character") from exc


def resolve_source(args: argparse.Namespace) -> tuple[str, Path | None, bytes | None, int]:
    if args.file is not None:
        path = args.file.resolve()
        if not path.is_file():
            raise SystemExit(f"input file does not exist or is not a regular file: {path}")
        return "file", path, None, path.stat().st_size
    if args.text is not None:
        data = args.text.encode("utf-8")
        return "text", None, data, len(data)
    data = decode_hex(args.hex_data)
    return "hex", None, data, len(data)


def resolve_oracle(explicit: Path | None) -> Path:
    candidates = [explicit] if explicit is not None else [
        ROOT / "build" / "Secasy.exe",
        ROOT / "build" / "Secasy",
    ]
    for candidate in candidates:
        if candidate is not None and candidate.resolve().is_file():
            return candidate.resolve()
    if explicit is not None:
        raise SystemExit(f"Secasy executable not found: {explicit.resolve()}")
    raise SystemExit(
        "Secasy executable not found under build; compile it or pass --oracle"
    )


def write_repeated_byte(output: object, value: int, count: int) -> None:
    chunk = bytes([value]) * COPY_CHUNK_BYTES
    remaining = count
    while remaining:
        amount = min(remaining, len(chunk))
        output.write(chunk[:amount])
        remaining -= amount


def write_candidate(
    destination: Path,
    prefix_byte: int,
    source_path: Path | None,
    source_data: bytes | None,
) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as output:
            write_repeated_byte(output, prefix_byte, NEUTRAL_PREFIX_BYTES)
            if source_path is not None:
                with source_path.open("rb") as source:
                    shutil.copyfileobj(source, output, COPY_CHUNK_BYTES)
            else:
                output.write(source_data or b"")
            output.flush()
            os.fsync(output.fileno())
        return temporary
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


@contextmanager
def source_hash_path(source_path: Path | None, source_data: bytes | None) -> Iterator[Path]:
    if source_path is not None:
        yield source_path
        return
    with tempfile.TemporaryDirectory(prefix="secasy-source-") as directory:
        path = Path(directory) / "message.bin"
        path.write_bytes(source_data or b"")
        yield path


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


def write_json_result(path: Path, result: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(result, indent=2, sort_keys=True) + "\n"
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
            output.write(encoded)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def main() -> int:
    args = parse_args()
    source_kind, source_path, source_data, source_bytes = resolve_source(args)
    destination = args.output.resolve()
    result_destination = args.result.resolve() if args.result is not None else None

    if source_path is not None and source_path == destination:
        raise SystemExit("input and output paths must differ")
    if result_destination is not None:
        if result_destination == destination:
            raise SystemExit("--result and --output paths must differ")
        if source_path is not None and result_destination == source_path:
            raise SystemExit("--result must not overwrite the input file")
        if result_destination.exists() and not args.force:
            raise SystemExit(
                f"result already exists; pass --force to replace it: {result_destination}"
            )
    if destination.exists() and not args.force:
        raise SystemExit(f"output already exists; pass --force to replace it: {destination}")

    prefix_byte = int(args.prefix_byte, 16)
    temporary = write_candidate(destination, prefix_byte, source_path, source_data)
    oracle: Path | None = None
    digest_original: str | None = None
    digest_candidate: str | None = None
    verified = False

    try:
        if not args.skip_verify:
            oracle = resolve_oracle(args.oracle)
            with source_hash_path(source_path, source_data) as original:
                digest_original = hash_file(oracle, original)
            digest_candidate = hash_file(oracle, temporary)
            verified = digest_original == digest_candidate
            if not verified:
                raise RuntimeError(
                    "production verification failed; the temporary candidate was not published"
                )
        os.replace(temporary, destination)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise

    result: dict[str, object] = {
        "command": "secasy-second-preimage",
        "digest_original": digest_original,
        "digest_second_preimage": digest_candidate,
        "formula": f"m2 = {args.prefix_byte}^{NEUTRAL_PREFIX_BYTES} || m",
        "neutral_prefix_byte": args.prefix_byte,
        "neutral_prefix_bytes": NEUTRAL_PREFIX_BYTES,
        "oracle": str(oracle) if oracle is not None else None,
        "output": str(destination),
        "second_preimage_bytes": destination.stat().st_size,
        "source_bytes": source_bytes,
        "source_kind": source_kind,
        "verified": verified,
        "verification_skipped": args.skip_verify,
    }

    if result_destination is not None:
        write_json_result(result_destination, result)

    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
