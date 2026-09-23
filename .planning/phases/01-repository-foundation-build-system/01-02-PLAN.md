---
phase: 01-repository-foundation-build-system
plan: 02
type: execute
wave: 2
depends_on:
  - 01-01
files_modified:
  - deps/nlohmann/json.hpp
  - deps/stb/stb_image.h
  - .github/workflows/build-project.yaml
  - .github/workflows/push.yaml
autonomous: true
requirements:
  - BUILD-01
  - BUILD-02

estimate:
  tokens: 40000
  raw_tokens: 27000
  tasks: 2
  confidence: low

must_haves:
  truths:
    - "deps/nlohmann/json.hpp exists and #include \"nlohmann/json.hpp\" compiles from a Phase 1 source (proves dep include path)"
    - "deps/stb/stb_image.h exists and matches the upstream stb_image.h public header (proves Phase 2 texture-loading unblocker)"
    - "cmake --preset windows-x64 && cmake --build --preset windows-x64 still succeeds after deps are dropped in (BUILD-01, BUILD-02 — non-regression)"
    - "GitHub Actions push workflow completes green on the initial push to main, Windows x64 job only (BUILD-01, BUILD-02 in CI; D-06, D-07)"
  artifacts:
    - "deps/nlohmann/json.hpp (vendored single header — nlohmann/json 3.11.x per RESEARCH.md A1)"
    - "deps/stb/stb_image.h (vendored single header — stb latest)"
  key_links:
    - "CMakeLists.txt target_include_directories(... deps) [Plan 1 task 01-01-07] ↔ deps/nlohmann/json.hpp on disk (missing dir breaks Phase 3 character JSON parsing)"
    - "obs-plugintemplate's existing .github/workflows/push.yaml ↔ build-project.yaml reusable workflow ↔ GitHub Actions windows-2022 runner (D-06 CI scope: build check only, no packaging)"
---

<objective>
**Build-scaffolding expansion**: two horizontal expansion tasks that build on the proven tracer from Plan 1. Vendors the single-header dependencies into `deps/` (the include path was already registered in the tracer's CMakeLists.txt update — this plan just populates the directory), and verifies that the obs-plugintemplate's existing CI workflow satisfies D-06 (Windows x64 build check only) and D-07 (no macOS/Ubuntu jobs).

Purpose: These two tasks share no files with each other or with Plan 3. Deps unblock Phase 2 (stb_image texture loading) and Phase 3 (nlohmann/json character parsing). CI verification is a one-time D-06/D-07 conformance check on the template's shipped workflow, not a new workflow.

Output:
- `deps/nlohmann/json.hpp`, `deps/stb/stb_image.h` (vendored single headers)
- GitHub Actions run passes green on Windows x64 only (D-06, D-07 conformance)
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

# Source files that may be read/modified in this plan:
@buildspec.json
@.github/workflows/push.yaml
@.github/workflows/build-project.yaml
@CMakeLists.txt
</context>

<tasks>

<task type="auto" tdd="false">
  <name>Task 01-02-01: Vendor nlohmann/json.hpp and stb_image.h into deps/</name>
  <files>deps/nlohmann/json.hpp, deps/stb/stb_image.h</files>
  <precondition>Plan 01-01 committed (`deps/` include path is already registered in CMakeLists.txt from task 01-01-07). Network access to https://github.com is required for the two downloads; there is no offline fallback for the initial vendor.</precondition>
  <read_first>.planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Standard Stack (nlohmann/json 3.11.x, stb_image latest), §Assumptions Log (A1), .planning/phases/01-repository-foundation-build-system/01-PATTERNS.md §No Analog Found (deps download URLs)</read_first>
  <action>Create the two directories `deps/nlohmann/` and `deps/stb/`. Download `json.hpp` from the latest v3.11.x tag release asset at https://github.com/nlohmann/json/releases (single-header release — file is named `json.hpp`, ~950KB) into `deps/nlohmann/json.hpp`. Download `stb_image.h` from https://raw.githubusercontent.com/nothings/stb/master/stb_image.h into `deps/stb/stb_image.h`. Do NOT define `STB_IMAGE_IMPLEMENTATION` in this file — that macro is defined once in a Phase 2 .cpp translation unit that consumes the header. After downloading, run a smoke reconfigure + build (`cmake --preset windows-x64 &amp;&amp; cmake --build --preset windows-x64`) to confirm the tracer still compiles cleanly with `deps/` populated. If the smoke build fails, halt and surface the exact compiler error — do NOT modify CMakeLists.txt to work around it (Plan 1's include path is correct; a failure here means something else regressed).</action>
  <verify>
    <automated>test -f deps/nlohmann/json.hpp &amp;&amp; test -f deps/stb/stb_image.h</automated>
    <fails_when>non-zero exit — one or both vendored headers are missing</fails_when>
    <automated>head -20 deps/nlohmann/json.hpp | grep -c 'nlohmann'</automated>
    <fails_when>output is less than 1 — the vendored file does not identify itself as nlohmann/json (likely downloaded the wrong file or a redirect page)</fails_when>
    <automated>head -30 deps/stb/stb_image.h | grep -c 'stb_image'</automated>
    <fails_when>output is less than 1 — the vendored file does not identify itself as stb_image.h</fails_when>
    <automated>cmake --build --preset windows-x64 2>&amp;1 | tail -5</automated>
    <fails_when>non-zero exit, or the substring "error C" or "LNK" or "FAILED" appears in the tailed output — dropping in deps must not regress the tracer build</fails_when>
  </verify>
  <done>Both vendored headers present under `deps/`; smoke rebuild passes; no regression to Plan 1's tracer build.</done>
</task>

<task type="auto" tdd="false">
  <name>Task 01-02-02: Confirm CI scope matches D-06 + D-07 (Windows x64 only, build only) and trigger a green run</name>
  <files>.github/workflows/build-project.yaml (verify only — edit only if non-Windows jobs must be disabled per D-07), .github/workflows/push.yaml (verify only)</files>
  <precondition>Repository is pushed to a GitHub remote configured to run Actions on the `main` branch; the template's `.github/actions/build-plugin/action.yaml` and helper scripts are present (verified via `git ls-files .github/`). Task 01-02-01 committed so the CI checkout has the vendored deps in place.</precondition>
  <read_first>.github/workflows/push.yaml, .github/workflows/build-project.yaml, .planning/phases/01-repository-foundation-build-system/01-RESEARCH.md §Pattern 6 GitHub Actions CI Workflow, §Common Pitfalls (Pitfall 5 — buildspec.json name must match DLL output)</read_first>
  <action>Per RESEARCH.md Pattern 6 recommendation ("keep the template's build-project.yaml intact; D-06 scoping is automatically satisfied because the template's Windows job already does configure + build, and packaging is a separate optional step"). Do NOT rewrite the workflow; instead: (1) read `.github/workflows/push.yaml` and `.github/workflows/build-project.yaml` to confirm the Windows x64 job is present and runs on `windows-2022`; (2) confirm no macOS or Ubuntu jobs run on push (D-07: Windows x64 only) — if they do, disable them by editing only the `on:`/`jobs:` filter section, never the build steps themselves; (3) commit the vendored deps and push to `main`; (4) monitor the resulting Actions run at `https://github.com/{owner}/obs-animated-avatar/actions` (URL derived from buildspec.json `website` field once user updates it). If the run fails, capture the failing step + first error line and surface it — do not patch around a CI failure without understanding it.</action>
  <verify>
    <automated>test -f .github/workflows/push.yaml &amp;&amp; test -f .github/workflows/build-project.yaml</automated>
    <fails_when>non-zero exit — the template's CI workflow files are not present (something went wrong during initial template checkout; halt and re-verify Plan 1's precondition)</fails_when>
    <automated>grep -c 'windows-2022' .github/workflows/build-project.yaml</automated>
    <fails_when>output is less than 1 — the Windows x64 job is not configured to run on windows-2022 as documented in RESEARCH.md Pattern 6 (D-06/D-07 not satisfied by the template CI)</fails_when>
    <human-check>The most recent GitHub Actions push workflow run on the `main` branch shows "success" for the Windows x64 build job. The run was triggered by the push that committed task 01-02-01; the Windows job includes both the configure step and the build step and both exited 0. No macOS or Ubuntu jobs ran, or they were explicitly disabled with a documented D-07 note in the workflow YAML.</human-check>
  </verify>
  <done>Template's push workflow runs green on the Windows x64 matrix. D-06 (build check only) and D-07 (Windows x64 only) both hold. No custom workflow YAML was written; the template's own machinery is doing the job as RESEARCH.md recommended.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Local dev machine ↔ github.com download of vendored single headers | `json.hpp` and `stb_image.h` are fetched from external URLs — supply-chain risk if the source repos are hijacked or the download is MITMed |
