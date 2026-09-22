# Project Research Summary: OBS Animated Avatar Plugin

**Project:** OBS Animated Avatar Plugin
**Domain:** OBS Studio C++ plugin — layered 2D avatar rendering with reactive animation
**Researched:** 2026-09-22
**Confidence:** MEDIUM (OBS API findings based on training knowledge through mid-2025; Windows input APIs partially VERIFIED via live fetch)

---

## Executive Summary

This project is a native OBS Studio plugin (`.dll` on Windows) that registers a custom video source rendering a layered 2D avatar character. The avatar reacts in real time to three inputs: global keyboard/mouse activity (animation triggers), microphone audio amplitude (lip sync), and a JSON character definition specifying visual layers and animation clips. Expert OBS plugin authors build exactly this type of source by implementing the `obs_source_info` callback struct against `libobs`, using the OBS plugin template as the project scaffold, and keeping all GPU work strictly on the OBS render thread via `gs_*` calls inside `video_render`.

The recommended approach is a clean two-layer architecture: an OBS Integration Layer (owning all `obs_*` and `gs_*` calls, lifecycle callbacks, and threading boundaries) over an Avatar Engine (pure C++ with no OBS headers, owning animation tracks, parameter store, and character loading). The stack is lean: C++20 with MSVC/VS 2022, CMake 3.24 using `obs-plugintemplate`, and two vendored single-header libraries (`nlohmann/json` for character files, `stb_image` for PNG loading). No external package manager, no Qt, no game engine.

The dominant risks are threading and lifecycle correctness. Three concurrent threads (OBS render, OBS audio, custom input thread) share avatar state, and every mistake at the thread boundary — calling `gs_*` without the graphics lock, storing `obs_data_t*` pointers, or taking a mutex on the audio thread — causes crashes or audio dropouts rather than visible misbehavior. The mitigation strategy is aggressive use of lock-free primitives (`std::atomic<float>` for RMS amplitude, SPSC ring buffer for input events) and strict enforcement that all GPU operations occur only inside `video_render` or under `obs_enter_graphics()` / `obs_leave_graphics()`.

---

## Key Findings

### Stack

- **C++20 / MSVC VS 2022 / CMake 3.24+**: Required by `obs-plugintemplate`. Build with `/MD` (Release CRT) in all configurations — mixing `/MDd` with OBS's Release CRT causes heap corruption.
- **`obs-plugintemplate` scaffold**: Official OBS plugin scaffold; handles CI, packaging, and CMake package config. Use `find_package(libobs REQUIRED)` — the legacy `FindLibObs.cmake` is deprecated for OBS 29+. Target OBS 30.0+ minimum.
- **nlohmann/json + stb_image as single-header vendored deps** (`deps/` directory): The only two external dependencies needed. No package manager (no vcpkg, Conan, or Qt).
- **Win32 Raw Input** (`RIDEV_INPUTSINK` + message-only window): Confirmed by live-verified Microsoft docs as the preferred approach over `WH_KEYBOARD_LL` hooks for monitoring use cases. LL hooks are silently removed after 1000ms timeout on Windows 10 1709+; Raw Input has no such risk.
- **libobs audio callback** (`obs_source_add_audio_capture_callback`): Taps an OBS audio source for PCM frames; plugin computes RMS amplitude in-process.

### Features (OBS APIs)

- **Source type**: `OBS_SOURCE_TYPE_INPUT` with `output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`. `OBS_SOURCE_CUSTOM_DRAW` signals that the source drives its own `gs_*` draw calls — correct for a plugin compositing layers in `video_render`.
- **`video_tick` is the animation update site**: Called on the render thread every video frame before `video_render`, with delta time. This is where animation state advances. All `gs_*` calls are forbidden here.
- **`video_render` is the only GPU draw site**: All `gs_*` calls go here. Correct pipeline: `gs_texrender_reset/begin` → render all layers back-to-front with `gs_matrix_push/pop` → `gs_texrender_end` → blit to OBS output.
- **Audio via `obs_source_add_audio_capture_callback`**: Fires on the OBS audio thread. Communicate RMS to render thread via `std::atomic<float>` only — never a mutex on the audio thread hot path.
- **`obs_data_t*` must not be stored**: Pointer is owned by OBS and released after `create()` returns. Read all values into your own structs immediately.
- **`update()` must be idempotent**: OBS calls it on load and on every property change. Always check-before-reallocate.

