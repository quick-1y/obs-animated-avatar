<!-- GSD:project-start source:PROJECT.md -->

## Project

**OBS Animated Avatar Plugin**

A native OBS Studio plugin that provides an animated 2D layered avatar as a first-class OBS Source. The avatar reacts in real time to keyboard input, mouse movement and clicks, and microphone audio amplitude — behaving like a simple animated character sitting at a computer. Users add it via Sources → Animated Avatar with no separate application required.

**Core Value:** A zero-overhead, privacy-safe native OBS avatar that reacts to user activity in real time without requiring a separate capture window, external application, or game engine.

### Constraints

- **Tech stack**: C++20 + CMake + libobs only; no Qt/game engine/Electron
- **Privacy**: keyboard events used for animation only; no keystroke content stored or transmitted
- **Performance**: plugin must not measurably impact OBS streaming performance; render path must never block on input/audio
- **Compatibility**: must work as a standard OBS plugin installable from a ZIP or installer
- **Platform**: Windows-first; no Linux/macOS code in MVP unless required by architecture
- **Dependencies**: minimize external deps; prefer vendored-small or header-only libraries

<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->

## Technology Stack

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

## OBS CMake Build System (Current)

### From `FindLibObs.cmake` to CMake Package Config

- OBS installs a proper **CMake Package Config** (`libobsConfig.cmake`) alongside its development headers when you install the OBS development SDK or build OBS from source.
- Plugins call `find_package(libobs REQUIRED)` — CMake resolves this via `CMAKE_PREFIX_PATH` pointing at the OBS install/SDK root.
- `FindLibObs.cmake` is considered legacy. Do not use it for new plugins targeting OBS 29+.

### `cmake-helpers` and `buildspec.json`

- **`buildspec.json`** lives at the repository root. It specifies OBS version pins, dependency versions, and platform-specific build settings. The CI/CD helper scripts read it to download the correct prebuilt OBS dev libraries (headers + import libs) from OBS's GitHub releases — so you do not need a full OBS source checkout to build a plugin on Windows.
- **`cmake-helpers`** (or `cmake/helpers/` in the template) is a submodule or vendored copy of shared OBS CMake utilities. It provides macros like `setup_plugin_target()`, target property helpers, packaging macros, and CPack configuration.
- Together they implement a "download-and-build" workflow: `cmake --preset windows-x64` drives downloading the OBS dev package pinned in `buildspec.json`, then builds the plugin against it.

### `obs_add_module()` and `setup_plugin_target()`

# In your CMakeLists.txt:

# Declare the plugin module

# Apply standard OBS plugin properties (RPATH, install dirs, etc.)

- `obs_add_module()` calls `add_library(... MODULE ...)` internally, sets the correct output name and suffix (`.dll` on Windows), and wires up linking to `libobs`.
- `setup_plugin_target()` sets install destinations, configures RPATH on Linux/macOS, and adds platform-specific compile definitions.
- Both macros are provided by the CMake modules that ship with the OBS dev package (found via `find_package(libobs)`).

### CMake Minimum Version

- `cmake_preset` support (3.20+)
- `FetchContent` with `FIND_PACKAGE_ARGS` (3.24)
- `target_sources(FILE_SET ...)` usage

### CMake Module Paths

# or use the preset system in obs-plugintemplate which handles this automatically

## OBS Development Headers

### Essential Includes for an Avatar Source Plugin

#include <obs-module.h>      // OBS_DECLARE_MODULE, obs_register_source, etc.
#include <obs-source.h>      // obs_source_info struct
#include <obs-data.h>        // settings get/set
#include <obs-properties.h>  // properties panel
#include <graphics/graphics.h>  // GPU texture/effect API
#include <util/platform.h>   // file I/O, paths

## Compiler & C++ Standard

| Requirement | Value |
|------------|-------|
| Compiler | MSVC (cl.exe) — clang-cl also works but MSVC is the primary supported compiler |
| Visual Studio | 2022 (17.x) minimum; OBS CI uses VS 2022 |
| C++ standard | C++17 minimum (OBS core); C++20 fully supported in plugin code with VS 2022 |
| `/std:c++20` | Required in CMake: `set(CMAKE_CXX_STANDARD 20)` `set(CMAKE_CXX_STANDARD_REQUIRED ON)` |
| Windows SDK | 10.0.20348.0 or later (Windows 11 SDK preferred) |

## Dependency Management

### OBS Ecosystem Convention

### Recommended for This Project

| Dependency | How to Vendor | Notes |
|-----------|--------------|-------|
| nlohmann/json | Single header (`deps/json.hpp`) | 1 file; `#include "deps/json.hpp"` |
| stb_image | Single header (`deps/stb_image.h`) | Define `STB_IMAGE_IMPLEMENTATION` in one .cpp |
| No other external deps needed | — | Win32 API for input, libobs for audio/graphics |

