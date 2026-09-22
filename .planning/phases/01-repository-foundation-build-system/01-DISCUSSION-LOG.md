# Phase 1: Repository Foundation & Build System - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in 01-CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-22
**Phase:** 1-Repository Foundation & Build System
**Areas discussed:** Source identity & naming, Repo setup strategy, CI scope for Phase 1, Character path model

---

## Source Identity & Naming

### Source Type ID

| Option | Description | Selected |
|--------|-------------|----------|
| `animated_avatar_source` | Descriptive, namespaced to the feature. Standard OBS convention. | ✓ |
| `obs_avatar_source` | Prefixes with `obs_` to match binary name. | |
| `obs_animated_avatar` | No `_source` suffix — breaks OBS noun-phrase convention. | |

**User's choice:** `animated_avatar_source`
**Notes:** Permanent decision — embedded in OBS scene JSON files.

### Display Name

| Option | Description | Selected |
|--------|-------------|----------|
| `Animated Avatar` | Clean, intuitive. Matches roadmap description. | ✓ |
| `OBS Avatar` | Shorter, may be confused with official OBS feature. | |
| `Avatar (2D)` | Very short; `(2D)` adds noise. | |

**User's choice:** `Animated Avatar`
**Notes:** Localizable string; reversible unlike the source type ID.

---

## Repo Setup Strategy

### Template Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| GitHub "Use as template" | Fresh repo, clean initial commit, no template history. Standard path. | ✓ |
| Fork the template repo | Preserves template history; enables upstream pull. Most plugin devs don't need this. | |
| Clone + delete .git + reinit | Equivalent to template button, done manually. No real benefit. | |

**User's choice:** GitHub "Use as template"

### Repository Name

| Option | Description | Selected |
|--------|-------------|----------|
| `obs-animated-avatar` | Follows `obs-` prefix convention, matches DLL output name. | ✓ |
| `obs-avatar` | Shorter but less descriptive. | |
| `animated-avatar-obs-plugin` | Verbose, doesn't follow `obs-` prefix convention. | |

**User's choice:** `obs-animated-avatar`

---

## CI Scope for Phase 1

### CI Ambition Level

| Option | Description | Selected |
|--------|-------------|----------|
| Build check only | Windows x64: configure + build + verify DLL exists. Fast. | ✓ |
| Build + CPack artifact | Wire up packaging from day 1; reduces Phase 8 work. | |
| Build + artifact + lint/format | Adds clang-format enforcement; too early for Phase 1. | |

**User's choice:** Build check only
**Notes:** Packaging and test runner deferred to Phase 8.

### CI Target Matrix

| Option | Description | Selected |
|--------|-------------|----------|
| Windows x64 only | Matches MVP target. OBS 29+ dropped 32-bit. | ✓ |
| Windows x64 + x86 | OBS 29+ dropped 32-bit — no benefit. | |
| Windows x64 + arm64 | OBS arm64 is experimental; add when ecosystem validates it. | |

**User's choice:** Windows x64 only

---

## Character Path Model

### Runtime Character Directory

| Option | Description | Selected |
|--------|-------------|----------|
| `obs-data/obs-plugins/obs-animated-avatar/characters/` | OBS standard plugin data directory. `obs_get_module_data_path()`. | ✓ |
| Next to the DLL | `obs-plugins/64bit/` is not user-writable on standard installs. | |
| Configurable path in OBS settings | Maximum flexibility but adds UI complexity; standard path can be made configurable later. | |

**User's choice:** `obs_get_module_data_path()`-based path
**Notes:** One-way decision — changing path post-distribution requires user migration.

### Default Character Bundle

| Option | Description | Selected |
|--------|-------------|----------|
| Bundle minimal placeholder (Recommended) | Solid-color PNG stubs. Proves asset pipeline path in Phase 1. | ✓ |
| No characters until Phase 3 | Phase 1 only proves build chain; characters are Phase 3 scope. | |
| Bundle real default character | Art production blocks code progress in Phase 1. | |

**User's choice:** Bundle minimal placeholder (solid-color PNG stubs)

---

## Claude's Discretion

- Exact placeholder render implementation (1×1 texture vs inline GS effect vs `gs_draw_sprite`) — planner chooses simplest approach that proves graphics context works
- Exact CMake `install()` layout — follow obs-plugintemplate defaults
- `blog()` wrapper prefix string: `[obs-animated-avatar]`

## Deferred Ideas

- Windows arm64 CI — deferred until OBS arm64 is mainstream
- CPack artifact generation in CI — deferred to Phase 8
- clang-format CI enforcement — deferred to Phase 8
- Configurable character path override — deferred to Phase 7 if users request it
- Linux/macOS CI targets — deferred until platform/ abstraction layer (Phase 6+)
