import argparse
import ctypes as C
import random
import time
from dataclasses import dataclass
from pathlib import Path

from fluxjit import FluxJit, default_lib_path


@dataclass
class PolicyConfig:
    threshold: float = 0.8
    hysteresis: float = 0.05
    damage: float = 1.0
    work_factor: int = 50


class AutoPolicyDamageEngine:
    def __init__(self, jit: FluxJit, config: PolicyConfig):
        self.jit = jit
        self.config = config
        self.current_strategy = None
        self.current_kernel = None
        self.hot_swaps = 0

    def close(self):
        if self.current_kernel is not None:
            self.jit.destroy_kernel(self.current_kernel)
            self.current_kernel = None

    def _target_strategy(self, alive_ratio: float) -> str:
        # Hysteresis band prevents rapid flipping around threshold.
        low = self.config.threshold - self.config.hysteresis
        high = self.config.threshold + self.config.hysteresis

        if self.current_strategy is None:
            return "compacted" if alive_ratio < self.config.threshold else "masked"

        if self.current_strategy == "masked":
            return "compacted" if alive_ratio < low else "masked"

        return "masked" if alive_ratio > high else "compacted"

    def _compile_strategy(self, strategy: str):
        ir = {
            "stream": "Damage",
            "damage": self.config.damage,
            "work_factor": self.config.work_factor,
        }
        kernel = self.jit.compile(ir, strategy=strategy)
        return kernel

    def maybe_swap(self, alive_ratio: float):
        target = self._target_strategy(alive_ratio)
        if target == self.current_strategy:
            return False

        new_kernel = self._compile_strategy(target)
        old = self.current_kernel
        self.current_kernel = new_kernel
        self.current_strategy = target
        if old is not None:
            self.jit.destroy_kernel(old)
        self.hot_swaps += 1
        return True

    def run_damage(self, health, out_health, alive_flags):
        self.jit.run_f32(self.current_kernel, health, None, out_health, alive_flags)


