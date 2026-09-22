# Phase 1: Repository Foundation & Build System — Research

**Researched:** 2026-09-22
**Domain:** OBS Studio C++ plugin scaffold — CMake build system, obs-plugintemplate, libobs linkage, CI
**Confidence:** MEDIUM (template structure verified via live GitHub fetch; OBS API fields from FEATURES.md training knowledge)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** OBS source type ID: `animated_avatar_source` — one-way — embedded in OBS scene `.json`; never change after first release.
- **D-02:** Display name: `Animated Avatar` — reversible.
- **D-03:** Plugin module name / DLL filename: `obs-animated-avatar` → `obs-animated-avatar.dll` — costly to change after distribution.
- **D-04:** Use GitHub "Use as template" on `obsproject/obs-plugintemplate` — fresh repo, single initial commit, no template history.
- **D-05:** GitHub repository name: `obs-animated-avatar` — costly to change after publishing.
- **D-06:** Phase 1 CI scope: Windows x64 build check only — configure + build + verify DLL exists. No artifact packaging, no test runner.
- **D-07:** CI target: Windows x64 only. No x86, no arm64.
- **D-08:** Character packs live at `{obs_data}/obs-plugins/obs-animated-avatar/characters/` at runtime — via `obs_get_module_data_path()`.
- **D-09:** Phase 1 build output includes a minimal `characters/default/` placeholder directory with solid-color PNG stubs.

### Claude's Discretion

- Exact solid-color placeholder implementation (1×1 pixel texture vs inline GS effect vs `gs_draw_sprite`).
- Exact CMake `install()` target layout — follow obs-plugintemplate defaults.
- `blog()` wrapper prefix string: `[obs-animated-avatar]` (follows DLL name convention).

### Deferred Ideas (OUT OF SCOPE)

- Windows arm64 CI target
- CPack artifact generation in CI
- clang-format enforcement in CI
- Configurable character path override in OBS properties
- Linux / macOS CI targets
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| OBS-01 | Plugin registers as a native OBS Source using `obs_source_info` with `OBS_SOURCE_TYPE_INPUT \| OBS_SOURCE_VIDEO \| OBS_SOURCE_CUSTOM_DRAW` | Standard Stack §Core, Code Examples §obs_source_info Skeleton |
| OBS-06 | Plugin loads and unloads cleanly via `obs_module_load` / `obs_module_unload` without crashing OBS | Code Examples §plugin-main.cpp Pattern, Pitfalls §1.3 (struct size macro) |
| BUILD-01 | Plugin builds on Windows with VS 2022, CMake 3.24+, C++20, using `obs-plugintemplate` scaffold and `buildspec.json` | Standard Stack §Build System, Architecture Patterns §CMakeLists.txt |
| BUILD-02 | `cmake --preset windows-x64` produces a `.dll` that OBS 30.0+ can load | Standard Stack, Environment Availability, Architecture Patterns §Preset Workflow |
| BUILD-03 | Platform-specific code isolated behind `platform/` abstraction — engine core has no `#include <windows.h>` | Architecture Patterns §Project Structure |
| BUILD-04 | Debug and Release supported; no CRT mismatch (`/MD` both configs) | Common Pitfalls §CRT Mismatch |
| BUILD-05 | Build produces versioned ZIP artifact (Phase 8 concern — deferred; Phase 1 CI only confirms DLL exists) | Architecture Patterns §CI Workflow |
</phase_requirements>

---

## Summary

Phase 1 delivers a compilable OBS plugin skeleton. The authoritative scaffold is `obsproject/obs-plugintemplate` on GitHub master branch. As of the live fetch performed in this research session, the template pins **OBS 31.1.1** in `buildspec.json`, which means the Phase 1 build will compile against OBS 31.1.1 sources — the plugin will run under OBS 30.0+ as a minimum but the compiled binary will be built against 31.x headers. The installed OBS on the developer machine is **32.2.2**, which is newer than what buildspec.json pins; the plugin DLL still loads as long as `LIBOBS_API_VER` matches.

The build system is significantly different from the legacy pattern documented in prior research (STACK.md). The modern template **does not use prebuilt `.lib` files**. Instead, `cmake --preset windows-x64` triggers a CMake configure-time download of OBS 31.1.1 source archives from GitHub, builds the OBS core libraries (specifically the frontend API and libobs import targets) locally in a `deps/` subdirectory, then links the plugin against those locally-built targets. `find_package(libobs)` is satisfied by the locally-built CMake package config. The macro names that wire everything together are **`set_target_properties_plugin()`** (from `cmake/windows/helpers.cmake`) and the MODULE library target created directly via `add_library(... MODULE ...)` in the root `CMakeLists.txt` — the `obs_add_module()` macro from prior research does NOT exist in the current template.

The template's `src/` contains three files: `plugin-main.c` (entry points), `plugin-support.h` (logging declarations), and `plugin-support.c.in` (generated at configure time with `[PLUGIN_NAME]` prefix substitution). Phase 1 adds `src/avatar-source.cpp` (the `obs_source_info` registration), and renames the project-level metadata in `buildspec.json` and `CMakeLists.txt`. The placeholder render should be a 1×1 purple `gs_texture_t` drawn via `gs_draw_sprite` inside `video_render` — the simplest pattern that exercises the full render callback chain.