### Architecture

- **Two-layer architecture**: OBS Integration Layer (all `obs_*`/`gs_*` coupling) over pure-C++ Avatar Engine (no OBS headers). The engine owns: `ParameterStore` (named params, unordered_map), `AnimationTrack[]` (idle/keyboard/mouse/audio with priority ordering), `AnimationClip` evaluator (sparse keyframes + easing), `CharacterDefinition` loader.
- **`gs_texrender_t` FBO compositing**: Each frame, composite all avatar layers into an off-screen render target, then blit to OBS output. Cleaner than drawing directly to OBS output for multi-layer sources.
- **Premultiplied alpha at load time**: Convert straight-alpha PNGs to premultiplied on CPU once at texture load. OBS compositing pipeline assumes premultiplied throughout.
- **SPSC lock-free ring buffer** for input→render communication; `std::atomic<float>` for audio→render. Never take a mutex on OBS audio or render threads.
- **Sparse keyframe animation** (binary search + easing) with track-based blending: reset all params to defaults each tick, write lowest→highest priority tracks so highest priority wins per parameter.

### Pitfalls

1. **`gs_*` outside graphics context (CRITICAL)**: Crash / GPU corruption. Wrap all GPU ops in `obs_enter_graphics()` outside `video_render`. Most common OBS plugin crash.
2. **Mutex on OBS audio thread (CRITICAL)**: Audio dropouts. Use only `std::atomic` writes in audio callback — never a lock on the hot path.
3. **WH_KEYBOARD_LL silently removed after 1000ms (CRITICAL if LL hooks used)**: Silent hook removal on Windows 10 1709+, no error returned. Decision confirmed: use Raw Input.
4. **Raw Input requires `RIDEV_INPUTSINK` + message-only window + thread affinity**: Without `RIDEV_INPUTSINK`, input stops when user alt-tabs. Registration, HWND creation, and `GetMessage` loop must be on the same dedicated thread.
5. **Privacy — VKey must be discarded at input thread boundary**: `RAWKEYBOARD.VKey` exposes actual key identity. Architecture enforces: only `{key_down: bool}` enters SPSC queue. Per PROJECT.md requirement.
6. **`update()` non-idempotency causes texture leaks**: Called on every property change. Always check-before-reallocate GPU resources.
7. **`OBS_SOURCE_COMPOSITE` requires `audio_render`**: Setting the flag without implementing the callback is a null pointer crash. Do not set for a standalone avatar source.

---

## Implications for Roadmap

### Must-Do Before Coding (Spikes/Validation)

1. **Build system spike** (2 hours, blocks everything): Clone `obs-plugintemplate` HEAD, run `cmake --preset windows-x64`, verify current OBS stable version (30.x vs 31.x), confirm `buildspec.json` schema, confirm CMake module file names.
2. **Raw Input proof-of-concept** (half day): Minimal Win32 app that registers `RIDEV_INPUTSINK` on a message-only window, confirms background event delivery when app is not focused, validates privacy discard (key-down/up only).
3. **Graphics thread validation** (half day): Minimal OBS plugin that creates/destroys a texture with `obs_enter_graphics()` guards and renders it in `video_render`. Confirms threading model before investing in full pipeline.

### Architectural Decisions Confirmed by Research

- Two-layer architecture (OBS Integration + Avatar Engine without OBS headers)
- Raw Input over WH_KEYBOARD_LL (live-verified Microsoft docs)
- nlohmann/json for character files; stb_image for PNG loading (single-header vendoring)
- Premultiplied alpha conversion at CPU load time
- `gs_texrender_t` for off-screen FBO compositing
- SPSC ring buffer for input→render; `std::atomic<float>` for audio→render
- Sparse keyframe model + track-based blending with priority ordering

