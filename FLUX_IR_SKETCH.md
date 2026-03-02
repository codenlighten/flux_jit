# Flux IR Sketch (Data-First, Layout-Aware)

Version: 0.1 Draft  
Date: 2026-03-02

## 1) Design Goal

Flux IR is not just syntax-lowered AST; it is a **data-flow + layout-intent IR** designed to make these backend decisions trivial:

- AoS vs SoA vs AoSoA storage
- scalar vs SIMD/SPMD lowering
- masked branch vs stream compaction
- thread partitioning + false-sharing guard

The IR must explicitly model **field hotness**, **access stride**, and **alias constraints**.

---

## 2) IR Layers

### Layer A — HIR (Semantic IR)
Human-readable, typed, side-effect constrained.

- operations on logical schemas/fields
- explicit stream boundaries
- no fixed physical layout yet

### Layer B — DFIR (Data-Flow IR)
SSA-like graph with memory streams and profile metadata.

- nodes for field loads/stores by component column
- branch profile edges
- vectorizability and alignment facts

### Layer C — KIR (Kernel IR)
Backend-ready kernel form.

- chosen physical layout
- chosen branch strategy (masked/compacted)
- lane width and ISA capability assumptions

---

## 3) Core IR Entities

## 3.1 Type and Layout Handles

```text
TypeId      := logical schema identity (Physics, Health)
FieldId     := typed field (Physics.pos_x : f32)
LayoutId    := {AoS | SoA | AoSoA(tile=N)}
StoreId     := physical storage instance versioned by epoch
```

## 3.2 Stream Region

```text
StreamRegion {
  name: Symbol
  params: [Value]
  reads: [FieldId]
  writes: [FieldId]
  alias_set: NoAlias | RestrictedAlias
  order: Unordered | Ordered
  profile: ProfileSummary
}
```

## 3.3 ProfileSummary

```text
ProfileSummary {
  iterations: u64
  branch_prob: map<BranchId, f64>
  avg_trip_count: f64
  l1_miss_rate: f64?
  ll_miss_rate: f64?
  vector_width_observed: u32?
}
```

---

## 4) Node Set (DFIR)

Minimal node vocabulary:

```text
Range(start, end, step)
LoadField(stream, FieldId, idx)
StoreField(stream, FieldId, idx, value)
Broadcast(const)
FAdd(a,b) | FSub(a,b) | FMul(a,b) | FMA(a,b,c)
CmpGT(a,b) | CmpLT(a,b)
Select(mask, on_true, on_false)
MaskFromCmp(cmp)
Compact(mask, values)          // logical compact op
Gather(base, indices)          // fallback
Scatter(base, indices, values) // fallback
Reduce(op, values)
Fence/Barrier
```

All value-producing nodes are SSA values.

---

## 5) Metadata Required for DOD Decisions

Each memory node can carry:

```text
MemoryMeta {
  contiguous: bool
  stride_bytes: u32
  alignment: u32
  expected_lane_utilization: f64
  hotness_score: f64
  write_shared_across_threads: bool
}
```

Each branch node can carry:

```text
BranchMeta {
  p_true: f64
  cost_true: f64
  cost_false: f64
  recommended: Masked | Compacted
}
```

---

## 6) Worked Lowering: Physics + Health

Source (Flux):

```flux
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
```

## 6.1 HIR (logical)

```text
Stream Integrate:
  for e in Physics:
    pos_x[e] = pos_x[e] + vel_x[e] * dt
    pos_y[e] = pos_y[e] + vel_y[e] * dt

Stream Damage:
  for e in Health:
    if value[e] > 0:
      value[e] = value[e] - dmg
    else:
      is_alive[e] = false
```

## 6.2 DFIR (data-flow sketch)

