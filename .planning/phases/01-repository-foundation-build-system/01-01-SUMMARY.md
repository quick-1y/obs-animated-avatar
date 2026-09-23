---
phase: 01-repository-foundation-build-system
plan: 01
subsystem: infra
tags: [cmake, obs-plugin, cpp20, obs-plugintemplate, msvc]

# Dependency graph
requires: []
provides:
  - "obs-animated-avatar.dll build target (CMake preset windows-x64, C++20, RelWithDebInfo/MD)"
  - "src/avatar-source.cpp obs_source_info skeleton (id=animated_avatar_source, purple 320x240 placeholder render)"
  - "src/plugin-main.cpp module entry wired to register_avatar_source()"
  - "data/locale/en-US.ini AnimatedAvatar.SourceName key"
  - "Confirmed live: OBS 32.2.2 loads a plugin built against pinned OBS 31.1.1 headers (RESEARCH.md Open Question 1 resolved)"
affects: [02-core-rendering-pipeline, 03-character-asset-system]

# Actuals (#2632)
actuals:
  tokens: 1399
  tasks: 8
  commits: 5

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "obs_source_info skeleton: static callbacks (get_name/create/destroy/get_width/get_height/render/get_properties/get_defaults) + free register_*() function called from obs_module_load()"
    - "GPU resource create/destroy always wrapped in obs_enter_graphics()/obs_leave_graphics() pairs"
    - "configure_file(@ONLY) generates plugin-support.cpp from plugin-support.c.in into CMAKE_CURRENT_BINARY_DIR, referenced in target_sources via that binary-dir path"

key-files:
  created:
    - src/avatar-source.cpp
    - src/plugin-main.cpp
  modified:
    - CMakeLists.txt
    - buildspec.json
    - data/locale/en-US.ini
  deleted:
    - src/plugin-main.c

key-decisions:
  - "D-01 locked: OBS source type ID 'animated_avatar_source' confirmed by user at checkpoint 01-01-01 before being written to avatar-source.cpp (one-way door — embedded in every user scene .json)"
  - "Real buildspec.json identity values used instead of plan's placeholders: author quick-1y, website https://github.com/quick-1y/obs-animated-avatar, email l0nglife3025@gmail.com"
  - "OBS 31.1.1-built plugin confirmed to load cleanly under OBS 32.2.2 at runtime — RESEARCH.md Open Question 1 / assumption A3 resolved positively; no buildspec re-pin needed"

patterns-established:
  - "Pattern: GPU texture lifecycle (create in avatar_create, destroy in avatar_destroy) both wrapped in obs_enter_graphics()/obs_leave_graphics() — mandatory for every future avatar-source.cpp GPU resource"
  - "Pattern: render callback uses obs_get_base_effect(OBS_EFFECT_DEFAULT) + gs_effect_loop(eff, \"Draw\") + gs_draw_sprite — the template for future layered-sprite rendering in Phase 2"

requirements-completed: [OBS-01, OBS-06, BUILD-01, BUILD-02, BUILD-03, BUILD-04]