### Architectural Decisions Still Open

- **Mouth animation model**: Discrete texture swaps (simpler MVP) vs UV-parameterized single texture (smoother). Product decision needed before Phase 3/4 interface design.
- **Default character pack**: Whether to bundle a default character or require user-provided packs. Affects packaging phase.
- **`gs_texture_create` exact signature**: `data` param type varies by OBS version. Verify against current `graphics/graphics.h` before Phase 2.

### Phase Ordering Implications

**Phase 1 — Build System & Plugin Scaffold**: Everything blocks on a compilable plugin skeleton. Run the build system spike first. Deliverable: a `.dll` OBS loads that renders a solid color.

**Phase 2 — Core Rendering Pipeline**: GPU render pipeline is the foundation of every visual feature. Deliverable: `gs_texrender_t` compositing, `gs_matrix_push/pop` layer transforms, stb_image PNG loading, `video_render` + `video_tick` wired up.

**Phase 3 — Character Definition & Asset Loading**: JSON character format and texture loading that animation and rendering depend on. Deliverable: `character.json` parser, layer textures on GPU, character reload on settings change.

**Phase 4 — Animation System**: Testable with hardcoded idle animation before any input is wired in. Deliverable: `AnimationClip` evaluator, `ParameterStore`, track blending, idle animation playing.

**Phase 5 — Audio Lip Sync**: Simpler than global input (uses OBS audio callback API directly). Deliverable: audio source selection in properties, `obs_source_add_audio_capture_callback`, RMS→atomic→mouth parameter pipeline.

**Phase 6 — Global Input Capture (Raw Input)**: Most complex threading and Win32 setup; isolated to its own phase. Run Raw Input spike before this phase. Deliverable: message-only window, `RIDEV_INPUTSINK`, SPSC queue, keyboard/mouse animation triggers, clean unregistration on unload.

**Phase 7 — Properties UI & Polish**: Lowest risk, highest polish, done last. Deliverable: full properties panel, localization strings, advanced group, reload button.

**Phase 8 — Packaging & Distribution**: GitHub Actions CI, `.zip` artifacts, OBS plugin browser metadata.

---

## Technology Recommendations

| Decision | Recommendation | Confidence | Source |
|----------|---------------|------------|--------|
| Plugin scaffold | `obs-plugintemplate` | MEDIUM | STACK.md — verify HEAD |
| OBS minimum version | 30.0+ | MEDIUM | STACK.md — confirm current stable |
| Compiler | MSVC VS 2022, C++20 | HIGH | STACK.md |
| CMake minimum | 3.24 | HIGH | STACK.md |
| CRT linkage | `/MD` all configs | HIGH | STACK.md |
| OBS linkage | `find_package(libobs REQUIRED)` | HIGH | STACK.md |
| JSON library | nlohmann/json, vendored single header | HIGH | ARCHITECTURE.md |
| PNG loading | stb_image, vendored single header | HIGH | ARCHITECTURE.md |
| Global keyboard/mouse | Raw Input + `RIDEV_INPUTSINK` | HIGH | PITFALLS.md (live-verified docs) |
| Audio analysis | `obs_source_add_audio_capture_callback` + RMS | HIGH | FEATURES.md |
| Off-screen compositing | `gs_texrender_t` FBO | HIGH | ARCHITECTURE.md |
| Alpha convention | Premultiplied at CPU load time | HIGH | ARCHITECTURE.md |
| Input→render comms | SPSC lock-free ring buffer | HIGH | ARCHITECTURE.md |
| Audio→render comms | `std::atomic<float>` | HIGH | PITFALLS.md + FEATURES.md |
| Animation model | Sparse keyframes + track-based blending | HIGH | ARCHITECTURE.md |
| Package manager | None (vendor `deps/`) | HIGH | STACK.md |