```text
r0      = Range(0, N, 1)
vd      = Broadcast(dt)

px      = LoadField(PhysicsStream, Physics.pos_x, r0)
vx      = LoadField(PhysicsStream, Physics.vel_x, r0)
py      = LoadField(PhysicsStream, Physics.pos_y, r0)
vy      = LoadField(PhysicsStream, Physics.vel_y, r0)

nx      = FMA(vx, vd, px)
ny      = FMA(vy, vd, py)

StoreField(PhysicsStream, Physics.pos_x, r0, nx)
StoreField(PhysicsStream, Physics.pos_y, r0, ny)

vh      = LoadField(HealthStream, Health.value, r0)
zero    = Broadcast(0.0)
mask    = MaskFromCmp(CmpGT(vh, zero))
subd    = FSub(vh, Broadcast(dmg))

# Option A: masked
new_h   = Select(mask, subd, vh)
StoreField(HealthStream, Health.value, r0, new_h)
StoreField(HealthStream, Health.is_alive, r0, Select(mask, true, false))

# Option B: compacted (alternative plan)
idx_t   = Compact(mask, r0)
val_t   = Gather(Health.value, idx_t)
val_t2  = FSub(val_t, Broadcast(dmg))
Scatter(Health.value, idx_t, val_t2)
```

---

## 7) Layout Planning Pass

Given DFIR, planner computes per stream:

```text
PlanResult {
  chosen_layout: SoA
  chosen_kernel: SIMD(width=8, isa=AVX2)
  branch_policy[Damage.if]: Masked or Compacted
  thread_chunk: 64K entities/chunk
  writer_padding: 64B  // if shared counters/accumulators exist
}
```

Selection rule (high-level):

$$
choose\_compaction\;if\;C_{classify}+C_{pack}+pC_t+(1-p)C_f < C_t + C_f
$$

Else use masked vector path.

---

## 8) False-Sharing Guard in IR

DFIR carries thread-write sets:

```text
ThreadWriteSet {
  stream: Symbol
  target: FieldId | RuntimeCounter
  shared: bool
  line_bytes: 64
}
```

If `shared == true`, backend must generate:
- per-thread local buffer aligned to 64B, or
- segmented writes by non-overlapping cache lines.

---

## 9) Deopt & Guard Model

Generated kernel has guard predicates:

```text
Guard {
  layout_epoch == expected
  isa_support >= AVX2
  alignment_ok == true
  alias_contract_holds == true
}
```

On guard failure:
- fallback to scalar kernel
- schedule replan if failures exceed threshold

---

## 10) Minimal JSON-like IR Encoding Example

```json
{
  "stream": "Integrate",
  "layout": "SoA",
  "range": {"start": 0, "end": "N", "step": 1},
  "nodes": [
    {"id": "px", "op": "LoadField", "field": "Physics.pos_x"},
    {"id": "vx", "op": "LoadField", "field": "Physics.vel_x"},
    {"id": "dt", "op": "Broadcast", "value": "param:dt"},
    {"id": "nx", "op": "FMA", "args": ["vx", "dt", "px"]},
    {"op": "StoreField", "field": "Physics.pos_x", "value": "nx"}
  ],
  "meta": {
    "vectorizable": true,
    "alignment": 32,
    "alias_set": "NoAlias",
    "hotness": 0.97
  }
}
```

---

## 11) Validation Checklist for Backend

For each compiled stream, emit diagnostics:

- chosen layout + rationale
- vector width and ISA used
- branch strategy and branch profile
- estimated vs observed lane utilization
- cache miss and IPC summary (if runtime counters enabled)

A stream is considered healthy when:
- layout chosen is stable across epochs,
- guard failures are rare,
- CI-bench variance remains within configured threshold.

---

## 12) Next Step

Implement a tiny end-to-end prototype:

1. Parse schemas + one stream.
2. Build HIR then DFIR with node set above.
3. Hardcode SoA backend for `Integrate`.
4. Add branch policy toggle (`masked` vs `compacted`) for `Damage`.
5. Validate with existing clean-room benchmark harness.
