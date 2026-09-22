# Stack Research: OBS Animated Avatar Plugin

**Researched:** 2026-09-22
**Confidence:** MEDIUM — Bash/WebFetch/WebSearch tools were all denied in this session. Findings are based on training knowledge of the OBS plugin ecosystem (current to ~mid-2025) and corroborated by the project's own PROJECT.md. All claims that depend on OBS repository state at a specific commit are flagged LOW until verified against live sources.

---

## Recommended Stack

| Layer | Technology | Version | Rationale |
|-------|-----------|---------|-----------|
| Language | C++20 | ISO C++20 | OBS itself compiles at C++17 minimum, C++20 features usable in plugin code; required per PROJECT.md |
| Build system | CMake | 3.24+ | OBS plugin template and obs-cmake-modules require >= 3.20; 3.24 adds FetchContent improvements |
| Compiler (Win) | MSVC | VS 2022 (19.30+) | Required for reliable C++20 support; OBS CI uses VS 2022 |
| OBS target | OBS Studio | 30.x (latest stable) | Active release line; plugin API stable since ~29; binary-compat across 30.x |
| OBS linkage | libobs | Dynamic (.dll + .lib) | Plugins always link dynamically against libobs.dll via import lib |
| Plugin scaffold | obs-plugintemplate | current HEAD | Official OBS template; uses buildspec.json + cmake-helpers |
| JSON (config) | nlohmann/json | 3.11.x | Header-only, vendored single-header; standard in OBS ecosystem |
| Image loading | stb_image | latest | Header-only, vendored; standard for PNG loading in OBS plugins |
| Input (Windows) | Win32 Raw Input / WH_KEYBOARD_LL | OS built-in | No dep; covered by Windows SDK |
| Audio analysis | Custom RMS (libobs audio callback) | — | libobs supplies PCM frames; project computes RMS in-plugin |

---

## OBS CMake Build System (Current)

**Confidence: MEDIUM** — verified against OBS GitHub structure as known to training; confirm against current `obsproject/obs-plugintemplate` HEAD before coding.

### From `FindLibObs.cmake` to CMake Package Config

OBS migrated away from the legacy `FindLibObs.cmake` module pattern around OBS 28-29. The current approach (OBS 29+, definitely OBS 30) is:

- OBS installs a proper **CMake Package Config** (`libobsConfig.cmake`) alongside its development headers when you install the OBS development SDK or build OBS from source.
- Plugins call `find_package(libobs REQUIRED)` — CMake resolves this via `CMAKE_PREFIX_PATH` pointing at the OBS install/SDK root.
- `FindLibObs.cmake` is considered legacy. Do not use it for new plugins targeting OBS 29+.

### `cmake-helpers` and `buildspec.json`

Introduced with the modernized `obs-plugintemplate` (circa OBS 29-30):

- **`buildspec.json`** lives at the repository root. It specifies OBS version pins, dependency versions, and platform-specific build settings. The CI/CD helper scripts read it to download the correct prebuilt OBS dev libraries (headers + import libs) from OBS's GitHub releases — so you do not need a full OBS source checkout to build a plugin on Windows.
- **`cmake-helpers`** (or `cmake/helpers/` in the template) is a submodule or vendored copy of shared OBS CMake utilities. It provides macros like `setup_plugin_target()`, target property helpers, packaging macros, and CPack configuration.
- Together they implement a "download-and-build" workflow: `cmake --preset windows-x64` drives downloading the OBS dev package pinned in `buildspec.json`, then builds the plugin against it.

### `obs_add_module()` and `setup_plugin_target()`

```cmake
# In your CMakeLists.txt:
cmake_minimum_required(VERSION 3.24)
project(obs-animated-avatar VERSION 0.1.0)

find_package(libobs REQUIRED)

# Declare the plugin module
obs_add_module(obs-animated-avatar
  src/plugin-main.cpp
  src/avatar-source.cpp
  # ... other sources
)

# Apply standard OBS plugin properties (RPATH, install dirs, etc.)
setup_plugin_target(obs-animated-avatar)
```

- `obs_add_module()` calls `add_library(... MODULE ...)` internally, sets the correct output name and suffix (`.dll` on Windows), and wires up linking to `libobs`.
- `setup_plugin_target()` sets install destinations, configures RPATH on Linux/macOS, and adds platform-specific compile definitions.
- Both macros are provided by the CMake modules that ship with the OBS dev package (found via `find_package(libobs)`).

### CMake Minimum Version

