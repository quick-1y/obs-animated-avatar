---
phase: 01-repository-foundation-build-system
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - CMakeLists.txt
  - buildspec.json
  - src/plugin-main.c
  - src/plugin-support.c.in
  - src/plugin-support.h
  - src/avatar-source.cpp
  - data/locale/en-US.ini
files_deleted:
  - src/plugin-main.c
autonomous: false
requirements:
  - OBS-01
  - OBS-06
  - BUILD-01
  - BUILD-02
  - BUILD-03
  - BUILD-04
user_setup:
  - service: obs-studio-runtime
    why: "Manual load test — copy DLL to OBS plugins dir and launch OBS to verify plugin loads and renders (task 1.5, task 1.9 verify steps require a real OBS process)"
    dashboard_config:
      - task: "OBS Studio 32.2.2 already installed at C:/Program Files/obs-studio (verified in RESEARCH.md Environment Availability)"
        location: "C:/Program Files/obs-studio/obs-plugins/64bit/"
      - task: "Copy build output DLL + data/ tree to plugin install dir when task 1.5 runs"
        location: "C:/Program Files/obs-studio/"
  - service: internet-access-configure-time
    why: "First `cmake --preset windows-x64` downloads OBS 31.1.1 sources + prebuilt deps (~100-300MB) — no offline fallback"
    dashboard_config:
      - task: "Confirm network access at task 01-01-03 (first configure); subsequent configures cache"
        location: "network (github.com/obsproject)"

estimate:
  tokens: 95000
  raw_tokens: 65000
  tasks: 9
  confidence: low

must_haves:
  truths:
    - "cmake --preset windows-x64 exits 0 with no errors (BUILD-01, D-04)"
    - "cmake --build --preset windows-x64 produces obs-animated-avatar.dll (BUILD-02, D-03)"
    - "OBS 32.2.2 loads the plugin without 'Invalid module' error (OBS-06, D-01, D-03)"
    - "Log line '[obs-animated-avatar] plugin loaded successfully (version 0.1.0)' appears in OBS log (OBS-06)"
    - "'Animated Avatar' appears in Sources → Add menu (OBS-01, D-02)"
    - "Adding source shows a 320x240 solid purple rectangle in OBS preview (D-01, D-02)"
    - "Deleting the source produces no crash and no VRAM leak (OBS-06, OBS-07)"
    - "src/avatar-source.cpp contains zero #include <windows.h> lines (BUILD-03)"
    - "Build uses /MD (MultiThreadedDLL) runtime — no /MDd anywhere (BUILD-04)"
  artifacts:
    - "CMakeLists.txt (modified: C++20 standard, target_sources list, configure_file for plugin-support, deps/ include path)"
    - "buildspec.json (modified: name=obs-animated-avatar, displayName='Animated Avatar Plugin for OBS', version=0.1.0, author, email, website, bundleId)"
    - "src/plugin-main.cpp (renamed from plugin-main.c; adds register_avatar_source() call)"
    - "src/plugin-support.cpp (generated from plugin-support.c.in via configure_file — @CMAKE_PROJECT_NAME@ → obs-animated-avatar)"
    - "src/plugin-support.h (kept from template; already C++-compatible)"
    - "src/avatar-source.cpp (new: obs_source_info with id='animated_avatar_source', purple 1x1 GS_RGBA texture, gs_draw_sprite render)"
    - "data/locale/en-US.ini (updated: AnimatedAvatar.SourceName=Animated Avatar)"
    - "build/RelWithDebInfo/obs-animated-avatar.dll (build output)"
  key_links:
    - "buildspec.json 'name' field ↔ CMakeLists.txt bootstrap ${_name} ↔ set_target_properties_plugin OUTPUT_NAME ↔ DLL filename (mismatch breaks install path)"
    - "OBS_DECLARE_MODULE() + OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, 'en-US') ↔ data/locale/en-US.ini file existence (missing file → raw key returned)"
    - "register_avatar_source() call in obs_module_load() ↔ obs_register_source(&avatar_source_info) in avatar-source.cpp (missing call → source never appears in Sources menu)"
    - "obs_source_info.id = 'animated_avatar_source' (D-01 one-way door) ↔ scene .json serialization (rename orphans user scenes forever)"
    - "obs_enter_graphics() / obs_leave_graphics() around gs_texture_create AND gs_texture_destroy (missing → crash from wrong-thread D3D11 access)"
    - "plugin-support.c.in @CMAKE_PROJECT_NAME@ substitution ↔ project(obs-animated-avatar) declaration ↔ [obs-animated-avatar] log prefix (all three must agree)"
