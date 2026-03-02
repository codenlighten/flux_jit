#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"

TARBALL_PATH="${1:-}"
if [[ -z "$TARBALL_PATH" ]]; then
  TARBALL_PATH="$(ls -1t "$DIST_DIR"/fluxjit_speed_lab_*.tar.gz 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$TARBALL_PATH" || ! -f "$TARBALL_PATH" ]]; then
  echo "Could not find deployment tarball. Provide path as first argument." >&2
  exit 1
fi

CHECKSUM_PATH="${TARBALL_PATH}.sha256"

OUT_PARENT="${2:-/tmp}"
TS="$(date +%Y%m%d_%H%M%S)"
WORK_DIR="$OUT_PARENT/fluxjit_verify_$TS"
mkdir -p "$WORK_DIR"

echo "[1/5] Using tarball: $TARBALL_PATH"

if [[ -f "$CHECKSUM_PATH" ]]; then
  echo "[2/5] Verifying checksum: $CHECKSUM_PATH"
  (cd "$(dirname "$TARBALL_PATH")" && sha256sum -c "$(basename "$CHECKSUM_PATH")")
else
  echo "[2/5] Checksum file not found, skipping integrity check"
fi

echo "[3/5] Extracting bundle to: $WORK_DIR"
tar -xzf "$TARBALL_PATH" -C "$WORK_DIR"

BUNDLE_ROOT="$(find "$WORK_DIR" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [[ -z "$BUNDLE_ROOT" ]]; then
  echo "Failed to locate extracted bundle root" >&2
  exit 2
fi

echo "[4/5] Building FluxJIT in extracted bundle"
cmake -S "$BUNDLE_ROOT/fluxjit" -B "$BUNDLE_ROOT/fluxjit/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUNDLE_ROOT/fluxjit/build" -j >/dev/null

echo "[5/5] Running smoke tests"
(
  cd "$BUNDLE_ROOT/fluxjit/python"
  python3 test_smoke.py
  python3 autoscale.py --alive-ratio 0.2 --work-factor 10 --n 100000 --warmup 2 --iters 4 >/dev/null
)

echo ""
echo "Verification complete: PASS"
echo "Extracted bundle path: $BUNDLE_ROOT"