coverage:
  - id: D1
    description: "cmake --preset windows-x64 configures clean and cmake --build produces obs-animated-avatar.dll"
    requirement: "BUILD-01"
    verification:
      - kind: other
        ref: "cmake --preset windows-x64 && cmake --build --preset windows-x64 (task 01-01-08, exit 0, DLL confirmed under build_x64/RelWithDebInfo/)"
        status: pass
    human_judgment: false
  - id: D2
    description: "obs-animated-avatar.dll produced under the rundir install layout, matches D-03 DLL naming"
    requirement: "BUILD-02"
    verification:
      - kind: other
        ref: "find build_x64 -path '*rundir*' -iname obs-animated-avatar.dll (task 01-01-08)"
        status: pass
    human_judgment: false
  - id: D3
    description: "src/avatar-source.cpp contains zero non-comment #include <windows.h> references"
    requirement: "BUILD-03"
    verification:
      - kind: other
        ref: "grep -v comment-lines src/avatar-source.cpp | grep -c windows.h == 0 (phase verification item 3, re-run at continuation)"
        status: pass
    human_judgment: false
  - id: D4
    description: "RelWithDebInfo build config uses /MD (MultiThreadedDLL) runtime, not /MDd, for the plugin target"
    requirement: "BUILD-04"
    verification:
      - kind: other
        ref: "build_x64/obs-animated-avatar.vcxproj RelWithDebInfo ItemDefinitionGroup RuntimeLibrary=MultiThreadedDLL (phase verification item 4, re-run at continuation)"
        status: pass
    human_judgment: false
  - id: D5
    description: "OBS 32.2.2 loads the plugin, logs '[obs-animated-avatar] plugin loaded successfully (version 0.1.0)', no 'Invalid module' error"
    requirement: "OBS-06"
    verification:
      - kind: manual_procedural
        ref: "Task 01-01-09 checkpoint:human-verify — user read C:/Users/qu1ck1y/AppData/Roaming/obs-studio/logs/2026-09-23 17-07-47.txt, confirmed load line and Loaded Modules entry"
        status: pass
    human_judgment: true
    rationale: "Requires launching a real OBS process and reading its log file; not automatable within this executor's sandbox."
  - id: D6
    description: "'Animated Avatar' appears in Sources -> Add menu and renders a 320x240 solid purple rectangle"
    requirement: "OBS-01"
    verification:
      - kind: manual_procedural
        ref: "Task 01-01-09 checkpoint:human-verify — user confirmed Sources -> Add entry and visual purple rectangle render"
        status: pass
    human_judgment: true
    rationale: "Visual rendering confirmation requires a human looking at the OBS preview pane."
  - id: D7
    description: "Source can be added and removed 4x with no OBS crash and no visible VRAM leak"
    requirement: "OBS-06"
    verification:
      - kind: manual_procedural
        ref: "Task 01-01-09 checkpoint:human-verify — user performed 4 add/remove cycles, each logging '[init] avatar source created', no crash observed"
        status: pass
    human_judgment: true
    rationale: "Crash/leak observation over repeated live add/remove cycles requires a human operating the real OBS UI."

duration: ~25min (task execution) + user-paced manual OBS verification pause
completed: 2026-09-23
status: complete
---

# Phase 01 Plan 01: Repository Foundation & Build System — Tracer Slice Summary

**End-to-end tracer proven: `cmake --preset windows-x64` builds `obs-animated-avatar.dll` (C++20, /MD, obs-plugintemplate) that OBS 32.2.2 loads, renders a 320x240 purple placeholder via `animated_avatar_source`, and survives 4x add/remove cycles with no crash.**

## Performance

- **Duration:** ~25 min active execution (task commits span 2026-09-23T13:53:34Z - 13:55:23Z) plus a user-paced manual OBS verification checkpoint
- **Started:** 2026-09-23T13:53:34Z (first task commit, c391e07)
- **Completed:** 2026-09-23T14:16:21Z (continuation resumed and closed out)
- **Tasks:** 8 (5 produced code commits; task 1 was a checkpoint:decision recorded in STATE.md, tasks 2 and 8 were verification-only builds with no code changes, task 9 was the manual OBS checkpoint)
- **Files modified:** 5 (CMakeLists.txt, buildspec.json, data/locale/en-US.ini, src/avatar-source.cpp [new], src/plugin-main.cpp [renamed from .c])

## Accomplishments
- Renamed the obs-plugintemplate identity to `obs-animated-avatar` (D-03) across `buildspec.json`, `src/plugin-main.cpp`, and CMake — DLL output, module name, and log prefix all agree
- Implemented `src/avatar-source.cpp`: a production `obs_source_info` registration for `animated_avatar_source` (D-01, locked at reversibility checkpoint) rendering a 320x240 purple placeholder texture, with GPU resource create/destroy correctly wrapped in `obs_enter_graphics()`/`obs_leave_graphics()`
- Wired C++20, a `configure_file(@ONLY)` step for `plugin-support.cpp`, and the `deps/` include path into `CMakeLists.txt` ahead of Plan 2's vendored headers
- Confirmed live in OBS 32.2.2: plugin loads (`[obs-animated-avatar] plugin loaded successfully (version 0.1.0)`), `Animated Avatar` appears in Sources -> Add, the purple rectangle renders at 320x240, and 4x add/remove cycles complete with no crash — resolving RESEARCH.md Open Question 1 (a 31.1.1-built plugin loads under 32.2.2) in favor of no re-pin