---

<objective>
Deliver an end-to-end **tracer slice**: `cmake --preset windows-x64` → build DLL → copy to OBS → OBS loads plugin → user adds `Animated Avatar` source → sees 320×240 solid purple rectangle → deletes source cleanly → no crash. One thin path through every layer this phase touches (build system, OBS module entry, source registration, graphics context, render callback), production-quality, committed to main.

Purpose: Prove the architecture end-to-end on the tracer before any expansion (deps, CI, docs). Every locked decision from CONTEXT.md (D-01 source ID, D-02 display name, D-03 DLL name, D-06/D-07 CI scope, D-08/D-09 character path model) that has a Phase 1 code touchpoint gets its production implementation in this slice. The scaffold that ships in this tracer is the skeleton every subsequent phase builds on.

Output:
- Modified `CMakeLists.txt`, `buildspec.json` renamed to `obs-animated-avatar` (D-03)
- `src/plugin-main.cpp` (renamed from `.c`), `src/plugin-support.cpp` (generated), `src/plugin-support.h`, `src/avatar-source.cpp`, `data/locale/en-US.ini`
- Working `obs-animated-avatar.dll` that OBS 32.2.2 loads and renders
- Reversibility checkpoint recorded for D-01 (source type ID `animated_avatar_source` — one-way door)
</objective>

<execution_context>
@D:/Users/qu1ck1y/Documents/pyProjects/obs_avatar/.claude/gsd-core/workflows/execute-plan.md
@D:/Users/qu1ck1y/Documents/pyProjects/obs_avatar/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/REQUIREMENTS.md
@.planning/phases/01-repository-foundation-build-system/01-CONTEXT.md
@.planning/phases/01-repository-foundation-build-system/01-RESEARCH.md
@.planning/phases/01-repository-foundation-build-system/01-PATTERNS.md

# Source files that will be modified/read (all git-tracked, verified via `git ls-files`):
@CMakeLists.txt
@buildspec.json
@CMakePresets.json
@src/plugin-main.c
@src/plugin-support.c.in
@src/plugin-support.h
@data/locale/en-US.ini
@cmake/windows/helpers.cmake
</context>

<tasks>

<task type="checkpoint:decision" gate="blocking-human">
  <name>Reversibility checkpoint — confirm one-way-door source type ID `animated_avatar_source`</name>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-CONTEXT.md (D-01, D-02, D-03 decisions), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md (Pitfall 4 — OBS Source ID Is a One-Way Door)</read_first>
  <reversibility rating="one-way">Source type ID `animated_avatar_source` is embedded into every OBS scene .json file that includes the source. Once users save scenes, renaming this ID orphans their settings permanently — surviving users would see "[Missing source]" and lose their configured avatar setup. Per D-01 in CONTEXT.md this is locked; this checkpoint is a final verification before that string is written to committed code.</reversibility>
  <action>
    Present to user: "Phase 1 is about to hard-code the OBS source type ID `animated_avatar_source` into src/avatar-source.cpp (D-01 in CONTEXT.md). This string will appear in every user's OBS scene .json once they add the source. Renaming it after v0.1.0 ships requires a legacy alias via `unversioned_id` or users' scenes break. Confirm the id `animated_avatar_source` before we bake it in." Wait for explicit confirmation. On any other response, halt the plan.
  </action>
  <verify>
    <automated>test -f .planning/phases/01-repository-foundation-build-system/01-CONTEXT.md &amp;&amp; grep -q 'animated_avatar_source' .planning/phases/01-repository-foundation-build-system/01-CONTEXT.md</automated>
    <fails_when>non-zero exit or grep returns no match — CONTEXT.md must contain the locked id string; if it does not, D-01 is not actually locked and this checkpoint cannot proceed</fails_when>
  </verify>
  <done>User has explicitly confirmed `animated_avatar_source` as the permanent source type ID. Decision recorded in STATE.md decisions log with rationale "One-way door per D-01, locked before implementation of avatar-source.cpp".</done>
