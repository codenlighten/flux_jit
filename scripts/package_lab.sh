#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist}"
TS="$(date +%Y%m%d_%H%M%S)"
NAME="fluxjit_speed_lab_${TS}"
STAGE_DIR="$OUT_DIR/$NAME"
TAR_PATH="$OUT_DIR/${NAME}.tar.gz"

mkdir -p "$OUT_DIR"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -e "$src" ]]; then
    mkdir -p "$(dirname "$dst")"
    cp -R "$src" "$dst"
  fi
}

# Core docs/artifacts
copy_if_exists "$ROOT_DIR/README.md" "$STAGE_DIR/README.md"
copy_if_exists "$ROOT_DIR/RESULTS.md" "$STAGE_DIR/RESULTS.md"
copy_if_exists "$ROOT_DIR/STATUS.md" "$STAGE_DIR/STATUS.md"
copy_if_exists "$ROOT_DIR/PERFORMANCE_HIERARCHY.md" "$STAGE_DIR/PERFORMANCE_HIERARCHY.md"
copy_if_exists "$ROOT_DIR/FLUX_PROTOTYPE_SPEC.md" "$STAGE_DIR/FLUX_PROTOTYPE_SPEC.md"
copy_if_exists "$ROOT_DIR/FLUX_IR_SKETCH.md" "$STAGE_DIR/FLUX_IR_SKETCH.md"
copy_if_exists "$ROOT_DIR/PROJECT_ARCHIVE_EXEC_SUMMARY.md" "$STAGE_DIR/PROJECT_ARCHIVE_EXEC_SUMMARY.md"
copy_if_exists "$ROOT_DIR/MASTER_ARCHIVE_INDEX.md" "$STAGE_DIR/MASTER_ARCHIVE_INDEX.md"

# Project trees (exclude heavy build outputs and VCS metadata)
if [[ -d "$ROOT_DIR/cpp" ]]; then
  rsync -a --exclude build --exclude .git "$ROOT_DIR/cpp/" "$STAGE_DIR/cpp/"
fi
if [[ -d "$ROOT_DIR/rust" ]]; then
  rsync -a --exclude target --exclude .git "$ROOT_DIR/rust/" "$STAGE_DIR/rust/"
fi
if [[ -d "$ROOT_DIR/fluxjit" ]]; then
  rsync -a --exclude build --exclude .git "$ROOT_DIR/fluxjit/" "$STAGE_DIR/fluxjit/"
fi

# Include lightweight profiling artifacts if present
copy_if_exists "$ROOT_DIR/cpp/profiling" "$STAGE_DIR/cpp/profiling"
copy_if_exists "$ROOT_DIR/fluxjit/DAMAGE_STRATEGY_SWEEP.md" "$STAGE_DIR/fluxjit/DAMAGE_STRATEGY_SWEEP.md"
copy_if_exists "$ROOT_DIR/fluxjit/DAMAGE_STRATEGY_SWEEP_2D.md" "$STAGE_DIR/fluxjit/DAMAGE_STRATEGY_SWEEP_2D.md"

# Manifest
cat > "$STAGE_DIR/DEPLOYMENT_MANIFEST.txt" <<EOF
FluxJIT Speed Lab Deployment Bundle
Generated: $(date -Iseconds)
Host: $(hostname)
Root: $ROOT_DIR
Contents:
- C++ benchmarks and scripts
- Rust benchmarks
- FluxJIT prototype (C ABI + Python wrappers)
- Benchmark/profiling summaries and design artifacts
EOF

# Archive + checksum
(
  cd "$OUT_DIR"
  tar -czf "${NAME}.tar.gz" "$NAME"
  sha256sum "${NAME}.tar.gz" > "${NAME}.tar.gz.sha256"
)

rm -rf "$STAGE_DIR"

echo "Bundle created: $TAR_PATH"
echo "Checksum file: ${TAR_PATH}.sha256"
