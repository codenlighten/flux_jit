import argparse
import ctypes as C
import random
import time
from typing import List, Tuple

from fluxjit import FluxJit, default_lib_path


def make_health(n: int, alive_ratio: float) -> C.Array:
    values = []
    for _ in range(n):
        if random.random() < alive_ratio:
            # Alive entities get positive health
            values.append(random.uniform(1.0, 100.0))
        else:
            # Dead/inactive entities get non-positive health
            values.append(random.uniform(-10.0, 0.0))
    return (C.c_float * n)(*values)


def run_kernel_once(
    fx: FluxJit,
    kernel,
    health: C.Array,
    out_health: C.Array,
    alive_flags: C.Array,
) -> int:
    start = time.perf_counter_ns()
    fx.run_f32(kernel, health, None, out_health, alive_flags)
    end = time.perf_counter_ns()
    return end - start


def benchmark_strategy(
    fx: FluxJit,
    strategy: str,
    health: C.Array,
    damage: float,
    warmup: int,
    iters: int,
) -> Tuple[float, float]:
    n = len(health)
    out_health = (C.c_float * n)()
    alive_flags = (C.c_float * n)()

    ir = {
        "stream": "Damage",
        "damage": float(damage),
    }
    kernel = fx.compile(ir, strategy=strategy)

    # Warmup
    for _ in range(warmup):
        _ = run_kernel_once(fx, kernel, health, out_health, alive_flags)

    # Timed
    samples = []
    for _ in range(iters):
        ns = run_kernel_once(fx, kernel, health, out_health, alive_flags)
        samples.append(ns)

    fx.destroy_kernel(kernel)

    mean_ns = sum(samples) / len(samples)
    ns_per_elem = mean_ns / n
    return mean_ns, ns_per_elem


def tune_one_ratio(
    fx: FluxJit,
    n: int,
    alive_ratio: float,
    damage: float,
    work_factor: int,
    warmup: int,
    iters: int,
) -> Tuple[float, float, float, float, str, float]:
    health = make_health(n, alive_ratio)

    masked_mean, masked_npe = benchmark_strategy(
        fx, "masked", health, damage, warmup, iters
    )
    compact_mean, compact_npe = benchmark_strategy(
        fx, "compacted", health, damage, warmup, iters
    )

    winner = "masked" if masked_mean <= compact_mean else "compacted"
    speedup = (max(masked_mean, compact_mean) / min(masked_mean, compact_mean)) if min(masked_mean, compact_mean) > 0 else 1.0
    return masked_mean, masked_npe, compact_mean, compact_npe, winner, speedup


def parse_ratios(raw: str) -> List[float]:
    items = [x.strip() for x in raw.split(",") if x.strip()]
    ratios = [float(x) for x in items]
    for r in ratios:
        if not (0.0 <= r <= 1.0):
            raise ValueError(f"alive ratio out of range [0,1]: {r}")
    return ratios


def parse_int_list(raw: str) -> List[int]:
    items = [x.strip() for x in raw.split(",") if x.strip()]
    vals = [int(x) for x in items]
    for v in vals:
        if v < 0:
            raise ValueError(f"work_factor must be >= 0: {v}")
    return vals