**Primary recommendation:** Clone the template, verify `cmake --preset windows-x64` succeeds immediately before modifying any files, then rename/replace incrementally (task 1.6 before 1.7).

---

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Plugin DLL registration with OBS | OBS Integration Layer | — | `obs_module_load` / `obs_register_source` are OBS API calls; no engine involvement |
| `obs_source_info` lifecycle callbacks | OBS Integration Layer | Avatar Engine (future) | Callbacks are OBS-bound; engine data structures live behind them |
| GPU render (placeholder rectangle) | OBS Graphics Thread (video_render) | — | All `gs_*` calls must be in `video_render` or under `obs_enter_graphics()` |
| Build system / CMake configuration | Build Layer | — | `CMakeLists.txt`, `buildspec.json`, presets are build-time artifacts only |
| CI workflow | Build Layer | — | GitHub Actions produces the DLL; no runtime responsibility |
| Vendored headers (nlohmann/json, stb_image) | Build Layer | — | Header-only; no runtime linking |
| Logging (blog/obs_log wrappers) | OBS Integration Layer | — | Wraps OBS `blog()` — must be compiled into the OBS module context |

---

## Standard Stack

### Core

| Library / Tool | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| obs-plugintemplate | master HEAD | Project scaffold, CI, CMake presets | Official OBS project template; handles all boilerplate |
| OBS Studio sources | 31.1.1 (buildspec.json pin) [CITED: github.com/obsproject/obs-plugintemplate] | libobs headers + import targets downloaded at configure time | Template pins this; building against it satisfies `LIBOBS_API_VER` for OBS 31.x |
| CMake | 3.28 minimum (template) [CITED: github.com/obsproject/obs-plugintemplate] | Build configuration | `cmake_minimum_required(VERSION 3.28...3.30)` in template; VS-bundled 3.29.5 satisfies it |
| MSVC | 14.42.34433 (VS2022 Community, confirmed installed) [VERIFIED: /d/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/bin/HostX64/x64/cl.exe] | C++ compiler | Only OBS-certified compiler for Windows; required for `/MD` CRT alignment |
| C++ standard | C++20 | Language features | Set in CMakeLists.txt; supported by MSVC 14.42 |
| nlohmann/json | 3.11.x [ASSUMED] | JSON (future phases); vendor now to prove dep layout | Single-header; standard in OBS ecosystem |
| stb_image | latest (no versioning) [ASSUMED] | PNG loading (future phases); vendor now to prove dep layout | Single-header; used by OBS's own plugins |

### Supporting

| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| GitHub Actions `windows-2022` runner | — | CI execution | Template uses `runs-on: windows-2022` for Windows builds [CITED: github.com/obsproject/obs-plugintemplate] |
| Windows SDK | 10.0.22621 (preset architecture field) [CITED: CMakePresets.json windows-x64 preset] | Windows headers | Included with VS2022; needed for any Win32 calls in future phases |

### No External Package Managers

No vcpkg, Conan, or any package manager. All deps are either built by the template's bootstrap process (OBS itself) or vendored as single headers.

### Installation (after cloning template)

```bash
# From the cloned repo root:
git submodule update --init --recursive
cmake --preset windows-x64
cmake --build --preset windows-x64
```

The configure step downloads OBS 31.1.1 sources (~2.8 MB compressed) and pre-built deps, builds libobs, then configures the plugin. First run takes several minutes; subsequent runs use cached archives.

---

## Package Legitimacy Audit

This phase installs no npm/PyPI/crates packages. All dependencies are:
- `obsproject/obs-plugintemplate` — official OBS GitHub organization repository
- `nlohmann/json` and `stb_image` — vendored as single header files; no package manager installation

**No Package Legitimacy Gate required for this phase.**

---

## Architecture Patterns

### System Architecture Diagram

```
  Developer machine
  ┌────────────────────────────────────────────────────────────────┐
  │  cmake --preset windows-x64                                    │
  │    │                                                           │
  │    ▼                                                           │
  │  buildspec_common.cmake downloads:                            │
  │    ├─ OBS Studio 31.1.1 sources (GitHub archive)              │
  │    ├─ prebuilt obs-deps (2025-07-11)                           │
  │    └─ prebuilt Qt6 (2025-07-11, not used for this plugin)     │
  │    │                                                           │
  │    ▼                                                           │
  │  OBS sources built locally → libobs CMake package config      │
  │  CMAKE_PREFIX_PATH set to include obs dep dir                 │
  │    │                                                           │
  │    ▼                                                           │
  │  add_library(obs-animated-avatar MODULE)                      │
  │    → links OBS::libobs                                        │
  │    → set_target_properties_plugin()                           │
  │    │                                                           │
  │    ▼                                                           │
  │  obs-animated-avatar.dll                                       │
  │    │                                                           │
  │    ▼                                                           │
  │  Copy to C:/Program Files/obs-studio/obs-plugins/64bit/       │
  │    │                                                           │
  │    ▼                                                           │
  │  OBS Studio 32.2.2 loads plugin → obs_module_load()          │
  │    → obs_register_source(&avatar_source_info)                 │
  │    │                                                           │
  │    ▼                                                           │
  │  Sources → Add → Animated Avatar                              │
  │    → video_render: gs_draw_sprite(purple_1x1_tex)             │
  │    → 320×240 solid purple rectangle in OBS preview            │
  └────────────────────────────────────────────────────────────────┘
```

### Recommended Project Structure

