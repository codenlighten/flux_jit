# Phase 2 Roadmap: The Polyglot Bridge

## Mission
Transition FluxJit from a proven research lab into a stable, multi-language dependency with portable performance.

## North Star
**One IR, multiple hardware profiles, identical correctness, near-optimal strategy selection per target.**

---

## Sprint 1 (Weeks 1-4): Stability + Bridge

### Week 1 — Versioned Contract (In Progress)
- Define and publish stable Flux IR C-ABI contract (`flux_ir_v1.h`).
- Freeze naming/versioning/error semantics for language bindings.
- Add compatibility policy: `major` breaks ABI, `minor` additive, `patch` bugfix.

### Week 2 — Polyglot ABI Validation
- Add minimal binding sanity harnesses:
  - C++ host → Flux IR envelope compile/run
  - Rust host (FFI) → same envelope compile/run
- Validate memory ownership and error-string lifecycle across boundaries.

### Week 3 — Profile Store (Cold-start Elimination)
- Persist hardware fingerprints and strategy thresholds.
- Store format: JSON (v1), keyed by CPU model + feature flags.
- Add cache invalidation rules on ABI/version mismatch.

### Week 4 — Demonstration Gate
- Demo objective:
  - Single IR workload
  - Two hardware profiles (or emulated profile selections)
  - Same numeric correctness
  - Different chosen strategies where expected
- Deliverable: reproducible script + report artifact.

---

## Deliverables
- `fluxjit/include/flux_ir_v1.h` (stable ABI contract)
- Binding tests (C++ + Rust)
- Profile cache module and schema docs
- Demo report proving one-IR/multi-profile execution

## Technical Decisions
- **Wire format v1:** JSON payload envelope (fastest path to adoption)
- **Future wire format:** FlatBuffers/Protobuf once schema stabilizes
- **Policy persistence:** Local JSON cache first, SQLite optional in v2
- **Backend scope:** CPU-first; GPU backend evaluation begins post Sprint 1 gate

---

## Risks and Mitigations
- **FFI contract drift**
  - Mitigation: ABI version macros + conformance tests per binding.
- **Policy instability across machines**
  - Mitigation: profile cache keys include hardware fingerprint + build metadata.
- **Benchmark-vs-production mismatch**
  - Mitigation: include mixed workload demo and correctness checks in gate criteria.

## Success Criteria
- Language bindings can submit the same IR envelope without special cases.
- Runtime selects strategy based on persisted or measured profile data.
- Correctness outputs match across profile variations.
- Reproducible artifacts produced by script, not manual steps.

---

## Immediate Next Actions
1. Add API functions to `fluxjit.h` for envelope-based compile entrypoint.
2. Implement envelope parser shim in `fluxjit/src/fluxjit.cpp`.
3. Add Rust FFI smoke harness for the new contract.
4. Integrate profile-cache load/save in `auto_policy.py`.