### What NOT to use

- **Qt** — PROJECT.md explicitly excludes it. OBS has Qt headers but plugins targeting audio/video sources don't need Qt.
- **vcpkg** — adds complexity without benefit given the header-only nature of needed deps.
- **Conan** — no OBS ecosystem adoption.
- **system package managers** — not portable across Windows dev setups.

## OBS Plugin Template

### What It Contains

### Key Behaviors

- Running `cmake --preset windows-x64` triggers a script that reads `buildspec.json`, downloads the pinned OBS dev package (headers + import libs) from GitHub releases, and configures the build.
- No need to build OBS from source for plugin development — the prebuilt dev package is sufficient.
- CI matrix produces `.zip` artifacts (Windows), `.tar.gz` (Linux), `.pkg` (macOS) ready for distribution.

### How to Use It for This Project

## OBS Version Targeting & API Compatibility

### Current Stable (as of training cutoff ~mid-2025)

- **OBS 31.x** was the latest stable as of early-mid 2025 (OBS 30 was stable in 2024, 31 released early 2025).
- TARGET: **OBS 30.0+** as the minimum supported version. This gives broad install base coverage while using the modern CMake package config API.

### Plugin API Versioning

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

## Windows Build Environment

| Tool | Required Version | Notes |
|------|-----------------|-------|
| Visual Studio | 2022 (17.x) | Community edition sufficient; install "Desktop development with C++" workload |
| Windows SDK | 10.0.20348.0+ | Included with VS 2022; needed for Raw Input, DirectX headers |
| CMake | 3.24+ | Install separately or use VS-bundled (check version: must be >= 3.24) |
| Git | 2.x | Required to clone template + submodules (`git submodule update --init`) |
| Python | 3.10+ | Used by some OBS build scripts; not required for plugin-only builds |
| 7-zip or similar | any | Template CI uses it for packaging; not required locally |

### Recommended Local Workflow

### Debug vs Release

- Use **RelWithDebInfo** during development — gives debuggable symbols without debug-heap overhead.
- Do not use Debug mode in the final plugin: OBS ships Release CRT; mixing CRT versions causes heap corruption.
- Link against the Release CRT even in dev builds: `/MD` not `/MDd`.

### DirectX / GPU Context

- OBS's graphics subsystem initializes DirectX 11 on Windows. The `gs_*` API abstracts this.
- Plugins do NOT create their own D3D11 device. All GPU work goes through `gs_*` calls made inside the `video_render` callback, which runs on OBS's graphics thread.
- Never call `gs_*` functions outside the render callback (or outside an `obs_enter_graphics()` / `obs_leave_graphics()` pair).

## Key Stack Decisions

- **Use `obs-plugintemplate` as the project scaffold.** It is the official starting point, handles CI, packaging, and the CMake package config plumbing. Do not reinvent it.
- **Target OBS 30.0+ minimum.** This covers the modern CMake package config API and has a large installed base. Pin the exact version in `buildspec.json`.
- **Use CMake 3.24 minimum, C++20, VS 2022.** These are aligned with OBS's own CI and give full C++20 feature access on Windows.
- **Do not use `FindLibObs.cmake`.** Use `find_package(libobs REQUIRED)` with `CMAKE_PREFIX_PATH` pointing at the dev package.
- **Vendor nlohmann/json and stb_image as single headers in `deps/`.** No package manager needed; both are stable, permissive-licensed, and trivial to update.
- **Do not use vcpkg or Conan.** No OBS ecosystem precedent; adds CI complexity with no benefit for this project's dep footprint.
- **Link against Release CRT (`/MD`) in all configurations.** Mixing `/MDd` with OBS's Release CRT causes heap corruption at runtime.
- **All GPU calls must occur on the OBS graphics thread** — inside `video_render` callback, or wrapped in `obs_enter_graphics()` / `obs_leave_graphics()`.
- **Use Raw Input for global keyboard/mouse capture** (research decision flagged for deeper investigation in the Windows Input phase — see PITFALLS.md).

## Confidence Gaps / Items Requiring Live Verification

## Sources

- `obsproject/obs-plugintemplate` — https://github.com/obsproject/obs-plugintemplate
- `obsproject/obs-studio` — https://github.com/obsproject/obs-studio
- OBS Plugin Development wiki — https://obsproject.com/wiki/Getting-Started-with-OBS-Plugin-Development
- OBS CMake README — https://github.com/obsproject/obs-studio/blob/master/cmake/README.md
- nlohmann/json — https://github.com/nlohmann/json (single-header release)
- stb — https://github.com/nothings/stb (stb_image.h)
- PROJECT.md constraints — confirmed: C++20, CMake, MSVC, Windows-first, no Qt/game engine, minimize deps

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