```
obs-animated-avatar/
├── CMakeLists.txt           # project(), add_library(MODULE), C++20, target_sources, include deps/
├── buildspec.json           # MUST update: name, displayName, version, author (OBS version stays 31.1.1)
├── CMakePresets.json        # Use as-is from template (windows-x64 preset confirmed valid)
├── .github/
│   └── workflows/
│       ├── build-project.yaml   # Template's main CI (keep as-is)
│       └── push.yaml            # Template's push CI (keep as-is; add artifact check)
├── cmake/
│   ├── common/              # Template cmake helpers — do NOT modify
│   └── windows/             # Template windows helpers — do NOT modify
├── src/
│   ├── plugin-main.cpp      # OBS_DECLARE_MODULE, obs_module_load, obs_module_unload
│   ├── plugin-support.h     # Logging declarations (from template, adapt for C++)
│   ├── plugin-support.cpp   # obs_log() definition (rename from .c.in, adjust for C++)
│   └── avatar-source.cpp    # obs_source_info registration + placeholder render
├── deps/
│   ├── nlohmann/
│   │   └── json.hpp         # vendored single header
│   └── stb/
│       └── stb_image.h      # vendored single header
└── data/
    ├── locale/
    │   └── en-US.ini        # Localisation strings (required by OBS_MODULE_USE_DEFAULT_LOCALE)
    └── characters/
        └── default/         # D-09: placeholder directory with solid-color PNG stubs
```

**Note on plugin-support files:** The template ships `plugin-support.c.in` (a CMake-configured C file). For a C++20 project, rename the plugin-main and plugin-support files to `.cpp` and change the `target_sources` call accordingly. The template's `add_library(... MODULE ...)` will work with either `.c` or `.cpp` sources.

### Pattern 1: Root CMakeLists.txt for obs-animated-avatar

```cmake
# Source: obsproject/obs-plugintemplate CMakeLists.txt (live fetch 2026-09-22)
# Adapted for C++20 and renamed project

cmake_minimum_required(VERSION 3.28...3.30)

# bootstrap.cmake sets CMAKE_MODULE_PATH and reads buildspec.json metadata
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(obs-animated-avatar VERSION 0.1.0)

# Platform-specific CMake modules (sets CMAKE_PREFIX_PATH after deps download)
option(ENABLE_FRONTEND_API "Use OBS Frontend API" OFF)
option(ENABLE_QT "Use Qt6" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

# C++20 requirement
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Plugin target — MODULE = shared library without import lib
add_library(obs-animated-avatar MODULE)

# Sources
target_sources(obs-animated-avatar PRIVATE
  src/plugin-main.cpp
  src/plugin-support.cpp
  src/avatar-source.cpp
)

# Vendored headers
target_include_directories(obs-animated-avatar PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/deps"
)

# Link against libobs (target provided by find_package(libobs) in buildspec chain)
target_link_libraries(obs-animated-avatar PRIVATE OBS::libobs)

# Apply OBS plugin install layout and PDB handling
set_target_properties_plugin(obs-animated-avatar PROPERTIES
  OUTPUT_NAME obs-animated-avatar
)
```

**Key difference from prior research:** `obs_add_module()` does NOT exist in the current template. The template uses `add_library(... MODULE ...)` directly, then `set_target_properties_plugin()` for installation config. Do not use `obs_add_module()`.

### Pattern 2: plugin-main.cpp

```cpp
// Source: obsproject/obs-plugintemplate src/plugin-main.c (live fetch 2026-09-22)
// Translated to C++20

#include <obs-module.h>
#include "plugin-support.h"

// Forward declaration from avatar-source.cpp
void register_avatar_source();

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
    register_avatar_source();
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "plugin unloaded");
}
```

### Pattern 3: plugin-support.h

```cpp
// Source: obsproject/obs-plugintemplate src/plugin-support.h (live fetch 2026-09-22)
// Adapted for C++20 (extern "C" guards already present in template)

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern const char *PLUGIN_NAME;
extern const char *PLUGIN_VERSION;

void obs_log(int log_level, const char *format, ...);

#ifdef __cplusplus
}
#endif
```

`PLUGIN_NAME` and `PLUGIN_VERSION` are populated at configure time from `buildspec.json` via CMake's `configure_file` on `plugin-support.c.in` → `plugin-support.cpp`. The resulting prefix for all log lines is `[obs-animated-avatar]` (driven by the `name` field in `buildspec.json`).

### Pattern 4: obs_source_info Skeleton (avatar-source.cpp)