</task>

<task type="tracer" tdd="false">
  <name>Task 01-01-02: Verify baseline template builds unmodified (spike before rename)</name>
  <files>build/ (generated), buildspec.json (read only), CMakePresets.json (read only), CMakeLists.txt (read only)</files>
  <precondition>git submodule status runs clean and reports no missing submodules — the template's `cmake/common/` and `.github/actions/` are pulled in the initial checkout (see RESEARCH.md Pitfall 7); if any submodule shows a leading `-` character it must be initialised first via `git submodule update --init --recursive`.</precondition>
  <read_first>CMakeLists.txt, buildspec.json, CMakePresets.json, .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Environment Availability, §Common Pitfalls (Pitfalls 1, 6, 7)</read_first>
  <action>Spike step (per RESEARCH.md primary recommendation "verify the template builds unmodified before renaming"). Run `git submodule update --init --recursive` first (Pitfall 7). Then from repo root run `cmake --preset windows-x64` and confirm exit code 0. If configure fails with 'unknown CMake command obs_add_module' — halt: it means the template shape has changed since RESEARCH.md was written and Pattern 1 needs re-verification. If configure fails on OBS dep download — halt: check network. Then run `cmake --build --preset windows-x64` and confirm exit code 0 with a produced .dll under `build/RelWithDebInfo/`. This proves the baseline before any project rename touches CMakeLists.txt or buildspec.json.</action>
  <verify>
    <automated>cmake --preset windows-x64 2>&amp;1 | tail -20</automated>
    <fails_when>non-zero exit, or the substring "Error" or "unknown CMake command" appears in the tailed output</fails_when>
    <automated>cmake --build --preset windows-x64 2>&amp;1 | tail -10</automated>
    <fails_when>non-zero exit, or the substring "error" (case-insensitive) or "FAILED" appears in the tailed output</fails_when>
    <automated>ls build/RelWithDebInfo/*.dll 2>&amp;1</automated>
    <fails_when>non-zero exit or no .dll present under build/RelWithDebInfo/</fails_when>
  </verify>
  <done>Baseline template configures + builds + produces a .dll under `build/RelWithDebInfo/` (name doesn't matter — this is the unmodified template). CMake warnings (if any) are noted. No `obs_add_module` errors surfaced.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-03: Rename project identity in buildspec.json (D-03)</name>
  <files>buildspec.json</files>
  <read_first>buildspec.json (full — 45 lines), .planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §buildspec.json (modify), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Pattern 5 buildspec.json Fields to Update, §Pitfall 5 (name must match DLL output)</read_first>
  <action>Update ONLY the project identity fields per D-03 (DLL name obs-animated-avatar) and CONTEXT.md discretion (email l0nglife3025@gmail.com from user context): set `name` to `obs-animated-avatar`, `displayName` to `Animated Avatar Plugin for OBS`, `version` to `0.1.0`, `author` to `Your Name` (placeholder — user updates later), `website` to `https://github.com/YOUR_USERNAME/obs-animated-avatar` (placeholder), `email` to `l0nglife3025@gmail.com`, and `platformConfig.macos.bundleId` to `com.example.obs-animated-avatar`. Do NOT touch the entire `dependencies` block — OBS version stays pinned at 31.1.1 per RESEARCH.md (SHA256 hashes are verified checksums; changing them breaks the download bootstrap).</action>
  <verify>
    <automated>node -e "const j=require('./buildspec.json');const ok=j.name==='obs-animated-avatar'&amp;&amp;j.version==='0.1.0'&amp;&amp;j.dependencies['obs-studio'].version==='31.1.1';console.log(ok?'PASS':'FAIL '+JSON.stringify({n:j.name,v:j.version,obs:j.dependencies['obs-studio'].version}));process.exit(ok?0:1);"</automated>
    <fails_when>non-zero exit or output starts with "FAIL" — name must equal `obs-animated-avatar`, version must equal `0.1.0`, and obs-studio dep version must still be `31.1.1`</fails_when>
  </verify>
  <done>buildspec.json project identity renamed; obs-studio dependency block untouched (version 31.1.1 preserved).</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-04: Rename plugin-main.c → plugin-main.cpp and wire register_avatar_source()</name>
  <files>src/plugin-main.cpp, src/plugin-main.c</files>
  <read_first>src/plugin-main.c (full — 34 lines), .planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §src/plugin-main.cpp, .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Pattern 2</read_first>
  <action>Perform `git mv src/plugin-main.c src/plugin-main.cpp`. Then edit the new .cpp: change `#include <plugin-support.h>` to the local-quote form `#include "plugin-support.h"` (Pattern 3 in PATTERNS.md — file is now in the same source dir and no longer on a system include path). Add a forward declaration `void register_avatar_source();` above `OBS_DECLARE_MODULE()`. Inside `obs_module_load()`, after the existing `obs_log(LOG_INFO, ...)` line, add a call to `register_avatar_source();` (the function is defined later in src/avatar-source.cpp; forward decl lets this compile). Keep the existing `obs_module_unload` body as-is.</action>
  <verify>
    <automated>test -f src/plugin-main.cpp &amp;&amp; ! test -f src/plugin-main.c</automated>
    <fails_when>non-zero exit — either plugin-main.cpp is missing or plugin-main.c still exists (git mv should have removed the .c)</fails_when>
    <automated>grep -c 'register_avatar_source' src/plugin-main.cpp</automated>
    <fails_when>output is a number less than 2 — the symbol must appear at least twice (one forward declaration and one call site); "register_avatar_source" must be referenced in the file, not merely mentioned</fails_when>
  </verify>
  <done>src/plugin-main.cpp exists (src/plugin-main.c removed via git mv); forward declaration + call to register_avatar_source() present; include of plugin-support.h uses local quote form.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-05: Create data/locale/en-US.ini localisation key</name>
  <files>data/locale/en-US.ini</files>
  <read_first>data/locale/en-US.ini (current contents — may be empty or template stub), .planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §data/locale/en-US.ini, .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Code Examples data/locale/en-US.ini</read_first>
  <action>Ensure `data/locale/en-US.ini` contains the single required key line `AnimatedAvatar.SourceName=Animated Avatar` (per downstream_consumer instruction — the src/avatar-source.cpp `get_name` callback resolves this key via `obs_module_text`). If the file already exists with other unrelated keys, append this line. Do not remove pre-existing template keys. This key implements D-02 (display name `Animated Avatar` in the OBS Sources menu).</action>
  <verify>
    <automated>grep -q 'AnimatedAvatar.SourceName=Animated Avatar' data/locale/en-US.ini</automated>
    <fails_when>non-zero exit — the exact key/value pair `AnimatedAvatar.SourceName=Animated Avatar` is not present in the locale file; without it OBS displays the raw key string in the Sources menu</fails_when>
  </verify>
  <done>data/locale/en-US.ini contains `AnimatedAvatar.SourceName=Animated Avatar`; template keys preserved.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-06: Create src/avatar-source.cpp with obs_source_info registration + purple placeholder render</name>
  <files>src/avatar-source.cpp</files>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §src/avatar-source.cpp (full), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Pattern 4 obs_source_info Skeleton (avatar-source.cpp), §Common Pitfalls (Pitfalls 3, 4), src/plugin-support.h</read_first>
  <action>Create a new file src/avatar-source.cpp implementing the full pattern from PATTERNS.md §src/avatar-source.cpp: include `<obs-module.h>` (angle brackets) and `"plugin-support.h"` (quotes). Declare an `AvatarSourceContext` struct holding `obs_source_t *source`, `uint32_t width = 320`, `uint32_t height = 240`, and `gs_texture_t *placeholder_tex = nullptr`. Implement static callbacks: `avatar_get_name` returning `obs_module_text("AnimatedAvatar.SourceName")` (matches the key from task 01-01-05); `avatar_create` allocating a new AvatarSourceContext, then inside an `obs_enter_graphics()` / `obs_leave_graphics()` pair creating a 1×1 GS_RGBA texture from a `{128, 0, 128, 255}` (purple, straight alpha) byte array via `gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0)`; `avatar_destroy` wrapping `gs_texture_destroy(ctx->placeholder_tex)` in the same graphics-lock pair then `delete ctx`; `avatar_get_width` and `avatar_get_height` returning the struct fields; `avatar_render` using `obs_get_base_effect(OBS_EFFECT_DEFAULT)`, `gs_effect_get_param_by_name(eff, "image")`, `gs_effect_set_texture(param, ctx->placeholder_tex)`, and a `while (gs_effect_loop(eff, "Draw")) { gs_draw_sprite(ctx->placeholder_tex, 0, ctx->width, ctx->height); }` loop; `avatar_get_properties` returning `obs_properties_create()` (empty for Phase 1); `avatar_get_defaults` empty. Declare a static `obs_source_info avatar_source_info` with `.id = "animated_avatar_source"` (D-01 — confirmed at checkpoint 01-01-01), `.type = OBS_SOURCE_TYPE_INPUT`, `.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`, and each callback wired. Define non-static `void register_avatar_source() { obs_register_source(&amp;avatar_source_info); }`. CRITICAL: this file must have zero `#include &lt;windows.h&gt;` and zero platform-specific includes (BUILD-03 constraint) — the placeholder render uses only OBS graphics API.</action>
  <verify>
    <automated>test -f src/avatar-source.cpp</automated>
    <fails_when>non-zero exit — file was not created</fails_when>
    <automated>grep -c 'animated_avatar_source' src/avatar-source.cpp</automated>
    <fails_when>output is less than 1 — the locked source id string must appear at least once as an assignment value</fails_when>
    <automated>grep -c 'obs_enter_graphics' src/avatar-source.cpp</automated>
    <fails_when>output is less than 2 — must be at least two occurrences (one in create, one in destroy); GPU resource creation and destruction both need the graphics lock per RESEARCH.md Pitfall 3</fails_when>
    <automated>grep -v '^[[:space:]]*//' src/avatar-source.cpp | grep -v '^[[:space:]]*\*' | grep -c 'windows\.h'</automated>
    <fails_when>output is not exactly `0` — any non-comment reference to windows.h violates BUILD-03 (engine core has no Win32 includes)</fails_when>
  </verify>
  <done>src/avatar-source.cpp exists with obs_source_info registration for id `animated_avatar_source`, purple 1×1 texture created/destroyed inside graphics lock, render callback uses obs_get_base_effect + gs_draw_sprite, register_avatar_source() defined and callable from plugin-main.cpp, zero windows.h references.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-07: Update CMakeLists.txt — C++20, configure_file for plugin-support, target_sources list, deps/ include</name>
  <files>CMakeLists.txt</files>
  <read_first>CMakeLists.txt (full — 39 lines), .planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §CMakeLists.txt (modify), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Pattern 1 (Root CMakeLists.txt), §Open Question 2 (configure_file for plugin-support.c.in)</read_first>
  <action>Modify CMakeLists.txt in place (per PATTERNS.md §CMakeLists.txt — do NOT rewrite; four targeted changes only): (1) after `include(helpers)` add two lines `set(CMAKE_CXX_STANDARD 20)` and `set(CMAKE_CXX_STANDARD_REQUIRED ON)`; (2) add a `configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/plugin-support.c.in" "${CMAKE_CURRENT_BINARY_DIR}/src/plugin-support.cpp" @ONLY)` call before `target_sources` (this resolves Open Question 2 in RESEARCH.md — the .in file needs an explicit configure_file since the source list is replacing plugin-main.c with .cpp entries); (3) replace the single `target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)` line with the three-source form: `src/plugin-main.cpp`, `"${CMAKE_CURRENT_BINARY_DIR}/src/plugin-support.cpp"`, `src/avatar-source.cpp`; (4) after `target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)` add `target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/deps")` (Plan 2 populates deps/ — the include path can point at a not-yet-existing dir without breaking configure). Do NOT touch: `cmake_minimum_required`, `bootstrap.cmake` include, `project(${_name} VERSION ${_version})` (bootstrap reads name from buildspec.json), `find_package(libobs REQUIRED)`, `set_target_properties_plugin` line, or the ENABLE_FRONTEND_API / ENABLE_QT options.</action>
  <verify>
    <automated>grep -c 'CMAKE_CXX_STANDARD 20' CMakeLists.txt</automated>
    <fails_when>output is less than 1 — C++20 standard is not declared</fails_when>
    <automated>grep -c 'configure_file.*plugin-support\.c\.in' CMakeLists.txt</automated>
    <fails_when>output is less than 1 — the configure_file call that substitutes @CMAKE_PROJECT_NAME@ is missing (without it plugin-support.cpp is never generated)</fails_when>
    <automated>grep -c 'src/avatar-source\.cpp' CMakeLists.txt</automated>
    <fails_when>output is less than 1 — the new source file is not in target_sources and will not be compiled</fails_when>
    <automated>grep -c 'target_include_directories.*deps' CMakeLists.txt</automated>
    <fails_when>output is less than 1 — the deps/ include path is missing (Plan 2 depends on this)</fails_when>
  </verify>
  <done>CMakeLists.txt has C++20 set, configure_file for plugin-support.c.in emitting plugin-support.cpp, three-file target_sources referencing the generated file via CMAKE_CURRENT_BINARY_DIR, and deps/ include path added. Bootstrap / find_package / set_target_properties_plugin lines untouched.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-01-08: Build renamed plugin — cmake reconfigure + build, verify obs-animated-avatar.dll</name>
  <files>build/ (generated)</files>
  <precondition>Tasks 01-01-03 through 01-01-07 all committed — buildspec.json renamed, plugin-main.cpp renamed, avatar-source.cpp exists, en-US.ini has the key, CMakeLists.txt updated for C++20 + configure_file + three sources + deps include. This task is a pure verification build; it modifies no source.</precondition>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Common Pitfalls (Pitfalls 2, 5, 7), CMakePresets.json (already verified in 01-01-02)</read_first>
  <action>Reconfigure and rebuild after the rename. Run `cmake --preset windows-x64` (a fresh configure — bootstrap.cmake re-reads buildspec.json and picks up the new `name`). Then `cmake --build --preset windows-x64`. The output DLL should be named `obs-animated-avatar.dll` (matches D-03 and buildspec.json `name` field per RESEARCH.md Pitfall 5). Do NOT switch to Debug config — the preset defaults to RelWithDebInfo which uses `/MD` (Pitfall 2). If configure fails after the rename with an unexpected error, halt and surface the exact error message rather than retrying.</action>
  <verify>
    <automated>cmake --preset windows-x64 2>&amp;1 | tail -10</automated>
    <fails_when>non-zero exit, or the substring "Error" or "error CMake" appears in the tailed output</fails_when>
    <automated>cmake --build --preset windows-x64 2>&amp;1 | tail -15</automated>
    <fails_when>non-zero exit, or the substring "error C" (MSVC error) or "LNK" (linker error) or "FAILED" appears in the tailed output</fails_when>
    <automated>find build -name 'obs-animated-avatar.dll' -print 2>&amp;1</automated>
    <fails_when>find returns no path — the DLL was not produced with the expected name (indicates buildspec name / OUTPUT_NAME mismatch per Pitfall 5)</fails_when>
  </verify>
  <done>`cmake --preset windows-x64` reconfigures clean; `cmake --build` completes with no errors; `obs-animated-avatar.dll` present under build/ tree. Build uses RelWithDebInfo (verified by the preset — no Debug config override).</done>