| GitHub Actions runner ↔ external dep resolution | The CI job re-runs `cmake --preset windows-x64` and re-downloads OBS sources per buildspec.json hashes — same threat surface as T-01-02 in Plan 1 |

## STRIDE Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation Plan |
|-----------|----------|-----------|----------|-------------|-----------------|
| T-02-01 | Tampering | deps/nlohmann/json.hpp and deps/stb/stb_image.h vendored downloads | medium | mitigate | Task 01-02-01's verify gates check the file identity via `grep 'nlohmann'` in json.hpp header and `grep 'stb_image'` in stb_image.h header — catches trivially wrong downloads (HTML error pages, redirects, unrelated files). Future tightening: SHA256-pin both files in a follow-up commit once versions are frozen. |
| T-02-02 | Tampering | CI workflow YAML edits | medium | accept | This plan does not modify workflow YAML meaningfully — task 01-02-02 permits only surgical `on:`/`jobs:` filter edits per D-07, never build-step edits. Any workflow edit beyond that scope is out of scope for this plan and would require its own threat review. |
| T-02-SC | Tampering | npm/pip/cargo installs | n/a | accept | No npm/pip/cargo installs in this phase. Vendored single-header downloads are covered under T-02-01. |
</threat_model>

<verification>
1. `deps/nlohmann/json.hpp` and `deps/stb/stb_image.h` exist and identify themselves via header content grep
2. `cmake --preset windows-x64 && cmake --build --preset windows-x64` still succeeds (tracer non-regression)
3. Most recent GitHub Actions push run on `main` reports Windows x64 job success and no macOS/Ubuntu job ran (D-07)
</verification>

<success_criteria>
Deps directory is populated (unblocking Phase 2 stb_image and Phase 3 nlohmann/json). Template CI passes green on Windows x64 with no non-Windows jobs running (D-06 + D-07 conformance verified).
</success_criteria>

<output>
Create `.planning/phases/01-repository-foundation-build-system/01-02-SUMMARY.md` when done, per the summary template.
</output>
