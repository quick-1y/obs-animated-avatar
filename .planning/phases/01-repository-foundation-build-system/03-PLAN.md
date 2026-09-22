---
phase: 01-repository-foundation-build-system
plan: 03
type: execute
wave: 2
depends_on:
  - 01-01
files_modified:
  - data/characters/default/README.md
  - data/characters/default/placeholder.png
  - README.md
autonomous: true
requirements:
  - BUILD-01

estimate:
  tokens: 35000
  raw_tokens: 23000
  tasks: 2
  confidence: low

must_haves:
  truths:
    - "data/characters/default/ exists at repo root with a placeholder PNG stub and marker README, so the D-08 asset pipeline path is present in the packaged data/ tree (D-09)"
    - "No premature character.json stub is written (Phase 3 owns that schema)"
    - "README.md exists at repo root and documents: OBS version pin (31.1.1 per buildspec.json), obs-plugintemplate provenance, build command, DLL install path, data install path per D-08"
  artifacts:
    - "data/characters/default/placeholder.png (small solid-color PNG stub — D-09)"
    - "data/characters/default/README.md (marker file explaining the default character pack is a placeholder for Phase 3)"
    - "README.md (initial — build instructions, OBS version pin, install paths per D-08, obs-plugintemplate SHA)"
  key_links:
    - "data/characters/default/ tree ↔ CMake template's target_install_resources copying data/ into the install layout ↔ runtime path {obs_data}/obs-plugins/obs-animated-avatar/characters/default/ per D-08 (missing tree means Phase 3 character discovery has nothing to scan)"
    - "README.md OBS version doc ↔ buildspec.json pinned obs-studio version 31.1.1 (mismatch confuses users about supported OBS)"
    - "README.md install paths ↔ Plan 1 task 01-01-09 manual install steps ↔ D-08 runtime lookup path (all three must describe the same directory tree)"
---

<objective>
**Documentation and asset-pipeline scaffolding**: two independent horizontal expansion tasks that share no files with Plan 2 and can run in parallel with it (both are Wave 2, both `depends_on: [01-01]` only). Creates the `data/characters/default/` placeholder pack that D-09 requires (proves the D-08 asset pipeline path lands correctly in the install layout) and writes the initial `README.md` that the Phase 1 Definition of Done requires.

Purpose: Neither task touches runtime code — they scaffold the data-directory shape and document what has shipped, both required to close out Phase 1 per its ROADMAP.md Definition of Done ("...CI is green, and a minimum-viable README documents the build process") and per CONTEXT.md D-09.

Output:
- `data/characters/default/placeholder.png` + `data/characters/default/README.md` (D-09 placeholder pack)
- `README.md` at repo root (initial project README documenting build, OBS version, install path)
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
@.planning/phases/01-repository-foundation-build-system/01-01-SUMMARY.md

# Source files this plan reads/writes:
@buildspec.json
@cmake/windows/helpers.cmake
</context>

<tasks>