---

## Critical Risks Identified

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| `gs_*` called outside graphics context | HIGH | Crash / GPU corruption | Wrap all GPU ops in `obs_enter_graphics()` outside `video_render`; code review checklist |
| Mutex on OBS audio thread | MEDIUM | Audio dropout | Only `std::atomic<float>` for RMS; never lock on audio callback hot path |
| Raw Input unregistered on plugin unload | HIGH | Access violation / crash | RAII cleanup: `RIDEV_REMOVE` + destroy HWND + join thread in `obs_module_unload` |
| Privacy: VKey logged or stored | LOW | Privacy violation | Architecture enforces discard at input thread — only `{key_down: bool}` enters SPSC queue |
| OBS version mismatch (`LIBOBS_API_VER`) | MEDIUM | Plugin fails to load | Pin exact version in `buildspec.json`; document minimum OBS version in README |
| `obs_source_info` struct size mismatch | LOW | Callbacks silently not called | Always use `obs_register_source(info)` macro, never manual `obs_register_source_s` |
| WH_KEYBOARD_LL silent timeout removal | LOW (decision confirmed) | Avatar stops responding to keyboard | Architecture confirmed: Raw Input only |

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | Training knowledge through mid-2025; `obs-plugintemplate` HEAD must be verified before Phase 1 |
| Features (OBS APIs) | MEDIUM | Core `obs_source_info` API stable since OBS 26; new fields in 28–30 need verification against current headers |
| Architecture | MEDIUM-HIGH | Rendering patterns (gs_matrix, gs_texrender, premultiplied alpha) are HIGH; animation system design MEDIUM-HIGH |
| Pitfalls | HIGH (Windows input, live-verified); MEDIUM (OBS lifecycle, training) | Windows Raw Input and LL hook pitfalls confirmed against live Microsoft docs |

**Overall confidence:** MEDIUM — sufficient to build a detailed roadmap and begin Phase 1. Key uncertainty is `obs-plugintemplate` HEAD state; a 2-hour spike resolves it.

### Gaps to Address

- **Current OBS stable version** (was 30.x → may be 31.x): Confirm before pinning `buildspec.json`. Resolve in Phase 1 spike.
- **`obs-plugintemplate` CMake module filenames**: Macro names confirmed; file names need live verification. Resolve in Phase 1 spike.
- **`gs_texture_create` exact signature**: Check current `graphics/graphics.h`. Resolve before Phase 2 texture loading.
- **Mouth animation model** (discrete textures vs UV-parameterized): Product decision needed before Phase 3/4 interface design.

---

## Sources

### Primary — HIGH confidence (live-verified)
- `https://raw.githubusercontent.com/obsproject/obs-studio/master/libobs/obs-source.h` — OBS source lifecycle API, struct size field
- `https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input` — Raw Input API (updated 2026-03-18)
- `https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc` — LL hook timeout, official Raw Input recommendation (updated 2025-07-16)

### Secondary — MEDIUM confidence (training knowledge, OBS 30.x)
- `obsproject/obs-plugintemplate` — scaffold structure, `buildspec.json`, CMake preset workflow
- `obsproject/obs-studio` — `libobs/obs-source.h`, `libobs/graphics/graphics.h`, `libobs/obs-module.h`
- OBS Plugin Development Wiki — https://obsproject.com/wiki/Getting-Started-with-OBS-Plugin-Development
- StreamFX — https://github.com/Xaymar/obs-StreamFX — advanced rendering patterns
- obs-move-transition — https://github.com/exeldro/obs-move-transition — per-element transforms

### Tertiary — HIGH confidence (established libraries)
- nlohmann/json — https://github.com/nlohmann/json
- stb_image — https://github.com/nothings/stb/blob/master/stb_image.h
- Spine Runtime docs — http://en.esotericsoftware.com/spine-runtimes — track model / sparse keyframe reference

---
*Research completed: 2026-09-22*
*Ready for roadmap: yes*
