# Flux Prototype Specification

Version: 0.1 (Draft)  
Date: 2026-03-02

## 1. Purpose

Flux is a Data-Oriented DSL/JIT design that decouples **logic** from **physical memory layout**.  
A developer writes intent (schemas + streams + systems), and the runtime synthesizes:

- storage layout (AoS/SoA/hybrid)
- vectorization strategy (scalar/SIMD/SPMD)
- branch strategy (masked vs compacted)
- threading policy with false-sharing protection

Goal: approach hand-tuned systems performance while preserving high-level ergonomics.

---

## 2. Core Principles

1. **Layout Transparency**
   - Source-level schema does not lock storage layout.
   - Physical representation is chosen per workload/hot path.

2. **Stream-First Execution**
   - Dense component iteration is the default abstraction.
   - Linear access is prioritized for cache and prefetch behavior.

3. **Adaptive Branch Handling**
   - Runtime chooses masked SIMD or stream compaction based on profile data.

4. **Thread-Safe Locality**
   - Scheduler and allocator avoid false sharing by construction.

5. **Profile-Guided Layout/JIT Evolution**
   - Hot traces trigger re-layout and recompile with guards/deopt paths.

---

## 3. Language Model

### 3.1 Schemas (Logical Data Groups)

```flux
schema Physics {
    pos_x: f32,
    pos_y: f32,
    vel_x: f32,
    vel_y: f32,
}

schema Health {
    value: f32,
    is_alive: bool,
}
```

Semantics:
- Schemas define fields and types only.
- No inheritance/vtables in hot-path schema storage.
- Optional tags can hint constraints (alignment, mutability, transient/persistent).

### 3.2 Components and Entities

- `entity` is an opaque id.
- component instances are rows in schema storage.
- systems query by required schema set.

### 3.3 Streams and Systems

```flux
stream UpdatePhysics(p: Physics) {
    p.pos_x += p.vel_x;
    p.pos_y += p.vel_y;
}

stream ProcessDamage(h: Health, damage: f32) {
    if h.value > 0.0 {
        h.value -= damage;
    } else {
        h.is_alive = false;
    }
}
```

Semantics:
- stream body is side-effect-constrained and vectorizable by default.
- aliasing is disallowed unless explicitly marked.
- order is undefined unless `ordered` is requested.

---

## 4. Storage & Layout Synthesis

### 4.1 Candidate Physical Layouts

- **AoS**: creation-heavy, sparse access, polymorphic tooling compatibility.
- **SoA**: field-sweep kernels and SIMD-heavy loops.
- **AoSoA**: cache-tile/chunked vector blocks.

### 4.2 Layout Selection Heuristics

The runtime records per field and per system:
- access frequency
- co-access matrix
- stride and temporal locality
- branch selectivity
- write amplification

A periodic optimizer selects representation minimizing:

$$
Cost = \alpha \cdot MissPenalty + \beta \cdot BranchPenalty + \gamma \cdot CopyCost + \delta \cdot SyncCost
$$

### 4.3 JIT-Flip (Hot-Path Relayout)

- Hot loops can be migrated from AoS to SoA buffers.
- Buffer transitions are guarded by versioned metadata.
- Writes reconcile via dirty-range or double-buffer policy.

---

## 5. Execution Pipeline

1. Parse + type-check schemas/streams.
2. Build IR with alias, mutability, and side-effect annotations.
3. Profile warmup execution.
4. Select layout and kernel strategy.
5. Emit target-specific code (AVX2/AVX-512/NEON/SVE fallback scalar).
6. Install guards/deopt and telemetry hooks.

---

## 6. SIMD & Branch Strategy

### 6.1 SIMD Modes

- auto-vectorized scalar IR
- SPMD-lowered vector IR
- explicit vector width hints (optional)

### 6.2 Branch Handling Policy

For branch probability $p$ and per-path costs $C_t, C_f$:

- **Masked SIMD** preferred when branch bodies are light and lane utilization is high.
- **Compaction** preferred when one or both branches are expensive.

Decision threshold compares:

$$
C_{masked} \approx C_t + C_f
$$

vs

$$
C_{compact} \approx C_{classify} + C_{pack} + pC_t + (1-p)C_f
$$

### 6.3 Compaction Backends

- AVX-512: native compress/predicate
- AVX2: movemask + permute LUT + packed store
- scalar fallback for short tails

---

## 7. Threading & False-Sharing Guard

### 7.1 Work Partitioning

- chunk by contiguous ranges
- NUMA-aware placement (future)
- deterministic chunk sizing for benchmark mode

### 7.2 Cache-Line Isolation

- writable per-thread metadata aligned to 64B minimum
- thread-local accumulators merged post-kernel
- scheduler avoids adjacent writer slots sharing lines

### 7.3 Safety Rule

Any JIT-generated shared writable structure must pass:
- static alignment check
- runtime line-overlap assertion in debug mode

---

## 8. Runtime Introspection API

```flux
inspect layout Physics
inspect kernel UpdatePhysics
inspect profile ProcessDamage
```

Expected output includes:
- current layout (AoS/SoA/AoSoA)
- vector width and instruction family
- branch mode (masked/compacted)
- observed cache miss, branch miss, IPC telemetry

---

## 9. Deterministic Benchmark Mode

Flux runtime should expose:
- core pinning
- governor lock hooks (where supported)
- fixed seed scheduling
- repetition runs with confidence interval output

Minimum statistical report:
- mean, stddev, CV
- 95% and 99% confidence intervals
- relative speedup with interval separation indicator

---

## 10. Minimal “Hello World” Program

```flux
schema Physics { pos_x: f32, pos_y: f32, vel_x: f32, vel_y: f32 }
schema Health  { value: f32, is_alive: bool }

stream Integrate(p: Physics, dt: f32) {
  p.pos_x += p.vel_x * dt;
  p.pos_y += p.vel_y * dt;
}

stream Damage(h: Health, dmg: f32) {
  if h.value > 0.0 {
    h.value -= dmg;
  } else {
    h.is_alive = false;
  }
}

system Tick(dt: f32, dmg: f32) {
  run Integrate(dt);
  run Damage(dmg);
}
```

Expected runtime behavior:
- `Physics` selected as SoA in hot loops.
- `Damage` branch policy chosen from profile (masked or compacted).
- kernels emitted per available ISA and thread count.

---

## 11. Implementation Roadmap

### Milestone A: Interpreter + IR
- parser, schema registry, stream IR
- scalar executor baseline

### Milestone B: SoA backend + simple JIT
- SoA store manager
- vectorizable loop emission

### Milestone C: Profile-guided optimizer
- branch profiling
- masked vs compacted strategy switch

### Milestone D: Threading + false-sharing guard
- chunk scheduler
- padded writer buffers

### Milestone E: Diagnostics + benchmark mode
- layout/kernel/profile introspection
- CI statistics and run reproducibility tools

---

## 12. Non-Goals (v0.1)

- full GC-compacting object runtime
- unrestricted OO polymorphism in hot streams
- distributed memory execution

---

## 13. Success Criteria

Flux v0.1 is successful if it demonstrates:

1. automatic AoS→SoA relayout on hot streams,
2. consistent speedup over naive object layout,
3. adaptive branch strategy outperforming fixed policy,
4. no false-sharing regressions under multithread runs,
5. reproducible benchmark outputs with confidence intervals.

---

## 14. Notes

This spec intentionally prioritizes hardware-native behavior while preserving high-level authoring. It is designed as a research/engineering bridge between productivity-first languages and DOD-first performance models.