def main() -> None:
    parser = argparse.ArgumentParser(description="FluxJIT strategy auto-tuner for Damage kernel")
    parser.add_argument("--n", type=int, default=1_000_000, help="Number of entities")
    parser.add_argument("--alive-ratio", type=float, default=0.5, help="Fraction of alive entities [0..1]")
    parser.add_argument("--damage", type=float, default=1.0, help="Damage value")
    parser.add_argument("--work-factor", type=int, default=0, help="Synthetic heavy-payload iterations")
    parser.add_argument("--warmup", type=int, default=5, help="Warmup iterations per strategy")
    parser.add_argument("--iters", type=int, default=20, help="Timed iterations per strategy")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--lib", type=str, default=default_lib_path(), help="Path to libfluxjit.so")
    parser.add_argument("--sweep", action="store_true", help="Run across multiple alive ratios")
    parser.add_argument(
        "--ratios",
        type=str,
        default="0.01,0.05,0.1,0.2,0.5,0.8,1.0",
        help="Comma-separated alive ratios used when --sweep is enabled",
    )
    parser.add_argument(
        "--work-factors",
        type=str,
        default="",
        help="Optional comma-separated work factors for 2D sweep (e.g. 0,10,50)",
    )
    parser.add_argument(
        "--markdown-out",
        type=str,
        default="",
        help="Optional path to write markdown table output",
    )
    args = parser.parse_args()

    if not (0.0 <= args.alive_ratio <= 1.0):
        raise SystemExit("--alive-ratio must be in [0, 1]")

    random.seed(args.seed)

    fx = FluxJit(args.lib)
    try:
        if not args.sweep:
            masked_mean, masked_npe, compact_mean, compact_npe, winner, speedup = tune_one_ratio(
                fx,
                n=args.n,
                alive_ratio=args.alive_ratio,
                damage=args.damage,
                work_factor=args.work_factor,
                warmup=args.warmup,
                iters=args.iters,
            )

            print("=== FluxJIT Auto-Tuner (Damage) ===")
            print(f"n={args.n}, alive_ratio={args.alive_ratio:.3f}, damage={args.damage}, work_factor={args.work_factor}, warmup={args.warmup}, iters={args.iters}")
            print(f"masked    : {masked_mean:,.1f} ns/run  ({masked_npe:.3f} ns/elem)")
            print(f"compacted : {compact_mean:,.1f} ns/run  ({compact_npe:.3f} ns/elem)")
            print(f"winner    : {winner}  (~{speedup:.2f}x faster than alternative)")

            print("\nSuggested compile call:")
            print(
                f"kernel = jit.compile({{'stream': 'Damage', 'damage': {args.damage}, 'work_factor': {args.work_factor}}}, strategy='{winner}')"
            )
        else:
            ratios = parse_ratios(args.ratios)
            work_factors = parse_int_list(args.work_factors) if args.work_factors else [args.work_factor]
            rows = []
            for wf in work_factors:
                for ratio in ratios:
                    masked_mean, masked_npe, compact_mean, compact_npe, winner, speedup = tune_one_ratio(
                        fx,
                        n=args.n,
                        alive_ratio=ratio,
                        damage=args.damage,
                        work_factor=wf,
                        warmup=args.warmup,
                        iters=args.iters,
                    )
                    rows.append((wf, ratio, masked_mean, masked_npe, compact_mean, compact_npe, winner, speedup))

            print("=== FluxJIT Strategy Sweep (Damage) ===")
            print(
                f"n={args.n}, ratios={args.ratios}, work_factors={','.join(str(x) for x in work_factors)}, "
                f"damage={args.damage}, warmup={args.warmup}, iters={args.iters}"
            )
            print()

            md_lines = [
                "| work_factor | alive_ratio | masked ns/run | compacted ns/run | masked ns/elem | compacted ns/elem | winner | speedup |",
                "|---:|---:|---:|---:|---:|---:|:---|---:|",
            ]

            for wf, ratio, masked_mean, masked_npe, compact_mean, compact_npe, winner, speedup in rows:
                md_lines.append(
                    f"| {wf} | {ratio:.3f} | {masked_mean:,.1f} | {compact_mean:,.1f} | {masked_npe:.3f} | {compact_npe:.3f} | {winner} | {speedup:.2f}x |"
                )

            table = "\n".join(md_lines)
            print(table)

            if args.markdown_out:
                with open(args.markdown_out, "w", encoding="utf-8") as f:
                    f.write("# FluxJIT Damage Strategy Sweep\n\n")
                    f.write(table)
                    f.write("\n")
                print(f"\nWrote markdown table: {args.markdown_out}")
    finally:
        fx.close()


if __name__ == "__main__":
    main()