```cpp
// Source: FEATURES.md (training knowledge, OBS 30.x); struct verified via
// https://raw.githubusercontent.com/obsproject/obs-studio/master/libobs/obs-source.h (session fetch)

#include <obs-module.h>
#include "plugin-support.h"

struct AvatarSourceContext {
    obs_source_t *source;
    uint32_t width  = 320;
    uint32_t height = 240;
    gs_texture_t *placeholder_tex = nullptr;  // 1×1 purple texture
};

static const char *avatar_get_name(void * /* type_data */)
{
    return obs_module_text("AnimatedAvatar");  // key in en-US.ini
}

static void *avatar_create(obs_data_t *settings, obs_source_t *source)
{
    auto *ctx = new AvatarSourceContext;
    ctx->source = source;

    // Create 1×1 purple RGBA texture — MUST be inside graphics lock
    obs_enter_graphics();
    uint8_t purple[4] = {128, 0, 128, 255};   // R G B A — straight alpha
    const uint8_t *ptr = purple;
    ctx->placeholder_tex = gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0);
    obs_leave_graphics();

    obs_log(LOG_INFO, "[init] avatar source created");
    return ctx;
}

static void avatar_destroy(void *data)
{
    auto *ctx = static_cast<AvatarSourceContext *>(data);
    // GPU resource destruction MUST be inside graphics lock
    obs_enter_graphics();
    gs_texture_destroy(ctx->placeholder_tex);
    obs_leave_graphics();
    delete ctx;
}

static uint32_t avatar_get_width(void *data)
{
    return static_cast<AvatarSourceContext *>(data)->width;
}

static uint32_t avatar_get_height(void *data)
{
    return static_cast<AvatarSourceContext *>(data)->height;
}

static void avatar_render(void *data, gs_effect_t * /* effect */)
{
    auto *ctx = static_cast<AvatarSourceContext *>(data);
    if (!ctx->placeholder_tex) return;

    // Use built-in default effect for textured sprite rendering
    gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_eparam_t *param = gs_effect_get_param_by_name(eff, "image");
    gs_effect_set_texture(param, ctx->placeholder_tex);

    while (gs_effect_loop(eff, "Draw")) {
        gs_draw_sprite(ctx->placeholder_tex, 0, ctx->width, ctx->height);
    }
}

static obs_properties_t *avatar_get_properties(void * /* data */)
{
    obs_properties_t *props = obs_properties_create();
    // Phase 1: empty properties panel — proves OBS properties call works
    return props;
}

static void avatar_get_defaults(obs_data_t * /* settings */)
{
    // Phase 1: no settings yet
}

// *** CRITICAL: use the macro, not manual obs_register_source_s ***
// The macro computes sizeof(obs_source_info) at compile time against
// current SDK headers, ensuring struct size field is correct.
static struct obs_source_info avatar_source_info = {
    .id           = "animated_avatar_source",  // D-01: ONE-WAY DOOR
    .type         = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
    .get_name     = avatar_get_name,
    .create       = avatar_create,
    .destroy      = avatar_destroy,
    .get_width    = avatar_get_width,
    .get_height   = avatar_get_height,
    .video_render = avatar_render,
    .get_properties = avatar_get_properties,
    .get_defaults   = avatar_get_defaults,
    // icon_type, version, unversioned_id — leave as zero/NULL
    // All other callbacks: NULL (OBS checks before calling)
};

void register_avatar_source()
{
    obs_register_source(&avatar_source_info);
}
```

### Pattern 5: buildspec.json Fields to Update

```json
{
    "dependencies": {
        "obs-studio": {
            "version": "31.1.1",
            "baseUrl": "https://github.com/obsproject/obs-studio/archive/refs/tags",
            "label": "OBS sources",
            "hashes": {
                "macos": "...",
                "windows-x64": "..."
            }
        },
        "prebuilt": { ... },
        "qt6": { ... }
    },
    "platformConfig": {
        "macos": {
            "bundleId": "com.example.obs-animated-avatar"
        }
    },
    "name": "obs-animated-avatar",
    "displayName": "Animated Avatar Plugin for OBS",
    "version": "0.1.0",
    "author": "Your Name",
    "website": "https://github.com/YOUR_USERNAME/obs-animated-avatar",
    "email": "your@email.com"
}
```

**Do NOT change `dependencies.obs-studio.version` or the `hashes` in Phase 1.** These are verified checksums. Only update the `name`, `displayName`, `version`, `author`, `website`, `email`, and `platformConfig.macos.bundleId` fields. The OBS version stays at `31.1.1` as pinned.

### Pattern 6: GitHub Actions CI Workflow (Simplified for Phase 1)

The template's `push.yaml` delegates to `build-project.yaml` via reusable workflow and `build-plugin` actions. For Phase 1's scoped CI (D-06: Windows x64 build check only, no packaging), use the template's existing `push.yaml` as-is. It already triggers the full build-project workflow which includes Windows x64. The only Phase 1 customization needed is ensuring the repo name matches `buildspec.json`.

If a minimal standalone workflow is preferred over the template's reusable workflow structure:

```yaml
# .github/workflows/build.yml — Phase 1 minimal CI
name: Build (Windows x64)

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build-windows:
    name: Windows x64
    runs-on: windows-2022   # Matches template's runner
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Configure
        run: cmake --preset windows-x64

      - name: Build
        run: cmake --build --preset windows-x64

      - name: Verify DLL exists
        shell: pwsh
        run: |
          $spec = Get-Content buildspec.json | ConvertFrom-Json
          $dll = "release/$($spec.name)-$($spec.version)-windows-x64/" +
                 "obs-plugins/64bit/$($spec.name).dll"
          if (-not (Test-Path $dll)) {
            Write-Error "DLL not found at $dll"
            exit 1
          }
```

**Note:** The template's existing workflow is more robust (handles artifact packaging, hash verification, multi-platform). Recommendation: keep the template's `build-project.yaml` intact; D-06 scoping is automatically satisfied because the template's Windows job already does configure + build, and packaging is a separate optional step.

### Anti-Patterns to Avoid