</task>

<task type="checkpoint:human-verify" gate="blocking-human">
  <name>Task 01-01-09: Manual OBS load test — install DLL, launch OBS, add source, verify purple rectangle, delete cleanly</name>
  <files>C:/Program Files/obs-studio/obs-plugins/64bit/obs-animated-avatar.dll (user copies), C:/Program Files/obs-studio/data/obs-plugins/obs-animated-avatar/locale/en-US.ini (user copies)</files>
  <precondition>OBS Studio 32.2.2 is installed at C:/Program Files/obs-studio (VERIFIED in RESEARCH.md Environment Availability); user has admin/write access to that directory to drop the DLL and data files; OBS is fully closed before the DLL copy step.</precondition>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Environment Availability, §Pitfall 6 (OBS Version Mismatch), §Open Question 1 (OBS 31→32 load compatibility)</read_first>
  <action>Present to user step by step. (1) Close all running OBS Studio instances. (2) Copy `build/rundir/RelWithDebInfo/obs-plugins/64bit/obs-animated-avatar.dll` (path per set_target_properties_plugin install layout) to `C:/Program Files/obs-studio/obs-plugins/64bit/`. (3) Copy the `data/locale/en-US.ini` file (or the generated `build/rundir/RelWithDebInfo/data/obs-plugins/obs-animated-avatar/` tree) into `C:/Program Files/obs-studio/data/obs-plugins/obs-animated-avatar/`. (4) Launch OBS Studio. (5) Open Help → Log Files → View Current Log and check for the line `[obs-animated-avatar] plugin loaded successfully (version 0.1.0)`. If instead the log shows `Invalid module` — this is Pitfall 6 and Open Question 1: OBS 32.2.2 rejected the plugin built against 31.1.1 headers; halt and report so buildspec.json can be re-pinned to 32.x. (6) In the OBS main window, click the `+` button under Sources; confirm `Animated Avatar` appears in the menu. (7) Select it and confirm a 320×240 solid purple rectangle appears in the OBS preview. (8) Right-click the source → Remove. Confirm OBS does not crash. (9) Add + Remove the source 3 more times to exercise the destroy path and confirm no crash or visible VRAM leak. User replies "verified" if all steps pass, "failed: {reason}" otherwise.</action>
  <verify>
    <automated>find build -path '*rundir*' -name 'obs-animated-avatar.dll' -print 2>&amp;1</automated>
    <fails_when>find returns no path — the rundir install layout was not produced by the build; without it there is no DLL for the user to copy</fails_when>
    <human-check>User has (a) confirmed the [obs-animated-avatar] plugin-loaded log line appears in the OBS current log, (b) confirmed `Animated Avatar` appears in Sources → Add, (c) confirmed the purple rectangle renders at 320x240, and (d) confirmed the source can be added and removed 4 times with no OBS crash and no visible artifact after removal.</human-check>
  </verify>
  <done>User has replied "verified"; the [obs-animated-avatar] log line, the Sources → Add entry `Animated Avatar`, the purple rectangle at 320×240, and 4x add/remove cycles without crash have all been observed. If the load failed with `Invalid module`, the plan halts here for buildspec.json OBS-version re-pin (RESEARCH.md Open Question 1 resolution path).</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| OBS Studio process ↔ plugin DLL | OBS calls into the plugin via `obs_module_load` and the `obs_source_info` callbacks; a malicious DLL placed at the OBS plugin path would gain in-process code execution |
