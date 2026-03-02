# Data-Oriented Design: Performance Hierarchy

Based on empirical measurements from this workspace, here's the definitive optimization roadmap for modern CPU-bound applications.

## The DOD Performance Stack (Ranked by Impact)

```
┌─────────────────────────────────────────────────────────────────┐
│ L0: ARCHITECTURE                                    7-28× SPEEDUP│
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Traditional OOP (vtables, pointer chasing)                 │ │
│ │         ↓                                                    │ │
│ │  Entity Component System (SoA, linear iteration)            │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ L1: DATA LAYOUT                                    10-20× SPEEDUP│
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Array of Structures (AoS)                                  │ │
│ │         ↓                                                    │ │
│ │  Structure of Arrays (SoA)                                  │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ L2: AUTO-VECTORIZATION                              1.4-2× SPEEDUP│
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Scalar loops                                               │ │
│ │         ↓                                                    │ │
│ │  std::execution::unseq / LLVM auto-vectorize                │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ L3: ALGORITHMIC STRATEGY                               2× SPEEDUP│
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Masked SIMD (compute all branches)                         │ │
│ │         ↓                                                    │ │
│ │  Stream Compaction (partition then process)                 │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ L4: MANUAL INTRINSICS                             1.1-1.5× SPEEDUP│
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Compiler auto-vectorization                                │ │
│ │         ↓                                                    │ │
│ │  AVX2/AVX-512 manual intrinsics + LUTs                      │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## Decision Tree: Where to Invest Time

### Start Here: Architecture (L0)

**Question**: Does your codebase use inheritance-heavy OOP with virtual dispatch?

- **YES** → Migrate to ECS/component-based design.  
  **Impact**: 7-28× speedup (largest win available)
- **NO** → Already cache-friendly? Move to L1.

### Next: Data Layout (L1)

**Question**: Are your hot-path structs accessed field-by-field in tight loops?

- **YES** → Refactor to Structure of Arrays (SoA).  
  **Impact**: 10-20× speedup
- **NO** → Already using SoA? Move to L2.

### Then: Auto-Vectorization (L2)

**Question**: Are your loops simple arithmetic with no pointer aliasing?

- **YES** → Add `std::execution::unseq` (C++) or rely on LLVM (Rust).  
  **Impact**: 1.4-2× speedup (free performance)
- **NO** → Branchy or complex loops? Move to L3.

### Branch Handling: Algorithm (L3)

**Question**: Do you have expensive conditional logic in hot loops?

- **YES** → Use stream compaction (partition → process).  
  **Impact**: 2× speedup over masked SIMD
- **NO** → Simple branches? Try masked SIMD first.

### Last Resort: Manual Metal (L4)

**Question**: Is this loop THE critical path in profiling results?

- **YES** → Write AVX2/AVX-512 intrinsics with permutation LUTs.  
  **Impact**: 1.1-1.5× final squeeze (high maintenance cost)
- **NO** → Stop optimizing; you're done.

---

## Key Principles (Empirically Validated)

1. **Memory beats computation**: A bad data layout with clever algorithms loses to good layout with dumb loops.
2. **Architecture > micro-optimizations**: ECS (7-28×) >> manual SIMD (1.1-1.5×).
3. **Cache misses are the enemy**: Every pointer chase is 100+ cycles of stall time.
4. **Let the compiler help**: Auto-vectorization on SoA data often matches hand-written SIMD.
5. **Profile before optimizing L4**: Manual intrinsics are fragile and architecture-specific.

---

## Real-World Speedup Stack Example

Starting with traditional OOP game engine (baseline 100 ms/frame):

1. **Migrate to ECS** → 100 ms → **12.5 ms** (8× win, L0)
2. **Already SoA in ECS** → (included in L0 win)
3. **Enable auto-vectorization** → 12.5 ms → **7.8 ms** (1.6× win, L2)
4. **Partition expensive AI logic** → 7.8 ms → **3.9 ms** (2× win, L3)
5. **AVX2 on physics kernel** → 3.9 ms → **3.2 ms** (1.2× win, L4)

**Total improvement**: 100 ms → 3.2 ms = **31× overall speedup**

Most gains came from L0 (architecture) and L2 (free vectorization after SoA layout).

---

## Further Reading

- **ECS Libraries**:
  - C++: EnTT, flecs  
  - Rust: bevy_ecs, hecs, specs
- **SIMD Documentation**:
  - Intel Intrinsics Guide
  - Compiler Explorer (godbolt.org) for assembly verification
- **Profiling Tools**:
  - `perf stat -e cache-misses,cache-references`
  - VTune, Tracy, Optick

---

**See `RESULTS.md` for detailed benchmark data supporting these recommendations.**