def infer_threshold_from_sweep(markdown_path: str, work_factor: int) -> float:
    path = Path(markdown_path)
    if not path.exists():
        raise FileNotFoundError(f"sweep file not found: {markdown_path}")

    rows = []
    with path.open("r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line.startswith("|"):
                continue
            if "---" in line or "work_factor" in line:
                continue

            parts = [p.strip() for p in line.strip("|").split("|")]
            if len(parts) < 8:
                continue

            try:
                wf = int(parts[0])
                ratio = float(parts[1])
                winner = parts[6]
            except ValueError:
                continue

            if wf == work_factor:
                rows.append((ratio, winner))

    if not rows:
        raise ValueError(
            f"no rows found in sweep for work_factor={work_factor}; "
            "run autoscale.py --sweep with matching --work-factors first"
        )

    rows.sort(key=lambda x: x[0])
    compacted_ratios = [r for r, w in rows if w == "compacted"]
    masked_ratios = [r for r, w in rows if w == "masked"]

    if compacted_ratios and masked_ratios:
        max_compacted = max(compacted_ratios)
        min_masked = min(masked_ratios)
        if max_compacted <= min_masked:
            return (max_compacted + min_masked) * 0.5

        # Non-monotonic winners: select threshold from boundary nearest 0.5.
        boundaries = []
        for i in range(1, len(rows)):
            if rows[i - 1][1] != rows[i][1]:
                boundaries.append((rows[i - 1][0] + rows[i][0]) * 0.5)
        if boundaries:
            return min(boundaries, key=lambda b: abs(b - 0.5))

    if compacted_ratios and not masked_ratios:
        return 1.0
    if masked_ratios and not compacted_ratios:
        return 0.0

    return 0.8


def count_alive(health_arr) -> int:
    return sum(1 for v in health_arr if v > 0.0)


def make_health(n: int, alive_ratio: float):
    vals = []
    for _ in range(n):
        if random.random() < alive_ratio:
            vals.append(random.uniform(1.0, 100.0))
        else:
            vals.append(random.uniform(-10.0, 0.0))
    return (C.c_float * n)(*vals)


def mutate_density(health, target_alive_ratio: float, step: float = 0.15):
    n = len(health)
    current = count_alive(health)
    target = int(target_alive_ratio * n)

    if current < target:
        need = int((target - current) * step)
        flips = 0
        i = 0
        while flips < need and i < n * 2:
            idx = (i * 97) % n
            if health[idx] <= 0.0:
                health[idx] = random.uniform(1.0, 100.0)
                flips += 1
            i += 1
    elif current > target:
        need = int((current - target) * step)
        flips = 0
        i = 0
        while flips < need and i < n * 2:
            idx = (i * 89) % n
            if health[idx] > 0.0:
                health[idx] = random.uniform(-10.0, 0.0)
                flips += 1
            i += 1


def run_demo(args):
    random.seed(args.seed)

    jit = FluxJit(args.lib)
    threshold = args.threshold
    if args.policy_from_sweep:
        wf_for_policy = args.policy_work_factor if args.policy_work_factor >= 0 else args.work_factor
        threshold = infer_threshold_from_sweep(args.policy_from_sweep, wf_for_policy)

    cfg = PolicyConfig(
        threshold=args.threshold,
        hysteresis=args.hysteresis,
        damage=args.damage,
        work_factor=args.work_factor,
    )
    cfg.threshold = threshold
    engine = AutoPolicyDamageEngine(jit, cfg)

    try:
        health = make_health(args.n, args.start_alive_ratio)
        out_health = (C.c_float * args.n)()
        alive_flags = (C.c_float * args.n)()

        schedule = [float(x.strip()) for x in args.density_schedule.split(",") if x.strip()]
        if not schedule:
            raise SystemExit("--density-schedule must contain at least one ratio")

        print("=== FluxJIT Auto-Policy Demo (Damage) ===")
        if args.policy_from_sweep:
            wf_for_policy = args.policy_work_factor if args.policy_work_factor >= 0 else args.work_factor
            print(
                f"policy_from_sweep={args.policy_from_sweep}, policy_work_factor={wf_for_policy}, "
                f"derived_threshold={threshold:.3f}"
            )
        print(
            f"n={args.n}, threshold={threshold}, hysteresis={args.hysteresis}, "
            f"work_factor={args.work_factor}, frames={args.frames}"
        )

        t0 = time.perf_counter_ns()
        for frame in range(args.frames):
            target_ratio = schedule[frame % len(schedule)]
            mutate_density(health, target_ratio)
            alive_ratio = count_alive(health) / args.n
            swapped = engine.maybe_swap(alive_ratio)

            frame_start = time.perf_counter_ns()
            engine.run_damage(health, out_health, alive_flags)
            frame_end = time.perf_counter_ns()

            if frame % args.log_every == 0 or swapped:
                swap_note = " [hot-swap]" if swapped else ""
                print(
                    f"frame={frame:04d} alive_ratio={alive_ratio:.3f} "
                    f"strategy={engine.current_strategy:<9} "
                    f"runtime={(frame_end - frame_start):,} ns{swap_note}"
                )

        t1 = time.perf_counter_ns()
        total_ms = (t1 - t0) / 1_000_000.0
        print("\nSummary:")
        print(f"total_frames = {args.frames}")
        print(f"hot_swaps    = {engine.hot_swaps}")
        print(f"total_time   = {total_ms:.3f} ms")
        print(f"avg/frame    = {total_ms / args.frames:.3f} ms")
    finally:
        engine.close()
        jit.close()


def main():
    parser = argparse.ArgumentParser(description="Self-healing strategy policy demo for FluxJIT Damage kernel")
    parser.add_argument("--n", type=int, default=200_000)
    parser.add_argument("--frames", type=int, default=60)
    parser.add_argument("--start-alive-ratio", type=float, default=0.2)
    parser.add_argument("--density-schedule", type=str, default="0.1,0.2,0.4,0.7,0.9,1.0,0.6,0.2")
    parser.add_argument("--threshold", type=float, default=0.8)
    parser.add_argument("--policy-from-sweep", type=str, default="", help="Path to sweep markdown table for automatic threshold inference")
    parser.add_argument("--policy-work-factor", type=int, default=-1, help="Work factor row to use from sweep file (-1 => use --work-factor)")
    parser.add_argument("--hysteresis", type=float, default=0.05)
    parser.add_argument("--damage", type=float, default=1.0)
    parser.add_argument("--work-factor", type=int, default=50)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--log-every", type=int, default=5)
    parser.add_argument("--lib", type=str, default=default_lib_path())
    args = parser.parse_args()

    if not (0.0 <= args.threshold <= 1.0):
        raise SystemExit("--threshold must be in [0,1]")
    if args.policy_work_factor < -1:
        raise SystemExit("--policy-work-factor must be >= -1")
    if args.hysteresis < 0.0:
        raise SystemExit("--hysteresis must be >= 0")
    if args.work_factor < 0:
        raise SystemExit("--work-factor must be >= 0")

    run_demo(args)


if __name__ == "__main__":
    main()