| Plugin ↔ OBS plugin data directory | `data/locale/`, and future `data/characters/` per D-08, are read from disk under the OBS install root — a user-modifiable path |
| Plugin ↔ CMake configure-time downloads | `buildspec.json` triggers download of ~100-300MB of OBS sources and prebuilt deps from GitHub at first configure; supply-chain risk if hashes are altered |
| Plugin GPU code ↔ OBS graphics thread | `gs_*` calls must be on the render thread; wrong-thread access corrupts the D3D11 device state |

## STRIDE Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|-----------|----------|-----------|----------|-------------|-----------------|
| T-01-01 | Spoofing | DLL placement in OBS plugins directory | medium | accept | ASVS L1 scope; OBS itself does not signature-check plugin DLLs. Mitigation is user-side: install only from trusted sources / official release ZIP. Documented in README (Phase 2 task set) — no in-plugin control possible. |
| T-01-02 | Tampering | buildspec.json OBS + dep download at CMake configure | high | mitigate | The template's `buildspec_common.cmake` verifies SHA256 hashes from `buildspec.json` before extraction. Task 01-01-03 explicitly forbids editing the `dependencies` block, and the `dependencies['obs-studio'].version === '31.1.1'` invariant is enforced by the automated verify step in that task. |
| T-01-03 | Elevation of Privilege | Path traversal via plugin data path | low | accept | Phase 1 does no file I/O beyond OBS-managed locale loading. Character path (D-08) uses `obs_get_module_data_path()` which is sandboxed to the plugin data dir; Phase 3 will implement traversal-safe path joining. Deferred to Phase 3 threat model. |
| T-01-04 | Denial of Service | Graphics context misuse crashes OBS | high | mitigate | Task 01-01-06 hard-requires `obs_enter_graphics()` / `obs_leave_graphics()` around every `gs_texture_create` and `gs_texture_destroy`. Verify gate counts `>= 2` occurrences of `obs_enter_graphics` in avatar-source.cpp. Pitfall 3 documented in RESEARCH.md. |
| T-01-05 | Information Disclosure | Keyboard content leaks via plugin | n/a — deferred | accept | Phase 1 does not implement input capture. Deferred to Phase 6 threat model where Raw Input is introduced with the privacy boundary at the input thread (KBD-02). |
| T-01-06 | Repudiation | Log injection via unchecked format string | low | mitigate | All logging uses `obs_log(LOG_LEVEL, format, ...)` with fixed-literal format strings supplied by the plugin (never user input). Task 01-01-06 enforces this pattern; task 01-01-04 keeps the template's existing log lines. |
| T-01-SC | Tampering | npm/pip/cargo installs | n/a | accept | No npm/pip/cargo installs in this phase. All deps are downloaded by the CMake bootstrap (covered under T-01-02) or vendored as single headers in Plan 2. Package Legitimacy Gate not required per RESEARCH.md §Package Legitimacy Audit. |
</threat_model>