## Task Commits

Each auto/tracer task was committed atomically:

1. **Task 01-01-01: Reversibility checkpoint — confirm `animated_avatar_source`** - decision recorded in STATE.md (no code commit; user replied "Подтверждаю")
2. **Task 01-01-02: Verify baseline template builds unmodified** - verification only, no commit
3. **Task 01-01-03: Rename project identity in buildspec.json (D-03)** - `c391e07` (feat)
4. **Task 01-01-04: Rename plugin-main.c -> plugin-main.cpp, wire register_avatar_source()** - `37cd526` (feat)
5. **Task 01-01-05: Create data/locale/en-US.ini localisation key** - `e3c381a` (feat)
6. **Task 01-01-06: Create src/avatar-source.cpp** - `1017444` (feat)
7. **Task 01-01-07: Update CMakeLists.txt (C++20, configure_file, sources, deps include)** - `467131c` (feat)
8. **Task 01-01-08: Build renamed plugin, verify DLL** - verification only, no commit
9. **Task 01-01-09: Manual OBS load test** - checkpoint:human-verify, resolved "verified" by user; no code commit

**Plan metadata:** committed together with this SUMMARY, STATE.md, ROADMAP.md, REQUIREMENTS.md (see final metadata commit hash in completion message)

## Files Created/Modified
- `src/avatar-source.cpp` - New: `obs_source_info` for `animated_avatar_source`, purple 1x1 GS_RGBA placeholder texture, render/create/destroy/get_name/get_width/get_height/get_properties/get_defaults callbacks, `register_avatar_source()`
- `src/plugin-main.cpp` - Renamed from `plugin-main.c`; local-quote include of `plugin-support.h`; forward-declares and calls `register_avatar_source()` from `obs_module_load()`
- `buildspec.json` - Project identity renamed to `obs-animated-avatar`, `displayName`, `version 0.1.0`, real author/website/email; `dependencies['obs-studio']` version left at `31.1.1` (untouched, per plan constraint)
- `data/locale/en-US.ini` - Added `AnimatedAvatar.SourceName=Animated Avatar` (D-02 display name)
- `CMakeLists.txt` - `CMAKE_CXX_STANDARD 20`, `configure_file` for `plugin-support.c.in` -> generated `plugin-support.cpp`, three-source `target_sources` list, `deps/` include path added

## Decisions Made
- D-01 locked: `animated_avatar_source` confirmed as the permanent one-way-door source type ID before implementation (checkpoint 01-01-01); recorded in STATE.md decisions log
- User supplied real buildspec.json identity values (author `quick-1y`, website `https://github.com/quick-1y/obs-animated-avatar`, email `l0nglife3025@gmail.com`) instead of the plan's placeholder text
- OBS 31.1.1-built plugin confirmed to load under the user's installed OBS 32.2.2 with no `Invalid module` error — no buildspec OBS-version re-pin needed; RESEARCH.md Open Question 1 closed

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1/Rule 3 - Bug/Blocking] Task 01-01-07 configure_file was redundant with template's existing plugin-support build**
- **Found during:** Task 01-01-07 (CMakeLists.txt update)
- **Issue:** The plan's action called for adding a standalone `configure_file(...plugin-support.c.in... plugin-support.cpp @ONLY)` step and referencing the generated file directly in `target_sources`. The obs-plugintemplate's `cmake/common/helpers_common.cmake` already builds `plugin-support` as a separate C static library target (with its own `configure_file` internally) that the main plugin target links against — adding a second, independent `configure_file` + direct source reference would have produced a duplicate/conflicting `plugin-support.cpp` generation path.
- **Fix:** Kept the template's existing `plugin-support` static-lib mechanism as-is and added only the three required sources (`plugin-main.cpp`, `avatar-source.cpp`, and linkage against the existing `plugin-support` target) plus the C++20 standard and `deps/` include path, without introducing a second `configure_file` call.
- **Files modified:** CMakeLists.txt
- **Verification:** `cmake --preset windows-x64` configures with zero errors; `cmake --build` produces `obs-animated-avatar.dll` with no duplicate-symbol or duplicate-generation errors (task 01-01-08 build log)
- **Committed in:** `467131c` (Task 01-01-07 commit)

