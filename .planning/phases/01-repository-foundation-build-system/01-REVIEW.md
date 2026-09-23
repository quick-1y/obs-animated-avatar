---
phase: 01-repository-foundation-build-system
reviewed: 2026-09-23T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - src/avatar-source.cpp
  - src/plugin-main.cpp
  - CMakeLists.txt
  - buildspec.json
  - data/locale/en-US.ini
  - data/characters/default/README.md
  - README.md
  - .gitignore
  - .github/workflows/build-project.yaml
  - deps/.clang-format
findings:
  critical: 0
  warning: 2
  info: 4
  total: 6
status: issues_found
---

# Phase 01: Code Review Report

**Reviewed:** 2026-09-23T00:00:00Z
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

This phase adds the plugin scaffold: `avatar_source_info` registration with a 1x1 placeholder GPU texture, the module entry points, and build/CI plumbing (CMakeLists, buildspec.json, CI workflow scoped to Windows-only, locale + data placeholders).

I cross-checked the `obs_source_info` designated-initializer field order against the actual downloaded header (`.deps/include/obs-source.h` for OBS 31.1.1) — the order is correct and will compile under C++20's ordered-designator rule. `gs_texture_create`/`gs_texture_destroy`/`gs_draw_sprite` usage, graphics-lock discipline (`obs_enter_graphics()`/`obs_leave_graphics()` around every `gs_*` call in `create`/`destroy`, none outside those pairs or the render callback), the `Draw` technique name, and the straight-alpha texture layout were all verified against the actual OBS 31.1.1 source/shader tree and are correct. No `windows.h` usage, no `eval`/`exec`/hardcoded secrets, and `obs_register_source` (not `_s`) is used as required.

No Critical/blocker-level defects were found — this is a thin, well-guarded Phase 1 scaffold and the graphics-thread discipline the project mandates is followed correctly. The findings below are two real Warnings (a licensing/consistency gap and a silent-failure path with no diagnostics) plus four lower-value Info items for maintainability and doc consistency.

## Warnings

### WR-01: Inconsistent/incomplete GPL license headers

**File:** `src/avatar-source.cpp` (no header at all) and `src/plugin-main.cpp:1-17`
**Issue:** `LICENSE` is GPL-2.0, and `plugin-main.cpp` carries the standard GPL header comment — but it is still the unfilled template placeholder (`Copyright (C) <Year> <Developer> <Email Address>`), even though `buildspec.json` was updated this phase with real project metadata (`"author": "quick-1y"`, `"email": "l0nglife3025@gmail.com"`). `avatar-source.cpp`, added in this same phase, has no license header comment whatsoever. Per-file GPL notices are expected for compliance, and the inconsistency (one file with a stale placeholder, one file with none) will only get worse as more `.cpp` files are added following this precedent.
**Fix:**
```cpp
/*
Animated Avatar Plugin for OBS
Copyright (C) 2026 quick-1y <l0nglife3025@gmail.com>

This program is free software; ...
*/
```
Apply the same filled-in header to both `plugin-main.cpp` and `avatar-source.cpp` (and any future `.cpp`/`.h` added), rather than leaving the template placeholder or omitting it.

### WR-02: Silent failure path when placeholder texture creation fails

**File:** `src/avatar-source.cpp:16-30` and `:54-58`
**Issue:** `avatar_create` does not check the return value of `gs_texture_create()`. If it returns `nullptr` (e.g. device lost, GPU allocation failure), `avatar_create` still logs `"[init] avatar source created"` and returns a context that looks fully initialized. `avatar_render` does guard against the null texture (`if (!ctx->placeholder_tex) return;`), so there's no crash — but the source will render nothing, forever, with zero diagnostic trail: the log claims success and nothing downstream ever reports the failure. This will be very hard to diagnose from a user bug report ("the avatar source is just black/empty").
**Fix:**
```cpp
obs_enter_graphics();
uint8_t purple[4] = {128, 0, 128, 255};
const uint8_t *ptr = purple;
ctx->placeholder_tex = gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0);
obs_leave_graphics();

if (!ctx->placeholder_tex) {
	obs_log(LOG_ERROR, "[init] failed to create placeholder texture");
} else {
	obs_log(LOG_INFO, "[init] avatar source created");
}
```

## Info

### IN-01: `register_avatar_source()` forward-declared ad hoc instead of via a shared header

**File:** `src/plugin-main.cpp:22-23`
**Issue:** `void register_avatar_source();` is forward-declared directly in `plugin-main.cpp` rather than in a dedicated `avatar-source.h`. This works for a two-file project but doesn't scale — as more source registration functions are added in later phases (per the roadmap, more sources/filters are expected), each will need its own ad hoc forward declaration scattered through `plugin-main.cpp`, with no single header documenting the module's public surface.
**Fix:** Introduce `src/avatar-source.h` declaring `void register_avatar_source();`, include it from both `avatar-source.cpp` and `plugin-main.cpp`, and add it to `target_sources`/the include search path.

### IN-02: `ENABLE_QT`/Qt6 scaffold retained in CMakeLists.txt despite project's explicit no-Qt constraint

**File:** `CMakeLists.txt:8,27-38`
**Issue:** CLAUDE.md's tech stack section is explicit: "Qt — PROJECT.md explicitly excludes it." The template's `option(ENABLE_QT ...)` and the whole `if(ENABLE_QT)` block (Qt6 `find_package`, AUTOMOC/AUTOUIC/AUTORCC target properties) are still present. It defaults OFF so it's inert today, but it's dead weight that misrepresents the project's actual dependency footprint to anyone reading `CMakeLists.txt` and could get silently flipped on by a future contributor unaware of the constraint.
**Fix:** Remove the `ENABLE_QT` option and its `if(ENABLE_QT)` block entirely, since the project's tech stack doc states Qt is never used.

### IN-03: Windows SDK minimum version is inconsistent between README.md and CLAUDE.md

**File:** `README.md:11`
**Issue:** `README.md` states "Windows SDK 10.0.22621 or later" as a build requirement, while the project's own tech-stack reference (`.claude/CLAUDE.md`) states "Windows SDK 10.0.20348.0 or later (Windows 11 SDK preferred)". These are two different minimum-version claims for the same requirement in two docs a contributor is likely to read.
**Fix:** Reconcile the two documents to state a single minimum Windows SDK version (and note if 10.0.22621 was found to be actually required in practice, update CLAUDE.md to match, or vice versa).

### IN-04: ~180 lines of permanently-disabled macOS/Ubuntu CI job definitions retained in the workflow

**File:** `.github/workflows/build-project.yaml:74-254`
**Issue:** The `macos-build` and `ubuntu-build` jobs are disabled via `if: false` (correctly — this does skip them without consuming runner time), but the full job bodies remain, including references to composite actions (`./.github/actions/setup-macos-codesigning`, `./.github/actions/build-plugin` with macOS/Linux-specific inputs) and macOS signing/notarization secrets. Since these jobs never execute, they can silently rot (e.g. if `build-plugin`/`package-plugin` actions change their macOS/Linux-specific inputs) without any CI signal, and anyone re-enabling this block later (e.g. when adding macOS support, per the README TODO) would be trusting untested YAML.
**Fix:** This is a deliberate, documented decision (`D-07: Windows x64 only (Phase 1)`) and not incorrect, but consider either deleting the disabled jobs now (they're recoverable from git history when macOS/Linux support is actually planned) or adding a workflow_dispatch input to exercise them occasionally so they don't bit-rot silently.

---

_Reviewed: 2026-09-23T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