- **Using `obs_add_module()`:** This macro does NOT exist in the current template (as of live fetch 2026-09-22). Prior research in STACK.md incorrectly documented it. Use `add_library(... MODULE ...)` + `set_target_properties_plugin()`.
- **Using `FindLibObs.cmake`:** Deprecated for OBS 29+. The template's `buildspec_common.cmake` sets `CMAKE_PREFIX_PATH` to the locally-built OBS deps so `find_package(libobs)` resolves automatically.
- **Calling `gs_*` without graphics lock in `create()`:** Crash. Always `obs_enter_graphics()` / `obs_leave_graphics()` around texture creation.
- **Storing `obs_data_t *` pointer:** Dangling pointer after `create()` returns. Read all values immediately into your own structs.
- **Mixing `/MDd` and `/MD` CRT:** The template defaults to `RelWithDebInfo` build type, which uses `/MD`. Never switch to `Debug` config for final testing — OBS ships `/MD` CRT.
- **Calling `obs_register_source_s()` manually:** Always use the `obs_register_source()` macro which computes `sizeof(obs_source_info)` from current headers.
- **Modifying OBS version or hash in buildspec.json:** Only change project metadata fields; leave the OBS dependency block intact.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| OBS CMake integration | Custom CMakeLists.txt from scratch | obs-plugintemplate scaffold + cmake/windows/helpers.cmake | `set_target_properties_plugin()` handles install paths, PDB files, and rundir structure correctly |
| OBS dev package download | Manual download scripts | buildspec.json + buildspec_common.cmake bootstrap | Template's download mechanism handles SHA256 verification and extraction |
| Module entry point boilerplate | Custom `__declspec(dllexport)` tables | `OBS_DECLARE_MODULE()` macro | Macro generates the exact export table OBS's `LoadLibrary` looks for |
| Log prefix formatting | Custom sprintf with prefix | `obs_log()` from plugin-support.c.in | Configure-time substitution of `PLUGIN_NAME` ensures consistent prefix |
| Struct size for source registration | Hardcoded `sizeof` value | `obs_register_source(info)` macro | Macro recomputes against current SDK headers; future-proofs against struct growth |

**Key insight:** The template's cmake plumbing (buildspec → cmake/common → cmake/windows) is a carefully maintained system. Deviating from it by writing custom CMake breaks the CI workflow and the install layout that OBS expects.

---

## Runtime State Inventory

This is a greenfield phase — no existing runtime state to inventory. No databases, no live service configs, no OS-registered items, no secrets/env vars, no build artifacts from previous installs.

**Note:** After Phase 1 delivers the DLL, subsequent phases must account for the OBS plugin directory as a deployment target. But that is a future concern.

---

## Common Pitfalls

### Pitfall 1: `obs_add_module()` Does Not Exist in Current Template

**What goes wrong:** CMake configure error — "unknown CMake command obs_add_module".
**Why it happens:** Prior documentation (STACK.md, CLAUDE.md) references `obs_add_module()` as the standard macro. The current template (live-verified 2026-09-22) uses `add_library(... MODULE ...)` directly; `obs_add_module` is a macro that existed in older obs-cmake-modules versions and is not present in the current cmake/windows/helpers.cmake.
**How to avoid:** Use `add_library(obs-animated-avatar MODULE)` + `set_target_properties_plugin(obs-animated-avatar PROPERTIES ...)` as shown in Pattern 1.
**Warning signs:** `CMake Error: Unknown CMake command "obs_add_module"` during configure.

### Pitfall 2: CRT Mismatch (`/MDd` vs `/MD`)

**What goes wrong:** Heap corruption — allocations from the plugin's heap freed by OBS's heap (or vice versa), causing random crashes.
**Why it happens:** OBS ships with the `/MD` (MSVC Release CRT). If the plugin is compiled with `/MDd` (Debug CRT), the two CRT heaps are different allocators with different internal structures. Any `obs_data_t *`, `obs_source_t *`, or `char *` allocated by OBS and freed by the plugin (or the reverse) corrupts the heap.
**How to avoid:** The template defaults to `RelWithDebInfo` build type which uses `/MD`. Never build with `Debug` configuration. The `cmake --preset windows-x64` preset sets `RelWithDebInfo` by default — do not override it.
**Warning signs:** Random heap corruption crashes, `_ASSERT` failures in CRT code, or `HEAP[obs-animated-avatar.dll]: Invalid address specified to RtlFreeHeap` in the debugger.

### Pitfall 3: `gs_*` Called Without Graphics Context

**What goes wrong:** Access violation crash in OBS graphics thread — D3D11 context is thread-affine.
**Why it happens:** OBS's DirectX 11 device is created and owned by the OBS render thread. Calling any `gs_*` function from another thread (including `create()`, `update()`, or a background thread) without acquiring the graphics lock causes the D3D11 device context to be accessed from the wrong thread.
**How to avoid:** In `create()` and `destroy()`, wrap all `gs_*` calls in `obs_enter_graphics()` / `obs_leave_graphics()`. In `video_render()`, the context is already held — call `gs_*` freely.
**Warning signs:** Crash in `gs_texture_create` or `gs_texture_destroy`; OBS crash log showing access violation in d3d11 or libobs-d3d11.dll.

### Pitfall 4: OBS Source ID Is a One-Way Door