<verification>
Phase-level checks after all tasks complete:

1. `cmake --preset windows-x64` exits 0 (BUILD-01)
2. `cmake --build --preset windows-x64` produces `obs-animated-avatar.dll` (BUILD-02, D-03)
3. `src/avatar-source.cpp` contains zero non-comment `#include <windows.h>` lines (BUILD-03)
4. Build config is RelWithDebInfo (uses `/MD`); no `/MDd` anywhere (BUILD-04)
5. Manual: OBS 32.2.2 loads plugin; `[obs-animated-avatar]` log line appears (OBS-06)
6. Manual: `Animated Avatar` visible in Sources → Add (OBS-01, D-02)
7. Manual: Purple 320×240 rectangle renders (D-01 source id, D-02 display, render path proven)
8. Manual: 4x add/remove cycles without crash (OBS-06, OBS-07)
</verification>

<success_criteria>
The tracer is complete when: a real user can install the DLL into a real OBS install, launch OBS, add an `Animated Avatar` source, see a purple rectangle, and remove the source cleanly — and could not do any of this before this plan ran. Every layer touched by Phase 1 has been wired end-to-end on one path. The reversibility of `animated_avatar_source` has been explicitly confirmed by the user before that string was written to committed code.
</success_criteria>

<output>
Create `.planning/phases/01-repository-foundation-build-system/01-01-SUMMARY.md` when done, per the summary template.
</output>
