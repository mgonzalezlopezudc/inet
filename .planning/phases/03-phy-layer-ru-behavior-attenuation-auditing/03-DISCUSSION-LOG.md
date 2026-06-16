# Phase 3: PHY Layer RU Behavior & Attenuation Auditing - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md - this log preserves the alternatives considered.

**Date:** 2026-06-17
**Phase:** 03-phy-layer-ru-behavior-attenuation-auditing
**Areas discussed:** RU Frequency Mapping, Per-RU Power Split, Noise & Interference Isolation, Allocation Validation & Failure Policy

---

## RU Frequency Mapping

| Option | Description | Selected |
|--------|-------------|----------|
| Use scheduler-provided RU geometry as authoritative | Lock to scheduler RU descriptor and avoid medium-side geometry drift. | ✓ |
| Recompute RU geometry in medium from count + channel | Keep medium as reconstruction authority from channel and RU count. | |
| Hybrid check: scheduler mapping + medium recompute assertion | Keep both and assert consistency. | |

**User's choice:** Use scheduler-provided RU geometry as authoritative; enforce fail-fast for invalid/mismatched mappings; keep both tag and PHY-header allocation metadata.
**Notes:** User locked strict correctness policy with no tolerant path for mapping mismatches.

---

## Per-RU Power Split

| Option | Description | Selected |
|--------|-------------|----------|
| Proportional to RU bandwidth | Split transmit power using RU bandwidth fraction. | ✓ |
| Equal power per active RU | Uniform split across active allocations regardless of RU width. | |
| Configurable strategy via parameter | Runtime-selectable strategy with explicit default. | |

**User's choice:** Bandwidth-proportional split with nominal-channel denominator and explicit power-conservation auditing.
**Notes:** For future irregular RU geometry, policy remains bandwidth-based.

---

## Noise & Interference Isolation

| Option | Description | Selected |
|--------|-------------|----------|
| Same-MU sub-transmissions non-interfering | Preserve RU orthogonality within one MU PPDU. | ✓ |
| Model partial leakage between adjacent RUs | Introduce coupling into baseline behavior. | |
| Make coupling configurable for experiments | Keep optional advanced behavior, not default. | |

**User's choice:** Preserve non-interfering RU default, suppress physical effects of main MU transmission, add explicit per-RU audit observability.
**Notes:** Coupled/leakage behavior is deferred to future optional work.

---

## Allocation Validation & Failure Policy

| Option | Description | Selected |
|--------|-------------|----------|
| Abort MU transmission (fail-fast) | Stop on allocation validation failure. | ✓ |
| Drop invalid allocations and continue | Best-effort partial MU transmission. | |
| Fallback to single-user retries | Convert failure path into alternate flow. | |

**User's choice:** Fail-fast policy with `cRuntimeError` for validation and ownership errors; no tolerant fallback in Phase 3.
**Notes:** Deterministic, audit-grade behavior prioritized over graceful continuation.

---

## the agent's Discretion

None.

## Deferred Ideas

- Optional configurable non-fatal fallback mode for validation errors in future phases.
- Optional inter-RU coupling/leakage model for future experimental mode.