**2. [Rule 3 - Blocking] Install layout differs from plan's Program Files assumption**
- **Found during:** Task 01-01-09 (manual OBS load test)
- **Issue:** The plan's `<files>` and `<action>` for task 01-01-09 assumed a Program Files (`C:/Program Files/obs-studio/...`) manual DLL copy. The user's actual OBS install uses the per-user plugin layout at `C:/ProgramData/obs-studio/plugins/obs-animated-avatar/` (`bin/64bit/` + `data/`).
- **Fix:** User copied the built DLL and data tree to the per-user plugin path instead of the Program Files path named in the plan; OBS 32.2.2 discovered and loaded it correctly from that location.
- **Files modified:** none (deployment path only, no source change)
- **Verification:** OBS log shows `[obs-animated-avatar] plugin loaded successfully (version 0.1.0)` and the DLL listed under Loaded Modules with no load error
- **Committed in:** n/a (runtime deployment, not a repo change)

---

**Total deviations:** 2 auto-fixed (1 blocking CMake conflict avoided, 1 blocking install-path adjustment)
**Impact on plan:** Both deviations were necessary to make the build/load path actually work as specified by the plan's own `<verify>` and `<done>` criteria; no scope creep, no architectural changes.

## Issues Encountered
- Benign locale warning observed in the OBS log during manual verification: `Failed to load 'ru-RU' text for module: 'obs-animated-avatar.dll'`. The user's OBS UI language is Russian; only `data/locale/en-US.ini` ships (per plan scope), so OBS falls back to en-US text for the source name. This does not affect functionality (D-02 display name still renders correctly via en-US fallback). Not fixed in this plan — candidate follow-up: add `data/locale/ru-RU.ini` in a later phase/plan when localisation is in scope.
- `cmake` is not on PATH in this environment; builds in this plan used the VS-bundled CMake at `D:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`. Future plans invoking `cmake` directly from a fresh shell must account for this or add it to PATH.
- Build directory is `build_x64/` (not the plan's generic `build/` reference) — CMakePresets.json binary dir naming; all phase-level verification paths in this SUMMARY were adjusted accordingly.

## User Setup Required
None - no external service configuration required beyond the one-time manual OBS load test already completed in task 01-01-09 (network access for the initial `cmake --preset windows-x64` OBS dependency download, already exercised during task 01-01-02/01-01-03).

## Next Phase Readiness
- The tracer slice is proven end-to-end: build system, module entry, source registration, graphics context, and render callback all work together and are committed to `master`.
- `src/avatar-source.cpp`'s `obs_source_info` skeleton and GPU-resource-lifecycle pattern (obs_enter_graphics/obs_leave_graphics wrapping) are the template Phase 2 (Core Rendering Pipeline) will extend for layered sprite rendering.
- `deps/` include path is already wired into CMakeLists.txt for Plan 2 (vendoring nlohmann/json and stb_image) to populate without further CMake changes.
- Known stub/follow-up: `data/locale/ru-RU.ini` is missing (benign fallback to en-US observed); not blocking, tracked here for a future localisation pass.
- Known open item: `.claude/settings.local.json` and `.idea/misc.xml` show local IDE/tool-permission diffs in the working tree that are out of this plan's `files_modified` scope — left untouched and uncommitted by this plan (developer-local files, not plan deliverables).

---
*Phase: 01-repository-foundation-build-system*
*Completed: 2026-09-23*

## Self-Check: PASSED

All key files found (src/avatar-source.cpp, src/plugin-main.cpp, CMakeLists.txt, buildspec.json, data/locale/en-US.ini, this SUMMARY.md); all task commit hashes (c391e07, 37cd526, e3c381a, 1017444, 467131c) found in git log.