<task type="auto" tdd="false">
  <name>Task 01-03-01: Create placeholder character pack at data/characters/default/ (D-09)</name>
  <files>data/characters/default/placeholder.png, data/characters/default/README.md</files>
  <precondition>Repo root already has a git-tracked `data/locale/en-US.ini` (verified via `git ls-files data/`); the template's `target_install_resources` in `cmake/windows/helpers.cmake` copies the whole `data/` tree into the install layout at build time, so the `characters/default/` subtree lands under `{rundir}/data/obs-plugins/obs-animated-avatar/characters/default/` and, after user install (per Plan 1 task 09), under `{obs_data}/obs-plugins/obs-animated-avatar/characters/default/` — which is exactly the runtime path D-08 mandates for `obs_get_module_data_path()`.</precondition>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-CONTEXT.md (D-08, D-09), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Architecture Patterns (Recommended Project Structure — data/characters/default/), cmake/windows/helpers.cmake (target_install_resources behavior)</read_first>
  <action>Create the directory `data/characters/default/` at repo root. Add two files: (1) `data/characters/default/placeholder.png` — a small solid-color PNG stub (32x32 or 64x64 is fine; not real character art). Generate it programmatically (e.g. a one-off Python `PIL.Image.new('RGBA', (32,32), (128,0,128,255)).save(...)` invocation, or any existing small solid-color PNG copied in). Do NOT attempt to draw real avatar art — that is Phase 3's job. (2) `data/characters/default/README.md` with a single paragraph stating: "This directory is a placeholder for the default character pack (D-09, Phase 1). The real character.json + textures ship in Phase 3. On install this tree lands under {obs_data}/obs-plugins/obs-animated-avatar/characters/default/ per D-08 and is discoverable via obs_get_module_data_path()." Do NOT create a `character.json` file — Phase 3 defines that schema and creating a stub now would encode an implicit contract that Phase 3 has not decided yet.</action>
  <verify>
    <automated>test -d data/characters/default</automated>
    <fails_when>non-zero exit — the placeholder directory was not created</fails_when>
    <automated>test -f data/characters/default/placeholder.png &amp;&amp; test -f data/characters/default/README.md</automated>
    <fails_when>non-zero exit — one or both required placeholder files are missing</fails_when>
    <automated>file data/characters/default/placeholder.png 2>&amp;1 | grep -c 'PNG image'</automated>
    <fails_when>output is less than 1 — the placeholder.png file is not a valid PNG per libmagic detection; Phase 3 stb_image loading test would fail against this</fails_when>
    <automated>! test -f data/characters/default/character.json</automated>
    <fails_when>non-zero exit — a character.json stub was created despite the action explicitly forbidding it; Phase 3 owns that schema</fails_when>
  </verify>
  <done>data/characters/default/ exists at repo root with a valid placeholder PNG and a README explaining its placeholder status. No character.json premature-schema file created. Directory will be copied by the template's `target_install_resources` into the plugin install tree at build time.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-03-02: Write initial README.md documenting build, OBS version, install path</name>
  <files>README.md</files>
  <read_first>buildspec.json (for OBS version pin 31.1.1 and project identity fields), .planning/phases/01-repository-foundation-build-system/01-CONTEXT.md (D-01, D-03, D-08), .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Environment Availability (VS2022, CMake, OBS 32.2.2 install path)</read_first>
  <action>Create `README.md` at repo root with these sections (minimum-viable per phase Definition of Done): (1) Title `# Animated Avatar Plugin for OBS` and one-line description from PROJECT.md core value; (2) `## Requirements` listing OBS Studio 30.0 or later (documented compatibility floor), Windows 10/11 x64, Visual Studio 2022 (17.x) with C++ Desktop workload, CMake 3.28+ on PATH, Git with submodule support; (3) `## Building` with the two-command sequence `cmake --preset windows-x64` then `cmake --build --preset windows-x64` and a note that the first configure downloads OBS 31.1.1 sources (~100-300MB) — that OBS version pin is the one currently in `buildspec.json`; (4) `## Installation` documenting the copy paths — DLL goes to `C:/Program Files/obs-studio/obs-plugins/64bit/obs-animated-avatar.dll`, data goes to `C:/Program Files/obs-studio/data/obs-plugins/obs-animated-avatar/` (D-08 — matches `obs_get_module_data_path()` runtime lookup); (5) `## Provenance` recording that the project was scaffolded from `obsproject/obs-plugintemplate` — record the current SHA by running `git rev-parse HEAD` at the initial template import commit and paste the value (if that commit is no longer discoverable via git log, note "initial import commit SHA — see repo history"). Do not invent an author name or a real GitHub URL — leave the buildspec.json placeholders (`Your Name`, `YOUR_USERNAME`) as-is with a `## TODO` note listing what the user must personalize before release.</action>
  <verify>
    <automated>test -f README.md</automated>
    <fails_when>non-zero exit — README.md was not created</fails_when>
    <automated>grep -c 'cmake --preset windows-x64' README.md</automated>
    <fails_when>output is less than 1 — the build command is not documented; Phase 1 Definition of Done ("minimum-viable README documents the build process") is not met</fails_when>
    <automated>grep -c '31\.1\.1' README.md</automated>
    <fails_when>output is less than 1 — the pinned OBS version (buildspec.json obs-studio 31.1.1) is not called out; users cannot correlate their installed OBS version with what the plugin builds against</fails_when>
    <automated>grep -c 'obs-plugins/64bit' README.md</automated>
    <fails_when>output is less than 1 — the DLL install path (D-08 runtime convention) is not documented</fails_when>
  </verify>
  <done>README.md at repo root documents requirements, build command, install paths for both DLL and data/, and records obs-plugintemplate provenance. Placeholder identity fields explicitly marked TODO rather than fabricated.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Placeholder PNG author ↔ CMake `target_install_resources` copy step | The PNG is generated locally (PIL or copied-in) and shipped verbatim into the install tree — trivially trusted content, but marks the trust boundary between source-controlled data and installed data |
| README.md ↔ user reading it to install the DLL | Documented install path becomes a de facto instruction; if the path is wrong, users install to the wrong location and the plugin appears not to load |

## STRIDE Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|-----------|----------|-----------|----------|-------------|-----------------|
| T-03-01 | Tampering | data/characters/default/placeholder.png shipping arbitrary content in install tree | low | accept | The PNG is a locally-generated solid-color stub; content is inert bitmap data. It will be replaced in Phase 3 by real character art with its own review. Verify gate confirms it is a valid PNG (not swappable for arbitrary bytes without breaking `file` detection). |
| T-03-02 | Information Disclosure | README documents install paths | low | accept | Install paths are already publicly known (OBS convention); documenting them creates no new disclosure. Documenting them incorrectly is a usability issue, not a security issue. |
| T-03-03 | Spoofing | README leaves buildspec.json author/website as placeholders | low | mitigate | Task 01-03-02 explicitly forbids fabricating author name or GitHub URL, and requires a `## TODO` section listing what must be personalized before release — prevents accidentally publishing a release with template placeholders posing as real identity. |
| T-03-SC | Tampering | npm/pip/cargo installs | n/a | accept | No npm/pip/cargo installs in this phase (Python PIL usage in task 01-03-01 is a one-off local tool invocation, not a project dependency). |
</threat_model>

<verification>
1. `data/characters/default/placeholder.png` is a valid PNG (per libmagic); `data/characters/default/README.md` exists (D-09)
2. No `data/characters/default/character.json` file (Phase 3 owns that schema)
3. `README.md` at repo root documents the build command `cmake --preset windows-x64`, the OBS version pin `31.1.1`, and the DLL install path `obs-plugins/64bit`
</verification>

<success_criteria>
D-09 placeholder character tree exists at the correct path and gets copied by `target_install_resources` into the install layout at build time. README.md meets the Phase 1 Definition of Done "minimum-viable README documents the build process" clause. Together with Plans 01 and 02, Phase 1 is complete.
</success_criteria>

<output>
Create `.planning/phases/01-repository-foundation-build-system/01-03-SUMMARY.md` when done, per the summary template.
</output>
