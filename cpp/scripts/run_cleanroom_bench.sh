#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./scripts/run_cleanroom_bench.sh <benchmark_regex> [core] [repetitions] [binary]
# Example:
#   ./scripts/run_cleanroom_bench.sh 'BM_OOP_PointerChasing/1048576$' 2 20 ./build/ecs_benchmarks

BENCH_REGEX="${1:-BM_OOP_PointerChasing/1048576$}"
CORE="${2:-2}"
REPS="${3:-20}"
BIN="${4:-./build/ecs_benchmarks}"
OUT_DIR="${OUT_DIR:-./profiling}"
mkdir -p "$OUT_DIR"

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required (sudo apt-get install -y jq)" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  echo "Benchmark binary not found or not executable: $BIN" >&2
  exit 1
fi

# Save governors and restore on exit.
mapfile -t GOV_FILES < <(ls /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true)
declare -A GOV_OLD
for f in "${GOV_FILES[@]}"; do
  GOV_OLD["$f"]="$(cat "$f")"
done

NO_TURBO_FILE="/sys/devices/system/cpu/intel_pstate/no_turbo"
NO_TURBO_OLD=""
if [[ -f "$NO_TURBO_FILE" ]]; then
  NO_TURBO_OLD="$(cat "$NO_TURBO_FILE")"
fi

restore() {
  for f in "${!GOV_OLD[@]}"; do
    echo "${GOV_OLD[$f]}" | sudo tee "$f" >/dev/null || true
  done
  if [[ -n "$NO_TURBO_OLD" && -f "$NO_TURBO_FILE" ]]; then
    echo "$NO_TURBO_OLD" | sudo tee "$NO_TURBO_FILE" >/dev/null || true
  fi
}
trap restore EXIT

# Lock CPU policy for less noisy runs.
for f in "${GOV_FILES[@]}"; do
  echo performance | sudo tee "$f" >/dev/null
 done
if [[ -f "$NO_TURBO_FILE" ]]; then
  echo 1 | sudo tee "$NO_TURBO_FILE" >/dev/null
fi

TS="$(date +%Y%m%d_%H%M%S)"
JSON="$OUT_DIR/bench_${TS}.json"
TXT="$OUT_DIR/bench_${TS}.txt"

# Pinned benchmark run with repetitions.
taskset -c "$CORE" "$BIN" \
  --benchmark_filter="$BENCH_REGEX" \
  --benchmark_repetitions="$REPS" \
  --benchmark_report_aggregates_only=false \
  --benchmark_min_time=0.2s \
  --benchmark_out="$JSON" \
  --benchmark_out_format=json | tee "$TXT"

# Compute 99% CI from iteration runs (real_time, ns).
readarray -t VALUES < <(jq -r '.benchmarks[] | select(.run_type=="iteration") | .real_time' "$JSON")
N="${#VALUES[@]}"
if [[ "$N" -lt 2 ]]; then
  echo "Not enough repetition samples for CI (need >= 2)." >&2
  exit 2
fi

printf "%s\n" "${VALUES[@]}" | awk '
  BEGIN { n=0; sum=0; z=2.576 }
  {
    x=$1+0;
    a[n]=x;
    sum+=x;
    n++;
  }
  END {
    mean=sum/n;
    ss=0;
    for (i=0;i<n;i++) {
      d=a[i]-mean;
      ss+=d*d;
    }
    sd=sqrt(ss/(n-1));
    sem=sd/sqrt(n);
    ci=z*sem;
    printf("\n=== 99%% CI (real_time, ns) ===\n");
    printf("samples: %d\n", n);
    printf("mean: %.3f ns\n", mean);
    printf("stddev: %.3f ns\n", sd);
    printf("99%% CI: [%.3f, %.3f] ns\n", mean-ci, mean+ci);
    printf("relative half-width: %.3f%%\n", (ci/mean)*100.0);
  }
'

echo "\nSaved:"
echo "  JSON: $JSON"
echo "  Text: $TXT"
