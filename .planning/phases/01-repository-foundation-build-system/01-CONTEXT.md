# Phase 1: Repository Foundation & Build System - Context

**Gathered:** 2026-09-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver a compilable `obs-animated-avatar.dll` that OBS Studio loads cleanly and renders a solid-color placeholder rectangle when added as a source. The build system, CMake configuration, OBS linkage, CI scaffold, and vendored dependency layout must all be proven correct before any avatar logic is written. Nothing from later phases (rendering pipeline, character assets, animation, input) is included here.

</domain>

<decisions>
## Implementation Decisions

### Source Identity

- **D-01:** OBS source type ID: `animated_avatar_source` — **Reversibility:** one-way — This string is embedded in OBS scene `.json` files. Once users save scenes containing this source, renaming the ID orphans their settings. Must be chosen once and never changed.
- **D-02:** Display name in OBS Sources menu: `Animated Avatar` — **Reversibility:** reversible — Localizable string; can be changed without breaking scene files.
- **D-03:** Plugin module name / DLL filename: `obs-animated-avatar` → produces `obs-animated-avatar.dll` — **Reversibility:** costly — Changing the DLL name after distribution requires users to manually remove the old DLL; not impossible but disruptive.

### Repository Setup

- **D-04:** Use GitHub "Use as template" on `obsproject/obs-plugintemplate` — produces a fresh repo with a clean single initial commit and no template history. — **Reversibility:** reversible — Repo can always be restructured later.
- **D-05:** GitHub repository name: `obs-animated-avatar` — **Reversibility:** costly — Changing repo name after publishing breaks existing links, clone URLs, and any CI badge references.

### CI Configuration

- **D-06:** Phase 1 CI scope: Windows x64 build check only — configure + build + verify DLL exists. No artifact packaging, no test runner. — **Reversibility:** reversible — Packaging and test runner added in Phase 8.
- **D-07:** CI target: Windows x64 only. No x86 (OBS 29+ dropped 32-bit), no arm64 (OBS arm64 is experimental). — **Reversibility:** reversible — Additional targets added when OBS ecosystem validates them.

### Character Asset Path Model

- **D-08:** Character packs live at `{obs_data}/obs-plugins/obs-animated-avatar/characters/` at runtime — accessed via `obs_get_module_data_path()`. Follows OBS plugin data directory convention (same as `browser_source`, `image_source`). — **Reversibility:** costly — Changing the path after distribution requires users to move their character files; a migration guide would be needed.
- **D-09:** Phase 1 build output includes a minimal `characters/default/` placeholder directory with solid-color PNG stubs (not real artwork). Proves the asset pipeline path without requiring art production in Phase 1. — **Reversibility:** reversible — Placeholder replaced with real character assets in Phase 3.

### Claude's Discretion

- Exact solid-color placeholder implementation (1×1 pixel texture vs inline GS effect vs `gs_draw_sprite`) — planner chooses the simplest approach that proves the graphics context works.
- Exact CMake `install()` target layout for the DLL and `data/` directory — follow obs-plugintemplate defaults.
- `blog()` wrapper prefix string: `[obs-animated-avatar]` (follows DLL name convention).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Project Planning
- `.planning/PROJECT.md` — Core value, constraints (no Qt, no vcpkg, C++20, Windows-first, privacy boundary), architectural principle
- `.planning/REQUIREMENTS.md` — Phase 1 requirements: OBS-01, OBS-06, BUILD-01–05
- `.planning/ROADMAP.md` §Phase 1 — Detailed task list (1.1–1.12), files to create, technical decisions, validation criteria

### Research Findings
- `.planning/research/STACK.md` — OBS CMake build system (Package Config vs FindLibObs), obs-plugintemplate structure, buildspec.json schema, compiler/CRT requirements, dependency management approach
- `.planning/research/FEATURES.md` — `obs_source_info` mandatory fields, `OBS_SOURCE_CUSTOM_DRAW` flag, `obs_module_load`/`obs_module_unload` signatures, `obs_register_source` macro usage
- `.planning/research/PITFALLS.md` — `gs_*` outside graphics context (crash), `obs_source_info` struct size macro, `/MD` CRT requirement
- `.planning/research/SUMMARY.md` — Synthesized decisions and phase ordering rationale

### External (verify at implementation time)
- `https://github.com/obsproject/obs-plugintemplate` — Clone source; read README.md and buildspec.json schema before modifying
- `https://github.com/obsproject/obs-studio` — `libobs/obs-module.h`, `libobs/obs-source.h` for current struct field list

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- None — brand new project. obs-plugintemplate provides the scaffold.

### Established Patterns
- None yet — this phase establishes the patterns all subsequent phases follow.
- CMake pattern: `obs_add_module()` + `setup_plugin_target()` macros from obs-plugintemplate.
- Logging pattern: `blog(LOG_INFO, "[obs-animated-avatar] ...")` — set in this phase, used by all future phases.

### Integration Points
- `obs_module_load()` → registers `animated_avatar_source` via `obs_register_source()` — the single integration point from the plugin into OBS at module load time.
- `obs_module_data_path()` → base path for `characters/` discovery (Phase 3 will use this).

</code_context>

<specifics>
## Specific Ideas

- The ROADMAP.md Phase 1 task list is unusually complete and should be followed closely: tasks 1.1–1.12 are the explicit implementation spec for this phase.
- The placeholder render color in the roadmap is solid purple — acceptable; any visible non-black color that proves `video_render` is being called works.
- `src/plugin-support.h` with `blog()` wrappers should be created in Phase 1 so all subsequent phases have a consistent logging foundation.

</specifics>

<deferred>
## Deferred Ideas

- Windows arm64 CI target — deferred until OBS arm64 support is mainstream (Phase 8 or later).
- CPack artifact generation in CI — deferred to Phase 8 (Testing, Packaging & Release).
- clang-format enforcement in CI — deferred to Phase 8; coding conventions established during implementation phases first.
- Configurable character path override in OBS properties — the standard `obs_get_module_data_path()` path is sufficient for MVP; a configurable override path could be added in Phase 7 if users request it.
- Linux / macOS CI targets — platform abstraction is a Phase 6 concern; CI for non-Windows targets deferred until platform/ layer exists.

</deferred>

---

*Phase: 1-Repository Foundation & Build System*
*Context gathered: 2026-09-22*