**What goes wrong:** Scene files referencing `animated_avatar_source` load but display as "[Missing source]" after an ID rename.
**Why it happens:** OBS serializes the source type ID into scene `.json` files on disk. When a user saves their scene, the ID is embedded. Renaming the ID in a future release causes OBS to be unable to find the registered source for that ID, resulting in missing/broken sources in saved scenes.
**How to avoid:** `animated_avatar_source` is the locked ID per D-01. It must be set in Phase 1 and never changed. If a rename is ever needed, use `unversioned_id` in `obs_source_info` to register the old ID as a legacy alias.
**Warning signs:** None at compile time — this failure mode only manifests in user-reported issues after distribution.

### Pitfall 5: buildspec.json Name Must Match DLL Output Name

**What goes wrong:** The CI artifact upload step looks for a DLL at `release/{name}-{version}-windows-x64/obs-plugins/64bit/{name}.dll`. If `buildspec.json`'s `name` field does not match the CMake `OUTPUT_NAME` property, the artifact upload fails silently (no DLL found at expected path).
**Why it happens:** The template's CI reads `buildspec.json` to determine both artifact naming and the expected DLL path.
**How to avoid:** Set `buildspec.json` `name` to `obs-animated-avatar` AND set `set_target_properties_plugin(obs-animated-avatar PROPERTIES OUTPUT_NAME obs-animated-avatar)`. They must match exactly.
**Warning signs:** CI step "Upload Artifacts" succeeds but uploads 0 files, or "Verify DLL exists" step fails.

### Pitfall 6: OBS Version Mismatch at Runtime

**What goes wrong:** Plugin fails to load with OBS log error `"[obs-animated-avatar.dll] Invalid module (old version?)"`.
**Why it happens:** `LIBOBS_API_VER` is a compile-time constant from `obs-module.h` that encodes the OBS API version. OBS compares the plugin's compiled-in version against its own at load time. A plugin built against OBS 31.1.1 headers will have `LIBOBS_API_VER` for 31.1.1. OBS 32.2.2 (currently installed) should still load it if the API version check passes — OBS checks major version compatibility, not exact patch version. However, if the developer upgrades OBS significantly after building, re-pinning and re-building against a newer OBS version may be required.
**How to avoid:** Document the minimum OBS version in `README.md`. For development iteration, the currently installed OBS 32.2.2 should be fine loading a plugin built against 31.1.1, but verify at task 1.5.
**Warning signs:** `Invalid module` message in OBS log on plugin load.

### Pitfall 7: Submodule Init Required

**What goes wrong:** CMake configure fails with `include() of non-existent file cmake/common/bootstrap.cmake`.
**Why it happens:** The `cmake/common/` directory may be a git submodule (or the bootstrap.cmake may not exist if the template was cloned without submodules). Shallow clones via GitHub "Use as template" may not initialize submodules automatically.
**How to avoid:** After creating the repo from template and cloning locally, always run `git submodule update --init --recursive` before any CMake commands.
**Warning signs:** `cmake/common/bootstrap.cmake` is missing or the `cmake/common/` directory is empty after clone.

---

## Code Examples

### Verified Template Structure (live fetch 2026-09-22)

The following is confirmed from the live template repository:

- `plugin-support.c.in` defines `obs_log()` as: `snprintf(template, length, "[%s] %s", PLUGIN_NAME, format); blogva(log_level, template, args);`
- `plugin-support.h` declares `obs_log()` and `blogva()` with `extern "C"` guards — compatible with C++
- `plugin-main.c` uses `obs_log(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION)` and `obs_log(LOG_INFO, "plugin unloaded")` — no `blog()` macro, only `obs_log()`
- CMake generator for windows-x64 preset: `"Visual Studio 17 2022"` [CITED: CMakePresets.json]
- Windows SDK version field: `"x64,version=10.0.22621"` [CITED: CMakePresets.json]
- Build preset config: `RelWithDebInfo` [CITED: CMakePresets.json]

### Correct obs_register_source Pattern

```c
// CORRECT: macro form — computes sizeof at compile time
obs_register_source(&avatar_source_info);

// WRONG: never call the _s variant manually
// obs_register_source_s(&avatar_source_info, 42);  // hardcoded size = CRASH
```

### Texture Creation + Destruction Pattern

```cpp
// In create() — must use graphics lock:
obs_enter_graphics();
const uint8_t purple[4] = {128, 0, 128, 255};
const uint8_t *ptr = purple;
ctx->tex = gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0);
obs_leave_graphics();

// In destroy() — must use graphics lock:
obs_enter_graphics();
gs_texture_destroy(ctx->tex);
ctx->tex = nullptr;
obs_leave_graphics();

// In video_render() — no lock needed (already on render thread):
gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
gs_eparam_t *p   = gs_effect_get_param_by_name(eff, "image");
gs_effect_set_texture(p, ctx->tex);
while (gs_effect_loop(eff, "Draw")) {
    gs_draw_sprite(ctx->tex, 0, ctx->width, ctx->height);
}
```

### data/locale/en-US.ini

```ini
AnimatedAvatar=Animated Avatar
```

This file must exist for `obs_module_text("AnimatedAvatar")` to return `"Animated Avatar"` in `avatar_get_name()`. Without it, OBS falls back to the key string `"AnimatedAvatar"` — functional but not localized.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `obs_add_module()` macro | `add_library(MODULE)` + `set_target_properties_plugin()` | Prior to 2025 template modernization | Must use new pattern; old macro does not exist |
| `FindLibObs.cmake` | `find_package(libobs)` via CMake Package Config | OBS 28-29 era | Do not use Find module; buildspec chain auto-sets prefix path |
| Prebuilt OBS `.lib` in release download | OBS sources downloaded + built at configure time | OBS 30+ template | First configure takes longer (downloads and builds OBS) |
| `plugin-main.c` (C) | `plugin-main.cpp` (C++) | Configurable — template ships `.c`; rename to `.cpp` for C++20 | Must add `.cpp` explicitly in `target_sources` and ensure `extern "C"` on exported symbols |

