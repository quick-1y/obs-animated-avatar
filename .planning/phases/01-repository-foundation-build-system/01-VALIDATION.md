---
phase: "01"
slug: "repository-foundation-build-system"
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-09-22"
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | {cmake --build / manual OBS load test} |
| **Config file** | {none — build-system phase, no test framework yet} |
| **Quick run command** | `cmake --build --config RelWithDebInfo` |
| **Full suite command** | `cmake --preset windows-x64 && cmake --build --config RelWithDebInfo` |
| **Estimated runtime** | ~5 minutes |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build --config RelWithDebInfo`
- **After every plan wave:** Run full configure + build suite
- **Before `/gsd-verify-work`:** Full suite must be green + OBS manual load test passed
- **Max feedback latency:** ~300 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| {N}-01-01 | 01 | 1 | OBS-01 | — | N/A | build | `cmake --build --config RelWithDebInfo` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] CMake build system configured and producing `obs-animated-avatar.dll`
- [ ] OBS plugin loads without crash (manual verification)

*Planner fills this section with concrete test stubs and Wave 0 tasks.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| OBS loads plugin | OBS-06 | Requires OBS installed + running | Copy DLL to OBS plugins dir; launch OBS; check log for `[obs-animated-avatar]` prefix |
| Source renders purple rectangle | OBS-01 | Requires OBS visual inspection | Add "Animated Avatar" source in OBS; confirm 320×240 purple rectangle visible |
| Source add/delete without crash | OBS-06 | Requires OBS running | Add and delete source 3 times; confirm no crash |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
