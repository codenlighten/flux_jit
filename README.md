# 🚀 FluxJit: Data-Oriented Performance Laboratory

`FluxJit` is a reproducible systems-performance lab that demonstrates how Data-Oriented Design (DOD) outperforms object-centric layouts on modern CPUs.

This repository combines:

1. **Investigation** (C++ and Rust benchmarks)
2. **Theory** (Flux prototype spec + IR sketch)
3. **Implementation** (strategy-aware C++ runtime + Python bridge)
4. **Intelligence** (auto-tuning and self-healing policy switching)
5. **Distribution** (packaging and smoke-test verification)

---

## 🔬 What This Repo Proves

From measured runs in `RESULTS.md`:

- **SoA vs AoS:** up to **~19×** faster iteration from cache-friendly layout.
- **ECS vs OOP:** **~7–8×** in mixed workloads, and up to **~28×** in pointer-chasing worst cases.
- **Compaction vs Masking:** partition-then-process is **~2×** faster once branch work is non-trivial.
- **SIMD:** AVX2 provides additional gains for suitable kernels after layout is fixed.

The key lesson: architecture and memory layout dominate micro-optimizations.

---

## 🧭 Repository Map

- `cpp/` — Google Benchmark suites (AoS/SoA/SIMD/compaction + ECS vs OOP + false sharing)
- `rust/` — Criterion benchmark mirrors for cross-language validation
- `fluxjit/` — C-ABI runtime, strategy kernels, sweep reports, and Python wrapper
- `scripts/` — packaging and unpack/smoke verification scripts
- `RESULTS.md` — consolidated benchmark results
- `FLUX_PROTOTYPE_SPEC.md` — prototype design specification
- `FLUX_IR_SKETCH.md` — IR and lowering sketch
- `PERFORMANCE_HIERARCHY.md` — optimization priority model
- `PROJECT_ARCHIVE_EXEC_SUMMARY.md` and `STATUS.md` — executive/project tracking artifacts
- `PROJECT_COMPLETION.txt` — milestone freeze certificate
- `PHASE2_ROADMAP.md` — productization plan and sprint gates
- `SHA256_MANIFEST.txt` — integrity checksum manifest for the current workspace

---

## ⚡ Quick Start

### Reproduce C++ Benchmarks

```bash
cd cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/cpu_benchmarks
./build/ecs_benchmarks
./build/false_sharing_benchmarks
```

### Reproduce Rust Benchmarks

```bash
cd rust
cargo bench --bench aos_soa_simd_compaction
cargo bench --bench ecs_vs_oop
```

### Build and Smoke-Test FluxJit

```bash
cd fluxjit
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
python3 python/test_smoke.py
```

---

## 🧠 Adaptive Runtime (FluxJit)

The `Damage` kernel supports multiple execution strategies:

- `masked`
- `compacted`

Runtime tooling includes:

- `fluxjit/python/autoscale.py` — controlled strategy sweeps
- `fluxjit/python/auto_policy.py` — threshold + hysteresis policy selection and hot-swapping
- `--policy-from-sweep` mode — derive runtime policy from prior sweep markdown

See:

- `fluxjit/DAMAGE_STRATEGY_SWEEP.md`
- `fluxjit/DAMAGE_STRATEGY_SWEEP_2D.md`

---

## 📦 Portability and Distribution

Package and validate the full lab:

```bash
./scripts/package_lab.sh
./scripts/unpack_and_smoke.sh dist/fluxjit_speed_lab_*.tar.gz
```

This supports clean handoff to stakeholders and reproducible verification on other hardware.

---

## ✅ Benchmarking Hygiene

- Keep CPU frequency behavior consistent when possible.
- Minimize background load.
- Use warmup-aware frameworks (Google Benchmark / Criterion).
- Use anti-DCE guards (`DoNotOptimize`, `black_box`).
- Prefer repeated runs and confidence intervals over single-shot conclusions.

---

## 📌 Project Status

**Status:** Research-complete baseline frozen; Phase 2 (Polyglot Bridge) active.

If you’re new here, start with `RESULTS.md` for measured outcomes, then `fluxjit/python/test_smoke.py` for an end-to-end runtime sanity check.

---

## 👤 Author

**Gregory Ward**  
CTO, **SmartLedger Technology**  
https://smartledger.technology
