---
phase: 01-repository-foundation-build-system
fixed_at: 2026-09-23T18:25:00Z
review_path: .planning/phases/01-repository-foundation-build-system/01-REVIEW.md
iteration: 1
findings_in_scope: 6
fixed: 4
skipped: 2
status: partial
---

# Phase 01: Code Review Fix Report

**Fixed at:** 2026-09-23T18:25:00Z
**Source review:** .planning/phases/01-repository-foundation-build-system/01-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 6 (2 Warning, 4 Info — `fix_scope: all`)
- Fixed: 4
- Skipped: 2

**Verification environment:** All fixes were applied and verified inside an isolated git worktree (`.claude/worktrees/rf-01-*`, branch `gsd-reviewfix/01-*`), then fast-forwarded onto `master`. C++/CMake changes were formatted with clang-format and verified by a full `cmake --preset windows-x64` configure + `cmake --build --preset windows-x64` build in that same worktree, producing `build_x64/RelWithDebInfo/obs-animated-avatar.dll` after each source/CMake change (exit 0 each time). These build results are reproducible from `master` after the worktree fast-forward/cleanup, since the worktree had no separate `node_modules`/dependency state — the OBS dev package download in `.deps` and `build_x64/` are cached build artifacts, gitignored, and unaffected by the worktree teardown.

## Fixed Issues

### WR-01: Inconsistent/incomplete GPL license headers

**Files modified:** `src/plugin-main.cpp`, `src/avatar-source.cpp`
**Commit:** af6b3d0
**Applied fix:** Replaced the unfilled template placeholder header in `plugin-main.cpp` (`Copyright (C) <Year> <Developer> <Email Address>`) and added the same GPL-2.0-or-later header comment to `avatar-source.cpp` (which previously had none), both using the project's real identity: `Animated Avatar Plugin for OBS`, `Copyright (C) 2026 quick-1y <l0nglife3025@gmail.com>`. Verified with clang-format and a full build (DLL produced, exit 0).

### WR-02: Silent failure path when placeholder texture creation fails

**Files modified:** `src/avatar-source.cpp`
**Commit:** aa9b4e4
**Applied fix:** `avatar_create` now checks the return value of `gs_texture_create()` after leaving the graphics lock; on `nullptr` it logs `obs_log(LOG_ERROR, "[init] failed to create placeholder texture")` instead of unconditionally logging success. The pre-existing `"[init] avatar source created"` success log is now only emitted when texture creation actually succeeded. Verified with clang-format and a full build (DLL produced, exit 0).

### IN-01: `register_avatar_source()` forward-declared ad hoc instead of via a shared header

**Files modified:** `src/avatar-source.h` (new), `src/avatar-source.cpp`, `src/plugin-main.cpp`, `CMakeLists.txt`
**Commit:** 00551c9
**Applied fix:** Added `src/avatar-source.h` (with the standard GPL header) declaring `void register_avatar_source();`. Both `avatar-source.cpp` and `plugin-main.cpp` now `#include "avatar-source.h"` instead of `plugin-main.cpp` carrying an ad hoc forward declaration. `CMakeLists.txt`'s `target_sources()` call now lists `src/avatar-source.h` alongside the two `.cpp` files so the header is tracked as part of the module's source set. Verified with clang-format, a CMake reconfigure (to pick up the new source), and a full build (DLL produced, exit 0).

### IN-02: `ENABLE_QT`/Qt6 scaffold retained in CMakeLists.txt despite project's explicit no-Qt constraint

**Files modified:** `CMakeLists.txt`
**Commit:** 3ea0aef
**Applied fix:** Removed `option(ENABLE_QT "Use Qt functionality" OFF)` and the entire `if(ENABLE_QT) ... endif()` block (Qt6 `find_package`, `Qt6::Core`/`Qt6::Widgets` linking, AUTOMOC/AUTOUIC/AUTORCC target properties). `ENABLE_FRONTEND_API` and its `if()` block were left untouched — the finding (IN-02) and its cited line ranges (`CMakeLists.txt:8,27-38`) scoped the issue specifically to the Qt option/block, not the frontend-api option, so the fix was kept narrow to that citation. Verified with a CMake reconfigure and a full build (DLL produced, exit 0); `gersemi` was not available locally to check CMake formatting style, so Tier 3 fallback (Tier 1 re-read + successful configure/build) was used.

## Skipped Issues

### IN-03: Windows SDK minimum version is inconsistent between README.md and CLAUDE.md

**File:** `README.md:11`
**Reason:** Verified against `CMakePresets.json:60` (`"architecture": "x64,version=10.0.22621"`) — the preset actually requires Windows SDK `10.0.22621`, which is exactly what `README.md` states. `README.md` is correct; the inconsistency is in `.claude/CLAUDE.md` (which states `10.0.20348.0`), but `.claude/CLAUDE.md` is a GSD-managed file and out of scope for this fixer per environment instructions. No change made.
**Original issue:** README.md states "Windows SDK 10.0.22621 or later" while CLAUDE.md states "Windows SDK 10.0.20348.0 or later" — two different minimum-version claims for the same requirement.

### IN-04: ~180 lines of permanently-disabled macOS/Ubuntu CI job definitions retained in the workflow

**File:** `.github/workflows/build-project.yaml:74-254`
**Reason:** Per environment guidance, the disabled `macos-build` and `ubuntu-build` jobs are intentionally kept (re-enableable per deferred ideas in `01-CONTEXT.md`) rather than deleted. Inspected the file and confirmed both jobs already carry an explicit `# D-07: Windows x64 only (Phase 1) — no macOS/Ubuntu build in MVP` comment directly above their `if: false` guard (lines 76 and 182), which satisfies the finding's suggested improvement ("consider ... adding a workflow_dispatch input ... so they don't bit-rot silently" / documenting the decision). No further change made; documenting this choice here.
**Original issue:** The disabled `macos-build`/`ubuntu-build` jobs retain full (untested) job bodies that could silently rot without CI signal if action inputs change upstream.

---

_Fixed: 2026-09-23T18:25:00Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