**Deprecated/outdated:**
- `obs_add_module()`: Does not exist in current template's cmake/windows/helpers.cmake (live-verified 2026-09-22).
- `FindLibObs.cmake`: Legacy module; deprecated since OBS 29.
- `push.yaml` vs `build-project.yaml` naming: Template now uses a reusable workflow split; `push.yaml` calls `build-project.yaml`.

---

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | nlohmann/json version is 3.11.x | Standard Stack | Wrong version tag used when vendoring; pin to latest release tag instead |
| A2 | `obs_add_module()` is absent in all current forks of the template | Anti-Patterns | Some forks may still provide it; this research only verified the official obsproject/obs-plugintemplate master |
| A3 | OBS 32.2.2 (installed) will successfully load a plugin compiled against OBS 31.1.1 headers | Common Pitfalls §6 | Plugin fails to load; buildspec.json must be updated to pin OBS 32.x |
| A4 | `GS_RGBA` is the correct color format for a 4-byte RGBA pixel on the local machine | Code Examples | Purple texture appears wrong color; may need `GS_BGRA` on some D3D11 hardware |
| A5 | The `plugin-support.c.in` → `plugin-support.cpp` rename in CMakeLists.txt is sufficient for C++ compilation | Architecture Patterns | May require additional CMake `configure_file()` update if the .in extension is matched explicitly |

---

## Open Questions

1. **OBS 31.1.1 plugin DLL loading under OBS 32.2.2**
   - What we know: OBS uses `LIBOBS_API_VER` for compatibility checks; minor version upgrades usually load fine.
   - What's unclear: Whether the 31→32 major version change introduces an ABI break that prevents load.
   - Recommendation: Task 1.5 (copy DLL, launch OBS, check log) is the definitive test. If load fails with "Invalid module", update `buildspec.json` to pin OBS 32.x and re-run task 1.3-1.4.

2. **Does `plugin-support.c.in` need a CMake `configure_file()` call?**
   - What we know: The filename ends in `.in`, suggesting CMake template substitution for `@CMAKE_PROJECT_NAME@` and `@CMAKE_PROJECT_VERSION@`.
   - What's unclear: The root `CMakeLists.txt` WebFetch showed `target_sources` referencing `src/plugin-main.c` without a `configure_file` call visible — it may be in the platform helpers or implicit.
   - Recommendation: Read the template's `CMakeLists.txt` directly after clone (task 1.1) and look for `configure_file(src/plugin-support.c.in ...)`. If absent, add it.

3. **Exact `find_package` call for libobs in root CMakeLists.txt**
   - What we know: `CMAKE_PREFIX_PATH` is set by `buildspec_common.cmake` to the locally-built OBS dep directory. `find_package(libobs)` is expected to work via the CMake Package Config files in that directory.
   - What's unclear: Whether the root CMakeLists.txt calls `find_package(libobs REQUIRED)` explicitly, or whether it is triggered implicitly via `target_link_libraries(... OBS::libobs)`.
   - Recommendation: Read the actual `CMakeLists.txt` after clone at task 1.1 and verify.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Git | Template clone, submodules | Yes | 2.45.1.windows.1 | — |
| Visual Studio 2022 Community | C++ compilation | Yes [VERIFIED: /d/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/] | 14.42.34433 (VS 17.x) | — |
| MSVC cl.exe | C++20 compilation | Yes [VERIFIED: ...bin/HostX64/x64/cl.exe] | 14.42 | — |
| CMake (VS-bundled) | Build system | Yes [VERIFIED: ...CMake/CMake/bin/cmake.exe] | 3.29.5-msvc4 | — |
| Python 3 | Some OBS build scripts | Yes | 3.14.4 | Not required for plugin builds |
| OBS Studio (runtime) | Plugin load test (task 1.5) | Yes | 32.2.2 (installed at C:/Program Files/obs-studio) | — |
| OBS dev headers | Build (libobs API) | No — will be downloaded by buildspec at configure time | Downloaded from GitHub at first `cmake --preset windows-x64` | — |
| Internet access at configure time | buildspec.json dep download | Required | — | Must be available for first configure; subsequent builds use cached archives |

**Missing dependencies with no fallback:**
- Internet access during first `cmake --preset windows-x64`: The template downloads ~100-300MB of OBS sources and prebuilt deps. No offline fallback is built into the template.

**Missing dependencies with fallback:**
- None beyond the above.

**Environment risk: OBS version mismatch.** The developer's installed OBS is 32.2.2, while buildspec.json pins 31.1.1. The plugin DLL will be built against 31.1.1 headers. Task 1.5 (load test) will immediately reveal if this causes a load failure.