The `obs-plugintemplate` as of OBS 30 requires **CMake 3.24** minimum. This is driven by:
- `cmake_preset` support (3.20+)
- `FetchContent` with `FIND_PACKAGE_ARGS` (3.24)
- `target_sources(FILE_SET ...)` usage

Use `cmake_minimum_required(VERSION 3.24)` in your `CMakeLists.txt`.

### CMake Module Paths

When building with the downloaded dev package, OBS CMake modules land at:
```
<obs-dev-package>/cmake/libobs/
  libobsConfig.cmake
  libobsTargets.cmake
  ObsPluginHelpers.cmake   # provides obs_add_module, setup_plugin_target
```

Point CMake at them with:
```bash
cmake -DCMAKE_PREFIX_PATH="<path-to-obs-dev>" ...
# or use the preset system in obs-plugintemplate which handles this automatically
```

---

## OBS Development Headers

**Confidence: MEDIUM**

When you install OBS dev libraries (via the template's `buildspec.json` download, or a manual OBS Studio SDK install), headers are at:

```
<obs-dev>/include/
  obs/
    obs.h                    # top-level include
    obs-module.h             # required for every plugin (OBS_DECLARE_MODULE etc.)
    obs-source.h             # obs_source_info, source lifecycle callbacks
    obs-data.h               # obs_data_t (settings/serialization)
    obs-properties.h         # obs_properties_t (UI properties panel)
    obs-audio-data.h         # audio capture callbacks, audio_output_get_info
    graphics/
      graphics.h             # gs_texture_t, gs_effect_t, gs_draw_*
      matrix4.h              # GPU math types
      vec2.h / vec3.h / vec4.h
    util/
      platform.h
      bmem.h                 # OBS memory allocator
      dstr.h
      darray.h
```

### Essential Includes for an Avatar Source Plugin

```cpp
#include <obs-module.h>      // OBS_DECLARE_MODULE, obs_register_source, etc.
#include <obs-source.h>      // obs_source_info struct
#include <obs-data.h>        // settings get/set
#include <obs-properties.h>  // properties panel
#include <graphics/graphics.h>  // GPU texture/effect API
#include <util/platform.h>   // file I/O, paths
```

The `OBS_MODULE_USE_DEFAULT_LOCALE` macro and `OBS_DECLARE_MODULE()` / `OBS_MODULE_AUTHOR()` must appear in exactly one translation unit (typically `plugin-main.cpp`).

---

## Compiler & C++ Standard

**Confidence: HIGH** — well-established OBS CI and documentation.

| Requirement | Value |
|------------|-------|
| Compiler | MSVC (cl.exe) — clang-cl also works but MSVC is the primary supported compiler |
| Visual Studio | 2022 (17.x) minimum; OBS CI uses VS 2022 |
| C++ standard | C++17 minimum (OBS core); C++20 fully supported in plugin code with VS 2022 |
| `/std:c++20` | Required in CMake: `set(CMAKE_CXX_STANDARD 20)` `set(CMAKE_CXX_STANDARD_REQUIRED ON)` |
| Windows SDK | 10.0.20348.0 or later (Windows 11 SDK preferred) |

OBS itself uses C++17 in its core, but OBS 29+ plugin code compiles under C++20 without issue. Features useful for this project: `std::span`, `std::format` (VS 2022 17.2+), designated initializers, `[[likely]]`/`[[unlikely]]`, `constexpr` improvements, concepts for template constraints on animation system.

Do not use C++23 features — VS 2022 support is incomplete and OBS CI does not validate it.

---

## Dependency Management

**Confidence: MEDIUM**

### OBS Ecosystem Convention

The OBS plugin ecosystem does **not** use vcpkg as a convention. The dominant pattern is:

1. **Header-only / single-header vendoring** — copy `nlohmann/json.hpp`, `stb_image.h`, etc. directly into `deps/` or `vendor/`. No build system integration needed.
2. **CMake FetchContent** — used by some plugins for larger dependencies (e.g., fetching a specific tag of a library). However, the template and official OBS plugins are shifting toward vendoring over network fetches in CI.
3. **`buildspec.json` + prebuilt packages** — the template mechanism for OBS itself and any deps the OBS team pre-packages (e.g., FFmpeg, OpenSSL for bundled plugins). Not appropriate for custom game-logic deps.
4. **vcpkg** — occasionally used by plugin authors but adds complexity and is not an OBS community standard. Avoid unless a dependency truly requires it.

### Recommended for This Project

| Dependency | How to Vendor | Notes |
|-----------|--------------|-------|
| nlohmann/json | Single header (`deps/json.hpp`) | 1 file; `#include "deps/json.hpp"` |
| stb_image | Single header (`deps/stb_image.h`) | Define `STB_IMAGE_IMPLEMENTATION` in one .cpp |
| No other external deps needed | — | Win32 API for input, libobs for audio/graphics |

Keep `deps/` checked into the repo. This avoids network calls in CI and keeps the build hermetic.

### What NOT to use

- **Qt** — PROJECT.md explicitly excludes it. OBS has Qt headers but plugins targeting audio/video sources don't need Qt.
- **vcpkg** — adds complexity without benefit given the header-only nature of needed deps.
- **Conan** — no OBS ecosystem adoption.
- **system package managers** — not portable across Windows dev setups.

---

## OBS Plugin Template

**Confidence: MEDIUM** — training knowledge; verify current template HEAD.

**Repository:** `https://github.com/obsproject/obs-plugintemplate`

### What It Contains

```
obs-plugintemplate/
  CMakeLists.txt              # Root; calls find_package(libobs), obs_add_module()
  buildspec.json              # OBS version pin + dep versions for CI download
  cmake/
    common/                   # Shared cmake helpers (or git submodule)
      ObsPluginHelpers.cmake
      CPackConfig.cmake
  .github/
    workflows/
      push.yaml               # CI: Linux, macOS, Windows builds
  src/
    plugin-main.cpp           # OBS_DECLARE_MODULE, module_load/unload
  data/
    locale/
      en-US.ini               # Localisation strings
  README.md
  .gitmodules                 # Points to cmake-helpers submodule
```

### Key Behaviors

- Running `cmake --preset windows-x64` triggers a script that reads `buildspec.json`, downloads the pinned OBS dev package (headers + import libs) from GitHub releases, and configures the build.
- No need to build OBS from source for plugin development — the prebuilt dev package is sufficient.
- CI matrix produces `.zip` artifacts (Windows), `.tar.gz` (Linux), `.pkg` (macOS) ready for distribution.

### How to Use It for This Project

1. Use the template as the scaffold (fork or copy).
2. Replace `plugin-main.cpp` with project-specific sources.
3. Pin `buildspec.json` to the target OBS stable version.
4. Add `deps/` directory with vendored headers.
5. Edit the root `CMakeLists.txt` to add source files and set `CMAKE_CXX_STANDARD 20`.

---

## OBS Version Targeting & API Compatibility

**Confidence: MEDIUM**

### Current Stable (as of training cutoff ~mid-2025)

- **OBS 31.x** was the latest stable as of early-mid 2025 (OBS 30 was stable in 2024, 31 released early 2025).
- TARGET: **OBS 30.0+** as the minimum supported version. This gives broad install base coverage while using the modern CMake package config API.

### Plugin API Versioning

OBS does not use a formal numbered plugin ABI version that plugins must declare matching numbers for. Instead:

- Plugins must be compiled against matching `libobs` headers — binary compatibility is between a plugin `.dll` and the `libobs.dll` it runs in.
- OBS uses `LIBOBS_API_VER` (a version integer) compiled into the plugin via `obs-module.h`. At load time OBS compares this against its own version; mismatches can cause load failure.
- This means: **compile against the OBS version you target**. A plugin built for OBS 30 may not load under OBS 29 if APIs changed.
- The safe approach: target the current stable and document the minimum OBS version in the plugin's README.

### API Stability

- **Source API** (`obs_source_info`, video/audio callbacks) — stable for years; safe to use.
- **Graphics API** (`gs_*`) — stable; minor additions per release but no breaking changes since OBS 26.
- **Properties API** — stable.
- **Audio API** (`obs_audio_data`, `audio_output_connect`) — stable; the async audio callback interface has not broken since OBS 22.
- **Signal/procedure system** — stable.

### Deprecation Risk

- `obs_source_output_video()` (push model) was softly deprecated in favor of the video-tick pull model; use `video_render` callback in `obs_source_info`.
- Raw audio callback via `obs_audio_info` is stable; `audio_output_connect` is the correct way to tap an audio source.

---

## Windows Build Environment

**Confidence: HIGH**

| Tool | Required Version | Notes |
|------|-----------------|-------|
| Visual Studio | 2022 (17.x) | Community edition sufficient; install "Desktop development with C++" workload |
| Windows SDK | 10.0.20348.0+ | Included with VS 2022; needed for Raw Input, DirectX headers |
| CMake | 3.24+ | Install separately or use VS-bundled (check version: must be >= 3.24) |
| Git | 2.x | Required to clone template + submodules (`git submodule update --init`) |
| Python | 3.10+ | Used by some OBS build scripts; not required for plugin-only builds |
| 7-zip or similar | any | Template CI uses it for packaging; not required locally |

### Recommended Local Workflow

```
1. Clone obs-plugintemplate (or project repo once bootstrapped)
2. git submodule update --init --recursive
3. cmake --preset windows-x64        # downloads OBS dev package, configures
4. cmake --build build --config RelWithDebInfo
5. Copy build/RelWithDebInfo/obs-animated-avatar.dll to
   %ProgramFiles%\obs-studio\obs-plugins\64bit\
   (or symlink for dev iteration)
6. Launch OBS; check log for plugin load message
```

### Debug vs Release

- Use **RelWithDebInfo** during development — gives debuggable symbols without debug-heap overhead.
- Do not use Debug mode in the final plugin: OBS ships Release CRT; mixing CRT versions causes heap corruption.
- Link against the Release CRT even in dev builds: `/MD` not `/MDd`.

### DirectX / GPU Context

- OBS's graphics subsystem initializes DirectX 11 on Windows. The `gs_*` API abstracts this.
- Plugins do NOT create their own D3D11 device. All GPU work goes through `gs_*` calls made inside the `video_render` callback, which runs on OBS's graphics thread.
- Never call `gs_*` functions outside the render callback (or outside an `obs_enter_graphics()` / `obs_leave_graphics()` pair).

---

## Key Stack Decisions

This research informs the following decisions:

- **Use `obs-plugintemplate` as the project scaffold.** It is the official starting point, handles CI, packaging, and the CMake package config plumbing. Do not reinvent it.
- **Target OBS 30.0+ minimum.** This covers the modern CMake package config API and has a large installed base. Pin the exact version in `buildspec.json`.
- **Use CMake 3.24 minimum, C++20, VS 2022.** These are aligned with OBS's own CI and give full C++20 feature access on Windows.
- **Do not use `FindLibObs.cmake`.** Use `find_package(libobs REQUIRED)` with `CMAKE_PREFIX_PATH` pointing at the dev package.
- **Vendor nlohmann/json and stb_image as single headers in `deps/`.** No package manager needed; both are stable, permissive-licensed, and trivial to update.
- **Do not use vcpkg or Conan.** No OBS ecosystem precedent; adds CI complexity with no benefit for this project's dep footprint.
- **Link against Release CRT (`/MD`) in all configurations.** Mixing `/MDd` with OBS's Release CRT causes heap corruption at runtime.
- **All GPU calls must occur on the OBS graphics thread** — inside `video_render` callback, or wrapped in `obs_enter_graphics()` / `obs_leave_graphics()`.
- **Use Raw Input for global keyboard/mouse capture** (research decision flagged for deeper investigation in the Windows Input phase — see PITFALLS.md).

---

## Confidence Gaps / Items Requiring Live Verification

The following should be confirmed against the live `obsproject/obs-plugintemplate` repository before the build-system phase begins:

1. Exact current OBS stable version number (was 30.x → 31.x by mid-2025; confirm latest).
2. Whether `cmake-helpers` is still a git submodule or has been inlined into the template.
3. Exact CMake module names: `ObsPluginHelpers.cmake` vs `helpers.cmake` — the macro names `obs_add_module` and `setup_plugin_target` are confirmed, but the file name may differ.
4. `buildspec.json` exact schema — particularly the key names for OBS version and Windows deps.
5. Whether OBS 31+ introduced any breaking changes to the source info struct fields.

---

## Sources

All sources are training-knowledge based (confidence: MEDIUM). No live fetches were possible in this session (Bash, WebFetch, WebSearch all permission-denied).

- `obsproject/obs-plugintemplate` — https://github.com/obsproject/obs-plugintemplate
- `obsproject/obs-studio` — https://github.com/obsproject/obs-studio
- OBS Plugin Development wiki — https://obsproject.com/wiki/Getting-Started-with-OBS-Plugin-Development
- OBS CMake README — https://github.com/obsproject/obs-studio/blob/master/cmake/README.md
- nlohmann/json — https://github.com/nlohmann/json (single-header release)
- stb — https://github.com/nothings/stb (stb_image.h)
- PROJECT.md constraints — confirmed: C++20, CMake, MSVC, Windows-first, no Qt/game engine, minimize deps
