#!/usr/bin/env python3
"""Scan reduced Secasy parameters.

Goal: try smaller round counts and different `hashLengthInBits` parameters and
flag configurations that look weak via existing harnesses.

Notes:
- `hashLengthInBits` in this codebase is a *parameter* forwarded into the core
  that controls hash-output behavior/buffer sizing; it is not always equal
  to the produced hash bit-length.
- Some statistical tests can be flaky at low trial counts. This scanner
  confirms any Extended-Security overall failure with higher trials and
  multiple seeds before reporting it.
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import time
from dataclasses import dataclass
from itertools import product


@dataclass(frozen=True)
class DifferentialResult:
	ok: bool


@dataclass(frozen=True)
class ExtendedResult:
	ok: bool
	produced_bits: int | None
	max_corr: float | None
	high_corr_rate: float | None


DIFF_RE_STATUS = re.compile(r"Status:\s*(.*)")

EXT_RE_SUMMARY = re.compile(r"SUMMARY:\s*(\d+)\s*/\s*(\d+)")
EXT_RE_PRODUCED = re.compile(r"Produced hash length:\s*(\d+) hex chars \((\d+) bits\)")
EXT_RE_MAXCORR = re.compile(r"Max correlation:\s*([-+0-9.]+)")
EXT_RE_HICORR = re.compile(r"High correlation rate:\s*([0-9.]+)%")


def run(cmd: list[str], cwd: str, timeout: int, env_extra: dict[str, str] | None = None) -> str:
	env = os.environ.copy()
	if env_extra:
		env.update(env_extra)
	try:
		p = subprocess.run(
			cmd,
			cwd=cwd,
			env=env,
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			text=True,
			timeout=timeout,
		)
		return p.stdout
	except subprocess.TimeoutExpired as e:
		out = e.stdout or ""
		return out + "\n<<TIMEOUT>>\n"


def safe_name(s: str) -> str:
	return re.sub(r"[^a-zA-Z0-9_.-]+", "_", s).strip("_")


def write_text(path: str, content: str) -> None:
	os.makedirs(os.path.dirname(path), exist_ok=True)
	with open(path, "w", encoding="utf-8", newline="\n") as f:
		f.write(content)


def parse_differential(out: str) -> DifferentialResult:
	statuses = [m.group(1).strip().lower() for m in DIFF_RE_STATUS.finditer(out)]
	ok = bool(statuses) and all(("warning" not in s and "fail" not in s) for s in statuses)
	return DifferentialResult(ok=ok)


def parse_extended(out: str) -> ExtendedResult:
	msum = EXT_RE_SUMMARY.search(out)
	ok = bool(msum and msum.group(1) == msum.group(2))

	mprod = EXT_RE_PRODUCED.search(out)
	produced_bits = int(mprod.group(2)) if mprod else None

	mc = EXT_RE_MAXCORR.search(out)
	max_corr = float(mc.group(1)) if mc else None

	mrate = EXT_RE_HICORR.search(out)
	high_corr_rate = float(mrate.group(1)) if mrate else None

	return ExtendedResult(ok=ok, produced_bits=produced_bits, max_corr=max_corr, high_corr_rate=high_corr_rate)


def main() -> int:
	ap = argparse.ArgumentParser()
	ap.add_argument("--build", default="build", help="Build directory containing executables")
	ap.add_argument("--timeout", type=int, default=120, help="Per-process timeout seconds")
	ap.add_argument("--trials", type=int, default=200, help="Extended-security trials")
	ap.add_argument("--confirm-trials", type=int, default=500, help="Trials for confirming an extended-security failure")
	ap.add_argument("--confirm-retries", type=int, default=2, help="How many confirmation runs to attempt")
	ap.add_argument(
		"--confirm-mode",
		choices=["fast", "full"],
		default="fast",
		help="Confirmation mode for EXT failures: 'fast' keeps SECASY_EXT_FAST enabled; 'full' runs the full suite.",
	)
	ap.add_argument(
		"--no-confirm",
		action="store_true",
		help="Do not run confirmation loops; flag any EXT failure immediately.",
	)
	ap.add_argument(
		"--max-confirm-configs",
		type=int,
		default=3,
		help="Maximum number of distinct configurations to run EXT confirmation for (0 = unlimited).",
	)
	ap.add_argument("--seed", type=int, default=42)
	ap.add_argument("--rounds", default="50,100,200,500")
	ap.add_argument("--prime-index", default="100,200")
	ap.add_argument("--nbits", default="64,128,256")
	ap.add_argument(
		"--report-dir",
		default="",
		help="If set, write outputs for flagged configs to this directory",
	)
	args = ap.parse_args()

	root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
	build = os.path.abspath(os.path.join(root, args.build))

	diff_exe = os.path.join(build, "SecasyDifferential.exe")
	ext_exe = os.path.join(build, "SecasyExtendedSecurity.exe")

	if not os.path.exists(diff_exe) or not os.path.exists(ext_exe):
		print("Missing executables. Build targets first:")
		print("  cmake -S . -B build")
		print("  cmake --build build --config Release --target SecasyDifferential SecasyExtendedSecurity")
		return 2

	rounds = [int(x) for x in args.rounds.split(",") if x.strip()]
	prime_indices = [int(x) for x in args.prime_index.split(",") if x.strip()]
	nbits = [int(x) for x in args.nbits.split(",") if x.strip()]

	print("Scanning configs...")
	print(f"Rounds: {rounds}")
	print(f"PrimeIndex: {prime_indices}")
	print(f"hashLengthInBits param: {nbits}")
	print("\nLegend: flags show potentially weak reduced configs.")

	report_dir = ""
	summary_lines: list[str] = []
	if args.report_dir:
		ts = time.strftime("%Y%m%d_%H%M%S")
		report_dir = os.path.abspath(os.path.join(root, args.report_dir, ts))
		os.makedirs(report_dir, exist_ok=True)
		summary_lines.append(
			"rounds\tprimeIndex\tnumberOfBits\tflags\tproducedBits\tdiff_ok\text_ok\text_maxCorr\text_highCorrRate\n"
		)

	flagged = 0
	confirmed_cfgs = 0
	interrupted = False

	try:
		for r, pi, nb in product(rounds, prime_indices, nbits):
			diff_cmd = [diff_exe, str(pi), str(r), str(nb)]
			diff_out = run(diff_cmd, cwd=root, timeout=args.timeout, env_extra={"SECASY_DIFF_FAST": "1"})
			dres = parse_differential(diff_out)

			ext_cmd = [
				ext_exe,
				"-t",
				str(args.trials),
				"-r",
				str(r),
				"-i",
				str(pi),
				"-n",
				str(nb),
				"-s",
				str(args.seed),
			]
			ext_out = run(ext_cmd, cwd=root, timeout=args.timeout, env_extra={"SECASY_EXT_FAST": "1"})
			eres = parse_extended(ext_out)

			ext_confirm_outs: list[tuple[int, str]] = []

			ext_timed_out = "<<TIMEOUT>>" in ext_out
			empty_output = (eres.produced_bits == 0)

			# Confirm any EXT failure with higher trials and multiple seeds.
			confirmed_fail = False
			skip_reason: str | None = None
			if not eres.ok:
				skip_confirm_budget = (args.max_confirm_configs > 0 and confirmed_cfgs >= args.max_confirm_configs)
				if empty_output or ext_timed_out:
					confirmed_fail = True
				elif args.no_confirm:
					skip_reason = "no-confirm"
					confirmed_fail = True
				elif skip_confirm_budget:
					skip_reason = "budget"
					confirmed_fail = True
				else:
					confirmed_cfgs += 1
					confirmed_fail = True
					for retry in range(args.confirm_retries):
						seed = args.seed + 1000 + retry
						confirm_cmd = [
							ext_exe,
							"-t",
							str(args.confirm_trials),
							"-r",
							str(r),
							"-i",
							str(pi),
							"-n",
							str(nb),
							"-s",
							str(seed),
						]
						env_extra = None
						if args.confirm_mode == "fast":
							env_extra = {"SECASY_EXT_FAST": "1"}
						confirm_out = run(confirm_cmd, cwd=root, timeout=args.timeout, env_extra=env_extra)
						ext_confirm_outs.append((seed, confirm_out))
						confirm_res = parse_extended(confirm_out)
						if confirm_res.ok:
							confirmed_fail = False
							eres = confirm_res
							break
					if not confirmed_fail:
						# Treat as flaky and do not flag.
						pass

			flags: list[str] = []
			if not dres.ok:
				flags.append("DIFF")
			if not eres.ok and confirmed_fail:
				if empty_output:
					flags.append("EXT(empty)")
				elif ext_timed_out:
					flags.append("EXT(timeout)")
				elif skip_reason == "no-confirm":
					flags.append("EXT(no-confirm)")
				elif skip_reason == "budget":
					flags.append("EXT(unconfirmed)")
				else:
					flags.append("EXT(confirm)")

			if flags:
				flagged += 1
				print(f"r={r:4d} pi={pi:4d} nb={nb:4d}  FLAGS={','.join(flags)}  producedBits={eres.produced_bits}")

				if report_dir:
					base = safe_name(f"r{r}_pi{pi}_nb{nb}")
					write_text(os.path.join(report_dir, f"{base}.diff.txt"), diff_out)
					write_text(os.path.join(report_dir, f"{base}.ext.txt"), ext_out)
					for seed_val, out_text in ext_confirm_outs:
						write_text(os.path.join(report_dir, f"{base}.ext.confirm.seed{seed_val}.txt"), out_text)

			# Always record the row in the summary (even if not flagged)
			if report_dir:
				summary_lines.append(
					"\t".join(
						[
							str(r),
							str(pi),
							str(nb),
							",".join(flags),
							str(eres.produced_bits) if eres.produced_bits is not None else "",
							"1" if dres.ok else "0",
							"1" if eres.ok else "0",
							f"{eres.max_corr}" if eres.max_corr is not None else "",
							f"{eres.high_corr_rate}" if eres.high_corr_rate is not None else "",
						]
					)
					+ "\n"
				)
	except KeyboardInterrupt:
		interrupted = True
		print("\nInterrupted. Writing partial report...")

	if flagged == 0:
		print("\nNo suspicious configs flagged by these heuristics.")
	else:
		print(f"\nFlagged {flagged} config(s). Consider increasing trials for deeper analysis.")

	if report_dir:
		write_text(os.path.join(report_dir, "summary.tsv"), "".join(summary_lines))
		print(f"\nReport written to: {report_dir}")
		if interrupted:
			print("(Partial results due to interruption.)")

	return 0


if __name__ == "__main__":
	raise SystemExit(main())