---

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None in Phase 1 (greenfield; test infrastructure is Phase 8) |
| Config file | None — Wave 0 creates no test files in Phase 1 |
| Quick run command | `cmake --preset windows-x64 && cmake --build --preset windows-x64` |
| Full suite command | Manual: launch OBS, add source, verify purple rectangle, delete source |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| OBS-01 | Source appears in Sources → Add → Animated Avatar | manual-only | n/a — requires OBS running | N/A |
| OBS-06 | `obs_module_load` returns true; `[obs-animated-avatar]` in OBS log | manual-only | n/a — requires OBS running | N/A |
| BUILD-01 | `cmake --preset windows-x64` exits 0 | smoke | `cmake --preset windows-x64` | N/A |
| BUILD-02 | `obs-animated-avatar.dll` produced | smoke | `cmake --build --preset windows-x64` | N/A |
| BUILD-03 | `src/avatar-source.cpp` has no `#include <windows.h>` | static analysis | `grep -r "windows.h" src/` | N/A |
| BUILD-04 | No `/MDd` flag in build | smoke (check cmake flags) | `cmake --preset windows-x64 -- -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` | N/A |
| BUILD-05 | Deferred to Phase 8 | — | — | N/A |

### Sampling Rate

- **Per task commit:** `cmake --preset windows-x64 && cmake --build --preset windows-x64` (confirms no CMake regression)
- **Per wave merge:** Full build + manual OBS load test
- **Phase gate:** OBS 32.2.2 loads plugin cleanly; purple rectangle visible; source add/delete without crash

### Wave 0 Gaps

- No test infrastructure needed for Phase 1 — validation is build-based (cmake exit code) and manual (OBS load test). Phase 8 adds unit tests.

---

## Security Domain

`security_enforcement: true`, `security_asvs_level: 1`, `security_block_on: high`

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | Plugin has no auth; no user accounts |
| V3 Session Management | No | No sessions |
| V4 Access Control | No | No access control surface in Phase 1 |
| V5 Input Validation | Partial — `obs_data_t` string values read in `update()` | Use null-check on `obs_data_get_string()` results; never use in filesystem paths without validation |
| V6 Cryptography | No | No crypto in Phase 1 |

### Known Threat Patterns for OBS C++ Plugin

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Null pointer from `obs_data_get_string()` | Tampering | Always null-check before use; OBS returns empty string `""` not NULL for unset string fields |
| Path traversal via plugin data path | Elevation of Privilege | Use `obs_module_file()` or `obs_module_data_path()` — these are sandboxed to the plugin's data dir |
| DLL planting (adversarial `.dll` in OBS plugins dir) | Elevation of Privilege | Installation instructions must specify official OBS plugins directory; no mitigation needed in plugin code itself |
| Use-after-free via `obs_source_t *` stored beyond `destroy()` | Tampering | Do not store `obs_source_t *` beyond the source's lifecycle; release in `destroy()` |

**Phase 1 security posture:** This phase creates the module entry point, a minimal `obs_source_info`, and a placeholder render. The attack surface is minimal. No file I/O, no network calls, no user input parsing beyond `obs_data_t`. The graphics context rule (all `gs_*` inside lock) is both a correctness and stability requirement that doubles as a security control against GPU state corruption.

---

## Sources

### Primary (verified live via GitHub API/raw fetch, 2026-09-22)

- [obsproject/obs-plugintemplate buildspec.json](https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/buildspec.json) — OBS version 31.1.1, dep versions
- [obsproject/obs-plugintemplate CMakePresets.json](https://raw.githubusercontent.com/obsproject/obs-plugintemplate) — preset names, generator, SDK version
- [obsproject/obs-plugintemplate cmake/windows/helpers.cmake](https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/cmake/windows/helpers.cmake) — `set_target_properties_plugin()` confirmed; `obs_add_module` absent
- [obsproject/obs-plugintemplate src/plugin-main.c](https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/src/plugin-main.c) — `obs_log()` usage, no `obs_register_source`
- [obsproject/obs-plugintemplate src/plugin-support.c.in](https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/src/plugin-support.c.in) — logging prefix pattern `[PLUGIN_NAME]`
- [obsproject/obs-plugintemplate src/plugin-support.h](https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/src/plugin-support.h) — `obs_log()` declaration, `extern "C"` guards

### Secondary (MEDIUM confidence — training knowledge, OBS 30.x)

- `.planning/research/FEATURES.md` — `obs_source_info` struct fields, `OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`, callback signatures
- `.planning/research/PITFALLS.md` — Graphics context rules (live-verified), CRT mismatch pattern
- `.planning/research/STACK.md` — Build system overview (partially superseded by live fetch findings)
- [OBS Plugin Development Docs](https://docs.obsproject.com/plugins) — `obs_source_info` field descriptions

### Tertiary (LOW confidence — WebSearch)

- [OBS Studio 31.1.x release timeline](https://obs-versions.com/archive) — confirms 31.1.0 released July 2025, 31.1.1 July 12 2025, 31.1.2 July 28 2025

---

## Metadata

**Confidence breakdown:**
- Standard Stack: MEDIUM — template structure live-verified; API fields from prior training-knowledge research
- Architecture: MEDIUM-HIGH — CMake pattern live-verified; `obs_source_info` from training knowledge
- Pitfalls: HIGH for `obs_add_module` absence (live-verified), CRT and graphics context (prior live research); MEDIUM for others
- Environment: HIGH — VS2022/MSVC/CMake verified via filesystem

**Research date:** 2026-09-22
**Valid until:** 2027-03-22 (6 months — OBS plugin template is actively maintained; re-verify `buildspec.json` before any fresh clone)

---

## RESEARCH COMPLETE
