# CPU Data-Oriented Benchmark Suite

This workspace now contains runnable benchmarks for the concepts we discussed:

- AoS vs SoA data layout
- Scalar vs vectorization-friendly iteration (`unseq` in C++)
- Manual AVX2 SIMD update loops
- Branchy SIMD masking patterns
- Stream-compaction crossover (compute-both vs partition-then-process)

## Workspace Layout

- `cpp/`
  - `CMakeLists.txt`
  - `bench_aos_soa_simd_compaction.cpp`
  - `bench_ecs_vs_oop.cpp`
- `rust/`
  - `Cargo.toml`
  - `src/lib.rs`
  - `benches/aos_soa_simd_compaction.rs`
  - `benches/ecs_vs_oop.rs`

## Run: C++ Benchmarks

Requirements:

- CMake 3.16+
- C++20 compiler
- Google Benchmark installed (system package or vcpkg/conan)

Build and run:

1. Configure and build in `cpp/`.
2. Run `cpu_benchmarks` for micro-optimizations (AoS/SoA/SIMD/compaction).
3. Run `ecs_benchmarks` for architectural patterns (ECS vs OOP).
4. (Optional) export JSON from Google Benchmark for charting.

Suggested flags for vectorization reports:

- Clang: `-Rpass=loop-vectorize`
- GCC: `-fopt-info-vec-optimized`

## Run: Rust Benchmarks

Requirements:

- Rust toolchain (`cargo`)

From `rust/`:

1. Run `cargo bench --bench aos_soa_simd_compaction` for micro-benchmarks.
2. Run `cargo bench --bench ecs_vs_oop` for architectural patterns.
3. Open the generated Criterion HTML report in `target/criterion/report/index.html`.

## Fair Benchmarking Checklist

- Disable turbo/dynamic scaling when possible.
- Pin to a core for repeatability.
- Keep background load low.
- Trust warmup-aware frameworks (Google Benchmark / Criterion).
- Use anti-DCE guards (`DoNotOptimize`, `black_box`).

## Notes

- AVX2 benchmark functions are compiled only when AVX2 is enabled (`__AVX2__`).
- The stream-compaction benchmark in Rust intentionally demonstrates algorithmic trade-offs and includes data movement overhead.
