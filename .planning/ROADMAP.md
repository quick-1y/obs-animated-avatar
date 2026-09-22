# Roadmap: OBS Animated Avatar Plugin

## Overview

Eight phases deliver a native OBS Studio plugin from empty repository to a distributable ZIP artifact. Each phase completes a coherent, independently verifiable capability. Phases are ordered to unblock downstream work: the render pipeline (Phase 2) must exist before character assets can appear on screen (Phase 3); the parameter store (Phase 4) must exist before audio (Phase 5) or input (Phase 6) can drive it. Properties and error handling (Phase 7) are deliberately last so they polish a working system rather than speculative scaffolding. Testing and packaging (Phase 8) close the loop with verifiable distribution artifacts.

---

## Executive Summary

The OBS Animated Avatar Plugin is a native C++20 OBS Studio plugin that provides a layered 2D animated avatar as a first-class OBS Source. It requires no companion application, no game engine, and no additional capture window — the avatar lives entirely inside OBS, consumes its rendering pipeline, and reacts in real time to three independent input streams: global keyboard/mouse activity, microphone amplitude, and an idle animation clock. The result is a character that visually conveys user presence and activity while streaming.

Native OBS integration is the correct architectural choice for three reasons. First, it eliminates inter-process latency: audio capture, animation update, and render output are synchronized to the same OBS video frame clock. Second, it uses the GPU resources OBS has already allocated — no secondary render context, no window capture. Third, it gives users the familiar OBS source/properties/settings UX at no integration cost to the plugin author.

The two-layer architecture — Avatar Engine over OBS Integration Layer — is the central structural decision. The Avatar Engine owns all animation, character, and audio logic as pure C++20 with no OBS headers; it could theoretically power a standalone application. The OBS Integration Layer is a thin wrapper of `obs_source_info` callbacks that forwards lifecycle events, drives the render/tick callbacks, and bridges the three input threads into the engine's parameter store. This separation enables unit testing the animation math and character parser in isolation, and it constrains the scope of each phase.

Key technical risks that shape the phase order: (1) The graphics context constraint — `gs_*` calls outside `video_render` or an explicit `obs_enter_graphics()` lock cause immediate crashes; enforcing this discipline from Phase 2 onward is non-negotiable. (2) The audio thread constraint — taking any mutex on the OBS audio callback hot path causes audio dropouts; the `std::atomic<float>` bridge in Phase 5 is the only correct pattern. (3) Raw Input lifecycle — the input thread's message-only HWND must be created, maintained, and destroyed entirely within the input thread; failure to call `RIDEV_REMOVE` before plugin unload produces access violations. These three risks each have a dedicated mitigation strategy described in the Risk Register below.

---

## System Architecture Diagram (ASCII)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            OBS STUDIO HOST                                  │
│                                                                             │
│  OBS Render Thread          OBS Audio Thread         OBS UI Thread         │
│  video_tick()               audio_capture_callback   obs_properties_t       │
│  video_render()             (hot path, no mutex)     update() / create()    │
└──────────┬──────────────────────────┬────────────────────────┬─────────────┘
           │                          │                        │
           ▼                          ▼                        ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                         OBS INTEGRATION LAYER                                │
│                                                                              │
│  ┌─────────────────────┐   ┌──────────────────────┐   ┌──────────────────┐  │
│  │  SourceLifecycle    │   │  AudioBridge         │   │  PropertiesPanel │  │
│  │  create/destroy     │   │  std::atomic<float>  │   │  obs_properties_t│  │
│  │  update/show/hide   │   │  rms_amplitude_      │   │  get_defaults    │  │
│  │  get_width/height   │   │  attack/release      │   │  obs_data_t read │  │
│  └────────┬────────────┘   └──────────┬───────────┘   └────────┬─────────┘  │
│           │                           │                        │             │
│  ┌────────▼────────────┐              │                        │             │
│  │  RenderDriver       │              │                        │             │
│  │  video_tick()  ─────┼──────────────┼────────────────────────┼──────────┐  │
│  │  video_render() ────┼──────────────┼────────────────────────┼──────────┘  │
│  │  gs_texrender_t     │              │                        │             │
│  └────────┬────────────┘              │                        │             │
└───────────┼───────────────────────────┼────────────────────────┼─────────────┘
            │                           │                        │
            ▼                           ▼                        ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                            AVATAR ENGINE (no OBS headers)                    │
│                                                                              │
│  ┌─────────────────┐  ┌──────────────────────────────┐  ┌─────────────────┐ │
│  │ ParameterStore  │  │    AnimationSystem           │  │ CharacterDef    │ │
│  │ (unordered_map  │  │  ┌──────────────────────┐    │  │ JSON parser     │ │
│  │  float/vec2)    │◄─┼──│ Track[0] Idle        │    │  │ Layer list      │ │
│  │                 │  │  │ Track[1] Audio        │    │  │ Pivot points    │ │
│  └────────┬────────┘  │  │ Track[2] Mouse        │    │  │ Anim refs       │ │
│           │           │  │ Track[3] Keyboard     │    │  └────────┬────────┘ │
│           ▼           │  └──────────────────────┘    │           │          │
│  ┌─────────────────┐  │  ┌──────────────────────┐    │  ┌────────▼────────┐  │
│  │ RenderCompositor│  │  │ AnimationClip        │    │  │ TextureCache    │ │
│  │ per-layer matrix│  │  │ sparse keyframes     │    │  │ gs_texture_t    │ │
│  │ gs_matrix push/ │  │  │ easing functions     │    │  │ premult alpha   │ │
│  │ gs_texrender FBO│  │  │ binary search eval   │    │  │ load once       │ │
│  └─────────────────┘  │  └──────────────────────┘    │  └─────────────────┘ │
│                        └──────────────────────────────┘                     │
└──────────────────────────────────────────────────────────────────────────────┘
            ▲
            │  SPSC ring buffer (input events: key_down/key_up, mouse delta, clicks)
            │
┌───────────┴──────────────────────────────────────────────────────────────────┐
│                         INPUT THREAD (dedicated)                             │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │  Win32 Raw Input (RIDEV_INPUTSINK)                                     │  │
│  │  Message-only HWND — CreateWindowEx(HWND_MESSAGE)                     │  │
│  │  GetMessage loop — WM_INPUT dispatch                                   │  │
│  │  Discard VKey/scan code — emit {key_down: bool} only                  │  │
│  │  Mouse: dx/dy delta, left/right/middle button events                  │  │
│  │  RAII unregister on thread join (RIDEV_REMOVE + DestroyWindow)         │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Responsibilities

| Component | Layer | Responsibility | Key Dependencies |
|-----------|-------|----------------|-----------------|
| `SourceLifecycle` | OBS Integration | Implements `obs_source_info` callback table; owns create/destroy/update/show/hide; reads `obs_data_t` immediately into own structs | libobs, `AvatarEngine` |
| `RenderDriver` | OBS Integration | Drives `video_tick` (advance animation) and `video_render` (composite layers to FBO, blit to OBS output); owns `gs_texrender_t` | libobs gs API, `RenderCompositor`, `AnimationSystem` |
| `AudioBridge` | OBS Integration | Registers `obs_source_add_audio_capture_callback`; computes RMS per callback; writes to `std::atomic<float>`; never takes a mutex | libobs, `std::atomic<float>` |
| `PropertiesPanel` | OBS Integration | Builds `obs_properties_t` tree; populates audio source dropdown; handles `get_defaults`; dispatches to engine on property change | libobs, `CharacterDiscovery` |
| `ParameterStore` | Avatar Engine | Thread-safe (written on render thread only) named float/vec2 store; read by `RenderCompositor`; written by `AnimationSystem` | C++20 stdlib only |
| `AnimationSystem` | Avatar Engine | Owns 4 `AnimationTrack` instances; resets params to defaults each tick; writes tracks lowest-to-highest priority; highest wins per channel | `ParameterStore`, `AnimationClip` |
| `AnimationClip` | Avatar Engine | Evaluates sparse keyframe sequences at time T; binary search over keyframe list; applies easing function per pair | C++20 stdlib only |
| `CharacterDefinition` | Avatar Engine | Parses `character.json` via nlohmann/json; validates schema version; populates layer list, pivot points, default transforms, animation clip references | nlohmann/json |
| `TextureCache` | Avatar Engine / OBS Integration boundary | Loads PNG via stb_image; converts straight alpha to premultiplied; creates/destroys `gs_texture_t` inside graphics lock; caches by path | stb_image, libobs gs API |
| `RenderCompositor` | OBS Integration | Reads `ParameterStore`; for each layer: pushes `gs_matrix`, applies transform, draws texture quad, pops matrix; all inside `gs_texrender` begin/end | libobs gs API, `ParameterStore`, `TextureCache` |
| `InputThread` | Platform (Windows) | Owns message-only HWND; `RIDEV_INPUTSINK` registration; GetMessage loop; discards VKey; writes `InputEvent{type, dx, dy}` to SPSC queue | Win32 Raw Input API |
| `InputBridge` | OBS Integration | Drains SPSC queue on render thread during `video_tick`; converts events to animation parameters on Keyboard/Mouse tracks | `InputThread`, `AnimationSystem` |
| `CharacterDiscovery` | Avatar Engine | Scans `characters/` directory; finds `character.json` in each subdirectory; returns name list for OBS properties dropdown | C++20 `<filesystem>` |

---

## Pipeline Descriptions

### Input Pipeline (keyboard/mouse event → animation parameter)

1. OS delivers raw input message to the plugin's message-only HWND via `WM_INPUT`.
2. `InputThread::GetMessage` loop wakes; calls `GetRawInputData` on the `HRAWINPUT` handle.
3. For keyboard: extracts only event type (key-down vs key-up); discards `VKey` and scan code.
4. For mouse: extracts `lLastX`/`lLastY` relative deltas; extracts button flags (RI_MOUSE_LEFT_BUTTON_DOWN etc.).
5. Constructs `InputEvent{EventType type; int16_t dx; int16_t dy;}` — no key identity.
6. Pushes event to SPSC ring buffer (non-blocking; drops if full).
7. On the OBS render thread during `video_tick`, `InputBridge::drain()` pops all queued events.
8. Keyboard events increment/reset a key-press accumulator; when > 0 and within cooldown, activates `Keyboard` track with typing animation parameters.
9. Mouse delta events accumulate; `InputBridge` scales delta and writes `hand_offset_x`, `hand_offset_y` to the `Mouse` track. Click events trigger one-shot animation clips.
10. Typing cooldown timer counts down each tick; when expired, `Keyboard` track returns arm parameters to baseline.

### Animation Pipeline (tick → parameter store → render)

1. `video_tick(float delta_seconds)` fires on the OBS render thread; `delta_seconds` is the actual frame interval.
2. `AnimationSystem::tick(delta_seconds)` called: resets all `ParameterStore` entries to their default values first.
3. For each track in priority order (Idle=0, Audio=1, Mouse=2, Keyboard=3): advance clip playback clock; evaluate all active clips at current clip time.
4. `AnimationClip::evaluate(t)` performs binary search over sparse keyframe list; selects the pair bracketing `t`; computes normalized `u = (t - t0) / (t1 - t0)`; applies easing curve; lerps output values.
5. Evaluated clip output is written into `ParameterStore`. Higher-priority tracks overwrite lower-priority writes on the same parameter channel.
6. After all tracks have written, `ParameterStore` values represent the blended animation state for this frame.
7. `video_render` then reads `ParameterStore` to compute per-layer transforms (see Rendering Pipeline).

### Rendering Pipeline (video_render → FBO → blit)

1. `video_render(gs_effect_t*)` fires on the OBS render thread (graphics context is active; `gs_*` calls are safe).
2. `gs_texrender_reset(texrender_)` clears the off-screen target.
3. `gs_texrender_begin(texrender_, width_, height_)` binds the FBO as current render target.
4. For each layer in `CharacterDefinition::layers` (back-to-front order):
   a. Read position, rotation, scale, opacity from `ParameterStore` for this layer's parameter channels.
   b. `gs_matrix_push()` to save current transform.
   c. Apply translation: `gs_matrix_translate3f(x, y, 0)`.
   d. Apply rotation around pivot: translate to pivot, `gs_matrix_rotaa4f`, translate back.
   e. Apply scale: `gs_matrix_scale3f(sx, sy, 1)`.
   f. Set shader opacity uniform.
   g. `gs_draw_sprite(texture, 0, width, height)` draws the layer quad.
   h. `gs_matrix_pop()` restores transform.
5. `gs_texrender_end(texrender_)` unbinds FBO.
6. `obs_source_draw(gs_texrender_get_texture(texrender_), ...)` blits composited result to OBS output.

### Audio / Lip-Sync Pipeline (callback → RMS → atomic → mouth parameter)

1. `obs_source_add_audio_capture_callback(audio_source_, callback, this)` registered during `update()` when audio source setting changes.
2. OBS fires callback on the audio thread each audio frame; signature: `(void* param, obs_source_t* src, const audio_data* data, bool muted)`.
3. Callback reads `data->frames` samples from `data->data[0]` (32-bit float planar).
4. Computes RMS: `sqrt(sum(s*s) / n)` over all frames in the callback.
5. Applies silence threshold: if `rms < threshold_`, `rms = 0`.
6. Applies sensitivity multiplier: `rms *= sensitivity_`.
7. Writes result to `std::atomic<float> rms_amplitude_` using `store(rms, std::memory_order_relaxed)`.
8. No mutex, no allocation, no OBS API calls in the callback.
9. On the render thread during `video_tick`, `AudioBridge::update(delta)` reads `rms_amplitude_.load()`.
10. Applies attack/release smoothing: `if (rms > smoothed_) smoothed_ += (rms - smoothed_) * attack_coeff_; else smoothed_ -= (smoothed_ - rms) * release_coeff_`.
11. Maps smoothed RMS to discrete mouth state: `closed` (< t1), `small` (< t2), `open` (< t3), `wide` (≥ t3).
12. Writes `mouth_open` parameter (0.0 / 0.33 / 0.66 / 1.0) to the `Audio` animation track in `ParameterStore`.
13. `RenderCompositor` reads `mouth_open` and selects the correct mouth layer texture or transform each frame.

---

## Threading Model

| Thread | Owner | Key Callbacks / Functions | Allowed Operations | Forbidden Operations | Sync Mechanism |
|--------|-------|--------------------------|-------------------|---------------------|----------------|
| OBS Render Thread | OBS host | `video_tick`, `video_render`, `update`, `show`, `hide` | All `gs_*` calls; `ParameterStore` reads/writes; SPSC drain; `rms_amplitude_.load()` | Mutex acquisition; blocking I/O; `obs_source_add_audio_capture_callback` | Sole owner of `ParameterStore`; no lock needed |
| OBS Audio Thread | OBS host | `obs_source_audio_capture_callback` | `std::atomic<float>::store`; arithmetic; no allocations | Any mutex; any blocking call; any `obs_*` API; any `gs_*` | `std::atomic<float>` (relaxed ordering) |
| Input Thread | Plugin | `CreateWindowEx(HWND_MESSAGE)`, `RegisterRawInputDevices`, `GetMessage` loop | `GetRawInputData`; SPSC push; Win32 message API | `obs_*` API; `gs_*` API; mutex on hot path | SPSC lock-free ring buffer → Render Thread |
| OBS UI Thread | OBS host | `obs_properties_get`, property change callbacks | `obs_data_*` reads; `obs_source_*` property API; schedule reload on render thread | Direct `gs_*` without `obs_enter_graphics()` | `obs_enter_graphics()` / `obs_leave_graphics()` for any GPU work |

---

## Repository Structure

```
obs-avatar/
├── CMakeLists.txt              # Top-level; sets C++20, finds libobs, adds subdirs
├── CMakePresets.json           # From obs-plugintemplate; windows-x64 preset
├── buildspec.json              # OBS dev package version pin
├── .github/
│   └── workflows/
│       ├── build.yml           # CI: windows-x64 matrix, artifact upload
│       └── release.yml         # Tag-triggered release ZIP creation
├── src/
│   ├── plugin-main.cpp         # obs_module_load / obs_module_unload; registers source
│   ├── plugin-support.h        # blog() / obs_log() wrappers with [obs-avatar] prefix
│   ├── obs-integration/
│   │   ├── AvatarSource.h/.cpp # obs_source_info table; create/destroy/update/tick/render
│   │   ├── RenderDriver.h/.cpp # video_tick + video_render; owns gs_texrender_t
│   │   ├── AudioBridge.h/.cpp  # obs_source_add_audio_capture_callback; RMS → atomic
│   │   ├── InputBridge.h/.cpp  # Drains SPSC queue; maps events → animation params
│   │   └── PropertiesPanel.h/.cpp  # obs_properties_t construction; get_defaults
│   ├── engine/
│   │   ├── ParameterStore.h/.cpp   # Named float/vec2 store; default values
│   │   ├── AnimationSystem.h/.cpp  # 4 tracks; tick(); reset-then-write blending
│   │   ├── AnimationTrack.h/.cpp   # Single track; owns active clips; priority value
│   │   ├── AnimationClip.h/.cpp    # Sparse keyframes; binary search; easing evaluation
│   │   ├── CharacterDefinition.h/.cpp  # JSON parse; layer list; pivot points; anim refs
│   │   ├── CharacterDiscovery.h/.cpp   # Filesystem scan; character name list
│   │   └── Easing.h                # Constexpr easing functions (linear, cubic, step)
│   ├── render/
│   │   ├── RenderCompositor.h/.cpp # Per-layer matrix transform + gs_draw_sprite
│   │   └── TextureCache.h/.cpp     # stb_image load; premult alpha; gs_texture_t lifecycle
│   └── platform/
│       ├── InputCapture.h          # Abstract interface: start/stop/setCallback
│       ├── win32/
│       │   ├── Win32InputCapture.h/.cpp  # Raw Input; message-only HWND; SPSC push
│       │   └── SpscQueue.h               # Header-only lock-free SPSC ring buffer
│       └── null/
│           └── NullInputCapture.h/.cpp   # No-op implementation for testing
├── deps/
│   ├── nlohmann/
│   │   └── json.hpp            # nlohmann/json single header (vendored)
│   └── stb/
│       └── stb_image.h         # stb_image single header (vendored)
├── data/
│   └── locale/
│       └── en-US.ini           # OBS locale strings for all properties labels
├── characters/
│   └── default/
│       ├── character.json      # Default bundled character definition
│       └── textures/
│           ├── body.png
│           ├── head.png
│           ├── arm_left.png
│           ├── arm_right.png
│           ├── mouth_closed.png
│           ├── mouth_small.png
│           ├── mouth_open.png
│           └── mouth_wide.png
└── tests/
    ├── CMakeLists.txt          # GoogleTest/Catch2 integration
    ├── test_easing.cpp         # All 5 interpolation modes; boundary values
    ├── test_animation_clip.cpp # Keyframe evaluation; looping; one-shot completion
    ├── test_parameter_store.cpp # Priority blending; default reset
    ├── test_rms.cpp            # RMS computation; attack/release smoothing
    └── test_character_parser.cpp  # Valid JSON; missing fields; wrong version; bad paths
```

---

## Architecture Decision Records (ADRs)

### ADR-01: C++ vs Rust

**Decision:** Implementation language for the plugin.

**Alternatives Considered:**
- Rust with `cc` crate and C FFI bindings to libobs
- C++ with C++20 features (concepts, ranges, coroutines where applicable)
- C (no RAII, no templates)

**Chosen Approach:** C++20 with MSVC VS 2022.

**Rationale:** libobs is a C API with C++ community tooling. The official `obs-plugintemplate` scaffold targets MSVC/CMake/C++. The existing OBS plugin ecosystem (obs-StreamFX, obs-move-transition) is universally C++, providing directly applicable patterns. Rust FFI to a C API introduces binding maintenance overhead, and the C++ standard library provides all needed abstractions (RAII, `std::atomic`, `std::unordered_map`, `<filesystem>`). C++ is the path of least resistance with maximum reference material.

**Consequences:** RAII discipline required to prevent resource leaks. Undefined behavior risks mitigated by `-Wall -Wextra` and AddressSanitizer in debug builds. MSVC ABI locks the Windows platform; cross-platform later requires compiler matrix.

---

### ADR-02: Native OBS Plugin vs External Application

**Decision:** How the avatar is delivered to OBS.

**Alternatives Considered:**
- Separate application that creates a window captured via OBS "Window Capture" source
- Browser source (Electron/web renderer)
- Virtual camera feeding an OBS camera source
- Native OBS plugin (`.dll`)

**Chosen Approach:** Native OBS plugin registering an `OBS_SOURCE_TYPE_INPUT` source.

**Rationale:** Native plugin eliminates inter-process IPC latency, avoids a second window in the taskbar, does not require the user to manage a separate process lifecycle, and has zero compositing overhead (no screen capture intermediary). Browser source adds Chromium as a dependency and introduces CSS/JS rendering as an intermediary; it cannot access Raw Input directly. Virtual camera adds a V4L2/virtual-cam driver dependency and color space conversion overhead.

**Consequences:** Must conform to OBS plugin API and threading model. Breaking OBS API changes (rare but possible) require plugin updates. Distribution requires `.dll` install to OBS plugin directory.

---

### ADR-03: libobs Renderer vs External Renderer

**Decision:** Which GPU API to use for rendering the avatar layers.

**Alternatives Considered:**
- Direct3D 11 via `<d3d11.h>` with a separate render context
- OpenGL via GLFW in a separate context
- Skia 2D graphics library
- libobs `gs_*` API (`gs_texrender_t`, `gs_texture_t`, `gs_matrix_*`)

**Chosen Approach:** libobs `gs_*` API exclusively.

**Rationale:** OBS already owns the D3D11 context; creating a second D3D11 device causes resource sharing problems and doubles GPU memory overhead. The `gs_*` abstraction layer works on both D3D11 and OpenGL backends transparently. `gs_texrender_t` provides exactly the off-screen FBO compositing needed. `gs_matrix_push/pop` provide 2D transform stacks. For a 2D layered avatar, the OBS graphics API covers 100% of requirements. Adding a second renderer adds ~2MB of DLL surface and a complex context synchronization problem with no benefit.

**Consequences:** All GPU operations are tied to the OBS graphics context lifecycle. Must strictly observe the graphics lock rules (inside `video_render` or under `obs_enter_graphics()`). Graphics API is not unit-testable in isolation; render correctness is validated by visual inspection.

---

### ADR-04: Layered 2D vs Sprite Sheet Animation

**Decision:** Animation rendering approach for the character.

**Alternatives Considered:**
- Sprite sheet: single large texture atlas with frame rectangles; swap UV coords per frame
- Skeletal animation: Spine runtime, bone hierarchy, mesh deformation
- Layered 2D: independent PNG textures per body part, positioned and transformed independently

**Chosen Approach:** Layered 2D with independent PNG textures per layer.

**Rationale:** Sprite sheets require a fixed frame grid and are suited to traditional frame-by-frame animation; they don't support parameterized continuous transforms (smooth arm rotation driven by mouse delta). Skeletal animation (Spine) adds a 3MB runtime library dependency and requires Spine-formatted asset files — significant toolchain overhead for an MVP. Layered 2D with transform parameters maps directly to the parameter store model: `arm_right.rotation` is a float that drives a `gs_matrix_rotaa4f` call. It supports continuous parameter-driven motion, is simple to understand and author, and requires only standard PNG tools to create assets.

**Consequences:** Layer count grows with character complexity; established 12-layer cap in REND-07. Character artists must supply separate PNGs per body part rather than a single atlas. Mouth states require separate mouth texture per state (4 textures) rather than a parameterized UV offset.

---

### ADR-05: Animation Engine Design (Track/Priority Model)

**Decision:** How multiple animation sources (idle, audio, mouse, keyboard) blend onto shared parameters.

**Alternatives Considered:**
- Additive blending: all tracks add their deltas to a shared parameter
- Layered blending with masks: explicit bitmask of which parameters each track owns
- Priority override: reset to defaults, write lowest-to-highest priority, higher wins per channel
- State machine: explicit state transitions with cross-fade blending

**Chosen Approach:** Priority override model (reset-then-write).

**Rationale:** Additive blending causes parameter drift when multiple tracks write to the same channel simultaneously. Masked blending requires explicit authoring of which parameters each track "owns" — fragile and error-prone as character definitions evolve. A state machine with cross-fading is correct but substantially more complex; MVP complexity is not justified. The priority override model is simple, predictable, and matches the mental model: "keyboard track owns arm parameters while typing; when typing stops, idle reclaims them." It requires only a defined write order (Idle → Audio → Mouse → Keyboard) each tick. Reset to defaults first ensures no accumulation drift.

**Consequences:** A parameter can only be written by one track per tick (the highest-priority active track). True cross-fade blending between tracks is not supported in MVP; the transition will be an instant snap. This is acceptable for MVP; smooth cross-fade can be added in v2 by lerping between the written value and the previous frame's value.

---

### ADR-06: Keyboard Input Mechanism (Raw Input vs WH_KEYBOARD_LL vs Polling)

**Decision:** How to capture global keyboard events.

**Alternatives Considered:**
- `WH_KEYBOARD_LL` low-level keyboard hook: `SetWindowsHookEx(WH_KEYBOARD_LL, ...)`
- `WH_KEYBOARD` thread-local hook (does not cover other applications' windows)
- Direct input polling (`GetKeyState` in a tight loop)
- Windows Raw Input API: `RegisterRawInputDevices` with `RIDEV_INPUTSINK`

**Chosen Approach:** Windows Raw Input with `RIDEV_INPUTSINK` on a dedicated message-only window thread.

**Rationale:** `WH_KEYBOARD_LL` hooks are silently removed by Windows after approximately 1000ms of message-processing latency on Windows 10 version 1709 and later (per live-verified Microsoft documentation). There is no error indication; the hook simply stops firing. This makes LL hooks categorically unsuitable for a plugin that may share a process with other CPU-intensive OBS rendering. Polling `GetKeyState` in a tight loop wastes CPU and does not detect key-up reliably for fast keystrokes. Raw Input with `RIDEV_INPUTSINK` is the Microsoft-recommended approach for monitoring use cases; it does not require focus, has no timeout removal, and delivers events to a background thread via a message queue. The message-only window (`CreateWindowEx` with `HWND_MESSAGE` as parent) avoids adding a visible window to the desktop hierarchy.

**Consequences:** Requires a dedicated input thread with a Win32 message loop. Thread must be created and destroyed carefully (see Risk Register RISK-03). Privacy enforcement must be applied at the `GetRawInputData` call site — VKey and scan code must be discarded before the event enters the SPSC queue.

---

### ADR-07: Mouse Input Mechanism

**Decision:** How to capture global mouse movement and click events.

**Alternatives Considered:**
- `SetWindowsHookEx(WH_MOUSE_LL, ...)` low-level mouse hook
- `GetCursorPos` polling (absolute position, privacy concern)
- Windows Raw Input (same infrastructure as keyboard)

**Chosen Approach:** Windows Raw Input (same device thread as keyboard, same `RIDEV_INPUTSINK` registration).

**Rationale:** LL mouse hooks have the same 1000ms timeout removal problem as keyboard hooks on Windows 10 1709+. Absolute cursor position polling exposes cursor location — a mild privacy concern and requires computing deltas manually from absolute positions (losing precision on fast movement). Raw Input delivers relative deltas (`lLastX`, `lLastY`) directly, which is exactly what the mouse animation model needs. Sharing the same Raw Input device registration and message loop with keyboard input minimizes thread count and synchronization complexity.

**Consequences:** Both keyboard and mouse input share a single input thread and SPSC queue. Mouse events use `dx/dy` fields in `InputEvent`; keyboard events use `type` field only. Queue item size is fixed to accommodate either type.

---

### ADR-08: OBS Audio Integration Approach

**Decision:** How to access microphone audio data for lip sync.

**Alternatives Considered:**
- Windows WASAPI directly: `IAudioCaptureClient`, separate audio device enumeration
- OBS output audio (post-mix): taps the OBS master mix rather than a specific source
- `obs_source_add_audio_capture_callback` on a user-selected OBS audio source

**Chosen Approach:** `obs_source_add_audio_capture_callback` on a user-selected audio source.

**Rationale:** WASAPI would require duplicate device enumeration outside OBS, would not respect OBS's audio routing (e.g., if the user has routed their mic through a noise gate), and would bypass any OBS audio filters the user has applied. `obs_source_add_audio_capture_callback` taps the audio source post-filter, matching what the user actually hears. It reuses OBS's existing audio thread infrastructure. The user-selectable source gives flexibility: they can lip-sync to any OBS audio source, not just a physical microphone.

**Consequences:** Audio callback fires on OBS's audio thread; plugin must not acquire any mutex in the callback. `std::atomic<float>` is the only correct bridge. Audio source pointer can become invalid if the user removes the source; `AUDIO-07` covers clean disable with no crash.

---

### ADR-09: Configuration/Character Format (JSON vs TOML vs Binary)

**Decision:** File format for `character.json` and animation clip definitions.

**Alternatives Considered:**
- TOML: human-friendly, no single-header C++ parser with comparable maturity to nlohmann/json
- Binary (custom or MessagePack): compact, fast parse; difficult to hand-author and inspect
- XML: verbose; no good vendorable single-header C++ parser
- JSON with nlohmann/json

**Chosen Approach:** JSON with nlohmann/json vendored as a single header.

**Rationale:** JSON is the de-facto standard for configuration in OBS plugin ecosystems (OBS scene files are JSON). nlohmann/json is the most mature, most used single-header C++ JSON library; it requires no build system integration beyond placing `json.hpp` in `deps/`. JSON files are human-readable and editable with any text editor, enabling manual character authoring in MVP without a dedicated editor. Schema version field provides forward compatibility.

**Consequences:** No streaming parser; `nlohmann/json::parse` loads the full file into memory. For `character.json` files (< 10KB), this is negligible. Animation clip files with many keyframes will still be small (float arrays). Parsing error messages from nlohmann/json are detailed enough to produce useful `LOG_ERROR` output to users.

---

### ADR-10: Threading Model and Synchronization Primitives

**Decision:** How to share state across OBS render thread, OBS audio thread, and the plugin's input thread.

**Alternatives Considered:**
- Global mutex protecting all shared state: simple but blocks render thread on lock contention
- Message passing with a `std::queue` protected by `std::mutex`
- Lock-free structures throughout: complex but zero contention
- Hybrid: atomic for audio RMS; SPSC for input events; render thread as sole writer of ParameterStore

**Chosen Approach:** Hybrid lock-free model.

**Rationale:** A global mutex would require the audio thread to acquire it per callback (tens of thousands of times per second at 48kHz), which the OBS audio architecture explicitly forbids. The chosen model assigns ownership: the render thread is the sole writer of `ParameterStore` (no lock needed); the audio thread only writes `std::atomic<float> rms_amplitude_` (lock-free); the input thread only writes to an SPSC ring buffer (lock-free by construction). The render thread reads atomics and drains the SPSC queue during `video_tick`. Each path has defined owner; no lock is ever taken on the audio or render hot paths.

**Consequences:** `ParameterStore` has no lock because only one thread writes it. Any future feature that requires multi-threaded writes to `ParameterStore` must be designed carefully. SPSC queue requires fixed capacity; events dropped when full (acceptable: transient input events at human HCI speeds are rarely faster than the 60Hz drain rate).

---

### ADR-11: Asset Loading Library (stb_image vs libpng vs WIC)

**Decision:** Library for loading PNG textures.

**Alternatives Considered:**
- libpng: mature, streaming decode, complex API, CMake integration required, not header-only
- Windows Imaging Component (WIC): Windows-only, COM-based, verbose boilerplate
- stb_image: public domain single-header library; supports PNG, JPEG, BMP; simple API

**Chosen Approach:** stb_image vendored as `deps/stb/stb_image.h`.

**Rationale:** stb_image covers all needed formats (PNG with alpha is the only format for MVP). It is a single-header drop-in: `#define STB_IMAGE_IMPLEMENTATION` in one `.cpp` file and include everywhere else. No CMake package, no vcpkg, no DLL. The API is three lines: `stbi_load`, use pixel data, `stbi_image_free`. It handles straight-alpha PNG correctly (output is 4-channel RGBA uint8); the premultiplied alpha conversion is a simple per-pixel multiply pass. Zero additional toolchain complexity.

**Consequences:** stb_image is not the fastest PNG decoder (libpng with SIMD is ~2x faster). For avatar textures loaded once at character selection time (not per-frame), this is completely acceptable. If loading a character with 12 large 4K textures proves slow, the fix is a background loading thread with a placeholder texture, not a library swap.

---

### ADR-12: Platform Abstraction Approach

**Decision:** How to isolate Win32-specific code from the engine core.

**Alternatives Considered:**
- Preprocessor guards (`#ifdef _WIN32`) throughout engine source files
- Separate compilation units conditionally included via CMake
- Abstract interface (`InputCapture`) with platform-specific implementations in `platform/win32/`

**Chosen Approach:** Abstract interface with platform-specific subdirectory.

**Rationale:** Preprocessor guards scattered through engine files make the code harder to read, test, and port. CMake conditional compilation achieves isolation but requires CMake changes for each new platform. An abstract `InputCapture` interface with a `NullInputCapture` for testing and a `Win32InputCapture` for production cleanly separates concerns. The engine core never sees `<windows.h>`. Adding a Linux `EvdevInputCapture` or macOS `CGEventTapCapture` in v2 requires only a new file in `platform/` and a CMake `if(WIN32)/if(LINUX)/if(APPLE)` block.

**Consequences:** Slight indirection overhead (virtual dispatch on one function call per tick to drain the input queue). Inconsequential for a 60Hz event rate. `NullInputCapture` enables full animation system testing without Win32 dependencies.

---

## Risk Register

| Risk ID | Description | Probability | Impact | How to Validate | Fallback / Mitigation |
|---------|-------------|-------------|--------|-----------------|----------------------|
| RISK-01 | `gs_*` called outside graphics context | HIGH | Crash / GPU corruption | AddressSanitizer + manual code review of all `gs_*` call sites; Phase 2 spike confirms pattern | Enforce rule: `gs_*` only in `video_render` or under `obs_enter_graphics()`. Add comment to every call site. Code review checklist item. |
| RISK-02 | Mutex on OBS audio thread | MEDIUM | Audio dropout / deadlock | Verify no `std::mutex::lock` in `AudioBridge` callback path; review with ThreadSanitizer | Architectural rule: `AudioBridge::callback` may only call `std::atomic<float>::store` and arithmetic. No allocations, no locks, no OBS API. |
| RISK-03 | Raw Input silent unregister on plugin unload | HIGH | Access violation crash on OBS exit or plugin reload | Manual test: load plugin, reload OBS, reload plugin; verify no crash. Also: ASAN on shutdown path. | RAII cleanup in `Win32InputCapture` destructor: post `WM_QUIT` to input thread, join thread, call `RIDEV_REMOVE`, `DestroyWindow`. Destructor called from `obs_module_unload`. |
| RISK-04 | Anti-cheat software (Easy Anti-Cheat, BattlEye) interfering with Raw Input | LOW | Input capture silently fails; avatar stops reacting | User reports; no code-level detection possible | Document known limitation. Provide per-device enable toggle. Raw Input is a non-injecting API; EAC typically targets injection-based hooks, not Raw Input. |
| RISK-05 | OBS version API incompatibility | MEDIUM | Plugin fails to load; struct size mismatch | `LIBOBS_API_VER` check in `obs_module_load`; test against OBS 30.x and 31.x | Pin version in `buildspec.json`. Document minimum OBS version in README. Use `obs_register_source` macro (not `_s` variant). |
| RISK-06 | Privacy: accidental VKey content logging | LOW | Privacy violation; potential misuse | Code audit of `InputThread::GetMessage` handler: confirm VKey/ScanCode never written to SPSC queue or log | Architecture enforces discard: only `EventType` enum enters SPSC. `LOG_DEBUG` may never log key identity. Review checklist in Phase 6. |
| RISK-07 | GPU resource leak on source reload | MEDIUM | VRAM exhaustion after repeated character changes | Measure VRAM with GPU-Z before/after 10 source delete/recreate cycles | `TextureCache::unload()` called inside `obs_enter_graphics()` before reload. `update()` checks existing handles before allocating. Integration test covers reload cycle. |
| RISK-08 | Multiple avatar sources GPU memory | MEDIUM | VRAM exhaustion with many source instances | Measure VRAM with 5 simultaneous avatar sources at 1080p | Each source owns independent `TextureCache`. If VRAM is a concern in v2, implement shared texture registry keyed by file path. MVP per-source ownership is simpler. |
| RISK-09 | High CPU load from input thread | LOW | CPU contention affecting OBS streaming | Profile with VTune or Windows Performance Analyzer: input thread CPU% during sustained typing | Input thread sleeps in `GetMessage` (blocking wait); CPU only consumed on event delivery. SPSC push is O(1). No spin-wait in design. |
| RISK-10 | Character format version incompatibility | LOW | New character packs fail to load with old plugin | Load character with `version: 2` on plugin that only supports `version: 1`; confirm clear error log | `CharacterDefinition::parse()` reads `version` field first; rejects unsupported versions with `LOG_ERROR` and loads fallback (blank source). Version field is `CHAR-07`. |

---

## Performance Targets

| Scenario | CPU Budget | GPU Budget | Memory Budget | Latency Budget | How to Measure |
|----------|-----------|-----------|---------------|----------------|----------------|
| Idle animation, no input, 1080p/60fps | < 0.5% CPU (single core) | < 1ms GPU frame time | < 50MB VRAM | N/A (continuous) | Task Manager CPU column; RenderDoc frame time |
| Active typing animation, 60fps | < 1.0% CPU | < 2ms GPU frame time | < 50MB VRAM | < 16ms input→render | Keyboard timestamp vs frame timestamp |
| Active mouse movement, 60fps | < 1.0% CPU | < 2ms GPU frame time | < 50MB VRAM | < 16ms delta→render | Mouse event timestamp vs frame timestamp |
| Active lip sync (mic input), 60fps | < 0.5% CPU (audio thread) | < 2ms GPU frame time | < 50MB VRAM | < 33ms RMS→mouth | Audio callback timestamp vs render timestamp |
| 12-layer character, 1080p/60fps | < 1.5% CPU | < 3ms GPU frame time | < 80MB VRAM | N/A | RenderDoc; REND-07 validation |
| Character hot-reload (change selection) | Spike < 200ms | N/A (one-time) | Must release old VRAM within 2 frames | < 200ms visible freeze | Stopwatch; VRAM monitor |
| OBS startup / plugin load | < 500ms additional startup time | N/A | < 5MB DLL overhead | < 500ms | OBS startup timer log |

---

## Testing Strategy

### Unit Tests

Unit tests use GoogleTest (or Catch2) compiled separately from the plugin DLL. They link against `engine/` and `render/` but not `obs-integration/` (no OBS dependency in unit tests). Each test file corresponds to one module.

**`test_easing.cpp` — `Easing.h`**
- Linear interpolation: `lerp(0, 1, 0.5)` → 0.5; boundary values u=0 → 0, u=1 → 1
- Ease-in cubic: verify curve is slower at start, faster at end
- Ease-out cubic: verify curve is faster at start, slower at end
- Ease-in-out cubic: verify symmetry around u=0.5
- Hold (step): `evaluate(u < 1.0)` → start value; `evaluate(u = 1.0)` → end value
- Out-of-range u: u < 0 clamped to start; u > 1 clamped to end

**`test_animation_clip.cpp` — `AnimationClip.h`**
- Single-keyframe clip: evaluate at any t → constant value
- Two-keyframe linear clip: midpoint interpolation
- Multi-keyframe sparse: evaluate at keyframe times exactly; evaluate between keyframes
- Loop mode: t beyond clip duration wraps to `fmod(t, duration)`
- One-shot mode: t beyond clip duration clamps to final keyframe value
- One-shot completion flag: `isComplete()` returns true after clip duration elapsed

**`test_parameter_store.cpp` — `AnimationSystem.h` / `ParameterStore.h`**
- Default reset: after `tick()`, all params reset to defaults before track writes
- Priority ordering: Keyboard track (priority 3) write of `arm_right.rotation = 90` survives when Idle track (priority 0) writes `arm_right.rotation = 0` first in same tick
- Audio track owns `mouth_open` exclusively when active; Idle track does not override it
- Multi-parameter frame: verify 5 different param channels all hold correct values after one tick with 3 active tracks

**`test_rms.cpp` — `AudioBridge` logic (extracted pure function)**
- Zero-signal RMS: all-zero buffer → RMS = 0
- Full-scale RMS: buffer of 1.0 samples → RMS = 1.0
- Mixed: verify `sqrt(sum/n)` formula against hand-calculated value
- Silence threshold: RMS below threshold snaps to 0.0
- Attack smoothing: new RMS > smoothed → rises at attack rate
- Release smoothing: new RMS < smoothed → falls at release rate
- Sensitivity multiplier: 2x sensitivity doubles pre-threshold RMS

**`test_character_parser.cpp` — `CharacterDefinition.h`**
- Valid minimal JSON: 1 layer, version 1, no animation refs → loads successfully
- Valid full JSON: all fields present → all fields populated correctly
- Missing `version` field → `LOG_ERROR`; returns `std::nullopt`
- Wrong `version` number → `LOG_ERROR`; returns `std::nullopt`
- Missing `layers` array → `LOG_ERROR`; returns `std::nullopt`
- Malformed JSON (syntax error) → `LOG_ERROR`; returns `std::nullopt`
- Missing texture path in layer → layer populated with empty path (CHAR-04 handled at load time)
- Unicode path in texture field → round-trips correctly through `std::filesystem::path`

### Integration Tests

**`test_integration_load.cpp` — Plugin DLL lifecycle (requires OBS test harness)**

OBS provides a minimal test harness (`obs_init` / `obs_shutdown` in test mode). This test:
1. Calls `obs_module_load()`; verifies return `true`
2. Creates an avatar source via `obs_source_create("animated_avatar_source", ...)`
3. Calls `obs_source_get_width` / `obs_source_get_height`; verifies non-zero
4. Opens properties via `obs_source_properties`; verifies non-null
5. Sets a character setting; calls `obs_source_update`; verifies no crash
6. Saves settings: `obs_source_get_settings` → serialize → deserialize → `obs_source_update`
7. Releases source; calls `obs_module_unload()`; verifies clean (no ASAN errors)

### Manual Test Checklist

**Keyboard Reactions:**
- [ ] Open OBS with plugin installed. Add Animated Avatar source.
- [ ] Type text in any application window. Observe arm/hand animation activates.
- [ ] Stop typing for > 500ms. Observe arm/hand returns to neutral.
- [ ] Rapid typing: sustain typing for 10 seconds. Verify animation stays active throughout.
- [ ] Disable keyboard reactions in properties. Verify arm/hand does not animate while typing.
- [ ] Re-enable keyboard reactions. Verify animation resumes.

**Mouse Reactions:**
- [ ] Move mouse rapidly. Observe hand follows movement direction.
- [ ] Stop mouse. Observe hand returns smoothly to neutral.
- [ ] Left-click. Observe click animation on hand.
- [ ] Right-click. Observe click animation.
- [ ] Middle-click (scroll wheel). Observe animation.
- [ ] Disable mouse reactions. Verify hand does not move with mouse.

**Audio / Lip Sync:**
- [ ] Select microphone as audio source in properties.
- [ ] Speak at normal volume. Observe mouth opens.
- [ ] Speak loudly. Observe mouth transitions through small→open→wide.
- [ ] Silence. Observe mouth closes promptly after release time.
- [ ] Adjust sensitivity slider. Verify mouth response changes.
- [ ] Remove selected audio source from OBS. Verify plugin does not crash.
- [ ] Background noise only (typing, fan). Verify silence threshold keeps mouth closed.

**OBS Lifecycle:**
- [ ] Add source. OBS restart. Verify source persists with all settings.
- [ ] Duplicate source in same scene. Verify both instances animate independently.
- [ ] Add source to second scene. Switch scenes. Verify animation continues correctly.
- [ ] Delete source. Verify no crash or VRAM leak.
- [ ] Reload plugin (disable/enable in Tools → obs-plugins). Verify clean reload.

---

## Security & Privacy Considerations

### What Data Is Observed

The plugin observes: (1) keyboard event type (key-down or key-up), not key identity; (2) mouse relative delta (dx, dy) and button state (left/right/middle), not absolute cursor position; (3) microphone audio amplitude (RMS scalar), not waveform data or speech content.

### What Is Never Stored or Transmitted

The plugin never stores, logs, or transmits: key identity (Virtual Key codes, scan codes, characters); key sequences or keystroke history; absolute cursor position; raw audio samples or waveform data; any data to any network endpoint. There is no network communication of any kind. The plugin has no outbound connections.

### VKey Discard Architecture

The privacy boundary is enforced at the `GetRawInputData` call site in `Win32InputCapture::handleRawInput()`. The raw RAWINPUT struct contains `RAWKEYBOARD.VKey` (the Virtual Key code) and `RAWKEYBOARD.MakeCode` (scan code). These fields are read from the struct and immediately discarded. Only `RI_KEY_BREAK` flag (key-up vs key-down) enters the `InputEvent` struct. This event type is the only data written to the SPSC queue. There is no code path from `VKey` to storage, log, or queue. A code review checklist in Phase 6 verifies this boundary.

### No Keystroke History

The SPSC ring buffer has a fixed capacity (e.g., 256 events). Events are consumed by the render thread each frame (at 60fps, within ~16ms). The buffer holds at most a few seconds of typing events as undifferentiated key-down/key-up counts. There is no accumulation, no persistent store, and no timestamp that would allow reconstruction of timing-based inference. The `ParameterStore` holds a typing-active boolean and a cooldown timer — no count of keystrokes.

### User Disclosure Requirement

Plugin authors should disclose in README and OBS Plugin Browser listing that the plugin uses Windows Raw Input to capture global keyboard and mouse activity for animation purposes. This is standard practice for software that uses input monitoring APIs. The disclosure should note: what is captured, what is not captured, and that no data leaves the local process.

### Windows Security Center Implications

Raw Input with `RIDEV_INPUTSINK` does not trigger Windows Defender SmartScreen on a properly signed binary. Anti-cheat software (Easy Anti-Cheat, BattlEye) typically targets injection-based hooks (`SetWindowsHookEx`) rather than the non-injecting Raw Input API. The plugin's approach was specifically chosen to avoid LL hooks for this reason. If anti-cheat interference occurs, the user can disable keyboard/mouse reactions independently (CFG-02) without affecting audio lip sync.

---

## Build & Release Strategy

### CMake Preset Workflow

The `obs-plugintemplate` scaffold provides `CMakePresets.json` with a `windows-x64` configure preset. Workflow:

```
cmake --preset windows-x64
cmake --build --preset windows-x64        # Debug
cmake --build --preset windows-x64-release  # Release
```

`buildspec.json` pins the OBS development package version. The CI downloads it automatically via the template's `cmake/BuildSpec.cmake`.

### Debug vs Release

Both configurations use `/MD` (dynamic CRT) — never `/MDd` for Debug when linking against OBS's Release CRT (causes heap corruption). Debug adds `/Zi` (PDB), `/Od` (no optimization), `_DEBUG` define, and ASAN linkage (`/fsanitize=address`). Release adds `/O2`, `/GL` (whole-program optimization), and strips PDB from the output DLL (separate PDB file for crash debugging).

### Artifact Naming and ZIP Layout

Release artifact: `AnimatedAvatarPlugin-{version}-windows.zip`

```
AnimatedAvatarPlugin-{version}-windows.zip
├── obs-plugins/
│   └── 64bit/
│       └── obs-avatar.dll
├── data/
│   └── obs-plugins/
│       └── obs-avatar/
│           └── locale/
│               └── en-US.ini
└── characters/
    └── default/
        ├── character.json
        └── textures/
            └── *.png
```

Users extract and copy both `obs-plugins/` and `data/` into their OBS installation directory.

### Versioning

Semantic versioning: `MAJOR.MINOR.PATCH`. Pre-release: `1.0.0-alpha.1`, `1.0.0-beta.1`. First public release: `1.0.0`. Version is set in `CMakeLists.txt` `project(obs-avatar VERSION 1.0.0)` and propagated via `configure_file` to a `version.h` header so it appears in `obs_module_load` log output and the OBS source properties panel.

### GitHub Actions CI Matrix

```yaml
matrix:
  os: [windows-latest]
  arch: [x64]
  config: [Debug, Release]
```

CI steps:
1. Checkout with submodules
2. Run CMake configure (`--preset windows-x64`)
3. Build Debug → run unit tests (`ctest --preset windows-x64`)
4. Build Release → package ZIP
5. Upload ZIP as workflow artifact
6. On tag push: create GitHub Release, attach ZIP

---

## Future Roadmap (Post-MVP)

### Characters
- **v2**: Visual character pack browser with thumbnail preview in OBS properties
- **v2**: Built-in character editor (position/scale layer pivots, define animation channels)
- **v2**: Keyframe animation editor (visual timeline, easing curve editor)
- **v3**: Character pack marketplace with download UI inside OBS

### Input
- **v2**: Per-key hand mapping (WASD → left hand, arrow keys → right hand) via JSON config
- **v2**: Gamepad/controller button and axis reaction animations
- **v2**: OBS event reactions (scene switch animation, recording-start fist-pump)
- **v3**: Configurable key-to-animation macro system

### Animation
- **v2**: Smooth cross-fade blending between animation tracks (replace instant-snap with lerp)
- **v2**: Expression/emotion system: happy, surprised, focused, sleepy states
- **v2**: Simulated eye tracking: eyes follow mouse cursor position (approximated from delta)
- **v3**: Webcam-driven face tracking for head pose (via MediaPipe or similar)

### Audio
- **v2**: Multiple audio source blending for lip sync (blend two mics)
- **v3**: Phoneme-approximate lip sync: basic formant analysis for vowel shape mapping
- **v3**: External speech recognition integration (Whisper) for emoji/expression triggers

### Platform
- **v2**: Linux support (evdev or X11 input, PulseAudio/JACK audio callback)
- **v2**: macOS support (CGEventTap input, CoreAudio callback)
- **v2**: Unified CMake platform abstraction layer

### Distribution
- **v2**: Windows NSIS/WiX installer with OBS auto-detection
- **v2**: OBS Plugin Browser listing (metadata, screenshots, version history)
- **v3**: Auto-update mechanism

---

## Open Questions

1. **Current OBS stable version**: Is OBS 30.x still the latest stable, or has 31.x shipped? This affects `buildspec.json` pinning and determines which `libobs` API headers apply. Resolve in Phase 1 spike (2-hour task).

2. **`obs-plugintemplate` HEAD state**: Have the CMake module filenames or `buildspec.json` schema changed since the research was conducted (mid-2025 training knowledge)? Specifically: does `CMakePresets.json` still have a `windows-x64` configure preset? Resolve in Phase 1 spike.

3. **`gs_texture_create` exact signature**: The `data` parameter type may vary by OBS version (`const uint8_t* const*` vs `uint8_t**`). Check against current `graphics/graphics.h` before Phase 2 texture loading implementation.

4. **Mouth animation model**: Discrete texture swap (4 mouth PNGs, select by state) vs UV-parameterized single mouth texture (smooth blend). The discrete model is simpler for MVP character authoring but produces visible pops between states. Decision needed before Phase 3/4 interface definition. Recommendation: discrete for MVP; UV-parameterized for v2.

5. **Default character pack art**: Will a default character pack be hand-drawn for MVP, or will placeholder colored rectangles serve as the bundled default? This affects Phase 3 deliverables and Phase 8 ZIP contents.

6. **OBS Plugin Browser listing**: Is the intent to submit to the official OBS Plugin Browser in v1.0, or defer to v2? Submission requires OBS review and metadata. Does not affect MVP code but affects Phase 8 documentation scope.

7. **Code signing**: Will the plugin DLL be Authenticode-signed for v1.0? Unsigned DLLs trigger Windows SmartScreen on first install. Signing requires a code signing certificate. Decision affects Phase 8 release checklist.

8. **Localization scope**: Is en-US the only locale for v1.0, or are other locales needed? OBS locale system supports additional `locale/*.ini` files trivially, but translation is out of scope unless native speakers are available.

---

## Phases

- [ ] **Phase 1: Repository Foundation & Build System** - Clone obs-plugintemplate, wire CMake, produce a loadable OBS plugin DLL that renders a solid color rectangle
- [ ] **Phase 2: Core Rendering Pipeline** - Implement gs_texrender_t FBO compositing, per-layer matrix transforms, stb_image PNG loading, and video_render/video_tick wiring
- [ ] **Phase 3: Character Asset System** - JSON character definition parser, texture loading per layer, character discovery and hot-reload, missing asset fallback
- [ ] **Phase 4: Animation System** - ParameterStore, AnimationClip with sparse keyframes and easing, 4-track priority blending, idle animation playing
- [ ] **Phase 5: Audio / Lip Sync** - obs_source_add_audio_capture_callback, RMS calculation, atomic audio-to-render bridge, attack/release smoothing, 4 mouth states
- [ ] **Phase 6: Global Input Capture (Windows Raw Input)** - Dedicated input thread, message-only HWND, RIDEV_INPUTSINK, SPSC ring buffer, keyboard/mouse animation triggers, clean RAII unregistration
- [ ] **Phase 7: Properties, Polish & Error Handling** - Complete OBS properties panel, all settings with defaults, debug mode, full error handling and logging throughout
- [ ] **Phase 8: Testing, Packaging & Release** - Unit test suite, integration test, manual test checklist, GitHub Actions CI, versioned ZIP artifact, README

---

## Phase Details

### Phase 1: Repository Foundation & Build System

**Goal**: A compilable OBS plugin skeleton that OBS loads cleanly and renders a solid-color rectangle, proving the build system, CMake configuration, OBS linkage, and CI scaffold are all correct before any avatar logic is written.

**Why it exists / what it unblocks**: Nothing can be built until the build system works. This phase resolves the highest-variance unknowns (current OBS version, `obs-plugintemplate` HEAD state, `buildspec.json` schema) through spikes before any substantial code is written. Every subsequent phase depends on a compilable, loadable plugin skeleton.

**Prerequisites**: VS 2022 installed with C++ Desktop workload; CMake 3.24+ on PATH; Git; access to GitHub for `obs-plugintemplate` clone.

**Detailed Task List**:
- 1.1: Clone `obsproject/obs-plugintemplate` (HEAD); read `README.md` and `buildspec.json`
- 1.2: Identify current OBS stable version; update `buildspec.json` to pin it
- 1.3: Run `cmake --preset windows-x64`; verify configure succeeds; note any CMake warnings
- 1.4: Run `cmake --build`; verify template plugin DLL builds without errors
- 1.5: Copy DLL to OBS plugins directory; launch OBS; verify plugin loads (appears in `obs_module_load` log)
- 1.6: Rename plugin to `obs-avatar`; update `CMakeLists.txt` project name, DLL output name, and OBS source registration name (`"animated_avatar_source"`)
- 1.7: Create `src/plugin-main.cpp` with `obs_module_load` / `obs_module_unload`; register a minimal `obs_source_info` that renders a 320×240 solid purple rectangle using `gs_draw_sprite` on a 1-pixel purple texture
- 1.8: Create `src/plugin-support.h` with `blog()`/`obs_log()` wrappers using `[obs-avatar]` prefix
- 1.9: Verify OBS properties panel shows the source; verify source can be added and deleted without crash
- 1.10: Create `deps/` directory; vendor `nlohmann/json.hpp` and `stb/stb_image.h`; add to CMake include paths
- 1.11: Create `.github/workflows/build.yml` with windows-x64 matrix; push to GitHub; verify CI passes
- 1.12: Document OBS version and `obs-plugintemplate` commit SHA in `README.md`

**Files/Modules Created or Modified**:
- `CMakeLists.txt` (rename, add deps include, add src files)
- `buildspec.json` (OBS version pin)
- `CMakePresets.json` (verify presets; may need minor update)
- `src/plugin-main.cpp` (new)
- `src/plugin-support.h` (new)
- `deps/nlohmann/json.hpp` (vendored)
- `deps/stb/stb_image.h` (vendored)
- `.github/workflows/build.yml` (new)
- `README.md` (initial)

**Technical Decisions Required**:
- Confirm OBS minimum version (30.x vs 31.x) — pin in `buildspec.json`
- Confirm `find_package(libobs REQUIRED)` vs `find_package(OBS REQUIRED)` in current template
- Decide plugin DLL filename: `obs-avatar.dll`

**Dependencies**:
- External: `obsproject/obs-plugintemplate` (GitHub), OBS Studio dev package (downloaded by CMake)
- Internal: None (this is phase 1)

**Risks**:
- `obs-plugintemplate` HEAD may have changed since research; CMake preset names may differ — mitigated by reading template README first
- OBS dev package download URL in `buildspec.json` may have changed — resolved by checking template issues/README

**Validation Criteria (observable)**:
1. `cmake --preset windows-x64` exits with code 0 and no errors
2. `cmake --build` produces `obs-avatar.dll`
3. OBS launches with plugin loaded; `[obs-avatar]` prefix appears in OBS log
4. User can add "Animated Avatar" source in OBS and see a solid purple rectangle
5. User can delete the source without OBS crashing
6. GitHub Actions CI build passes on push

**Definition of Done**: OBS loads the plugin, a solid-color source appears and can be added/deleted cleanly, CI is green, and a minimum-viable README documents the build process.

---

### Phase 2: Core Rendering Pipeline

**Goal**: The FBO compositing pipeline is implemented: the plugin renders multiple layered PNG textures using `gs_texrender_t`, `gs_matrix_push/pop` transforms, and `obs_source_draw`, with `video_tick` and `video_render` correctly separated.

**Why it exists / what it unblocks**: The render pipeline is the foundation of every visual feature. Phases 3–7 all produce visible output through this pipeline. Getting the threading contract (GPU only in `video_render`), alpha handling (premultiplied), and FBO compositing pattern right now prevents cascading technical debt.

**Prerequisites**: Phase 1 complete (compilable, loadable plugin skeleton).

**Detailed Task List**:
- 2.1: Verify `gs_texture_create` exact signature against current OBS `graphics/graphics.h`
- 2.2: Create `src/render/TextureCache.h/.cpp`: `loadTexture(path) → gs_texture_t*`; load PNG via `stb_image`; convert straight-alpha to premultiplied (per-pixel: `r *= a/255`, etc.); call `gs_texture_create` inside graphics lock; cache by absolute path; destroy on `clear()` inside graphics lock
- 2.3: Create `src/render/RenderCompositor.h/.cpp`: takes a list of `LayerDrawCmd{gs_texture_t*, float x, y, rot, sx, sy, opacity}`; implements FBO setup and per-layer draw loop
- 2.4: Implement `gs_texrender_t` lifecycle in `RenderCompositor`: `gs_texrender_create` in constructor (inside graphics lock); `gs_texrender_destroy` in destructor (inside graphics lock)
- 2.5: Implement per-layer matrix transform in `RenderCompositor::composite()`: `gs_matrix_push` → translate → rotate around pivot → scale → `gs_draw_sprite` → `gs_matrix_pop`
- 2.6: Wire `video_render` in `AvatarSource`: call `RenderCompositor::composite()` with a hardcoded list of 3 test textures (colored PNGs); call `obs_source_draw` to blit result to OBS output
- 2.7: Wire `video_tick` in `AvatarSource`: store `delta_time`; no `gs_*` calls here; add assertion/comment that `gs_*` are forbidden in `video_tick`
- 2.8: Implement `get_width` / `get_height` returning configurable source dimensions (default 512×512)
- 2.9: Test: add source in OBS, verify 3 colored test layers render back-to-front with correct alpha compositing
- 2.10: Test: delete source multiple times; verify no VRAM leak (GPU-Z before/after)
- 2.11: Implement `show()` / `hide()` callbacks (stub for now; mark FBO active/inactive)

**Files/Modules Created or Modified**:
- `src/render/TextureCache.h/.cpp` (new)
- `src/render/RenderCompositor.h/.cpp` (new)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: add video_render, video_tick, get_width, get_height)
- Test PNG assets in `tests/assets/` (colored rectangles for render testing)

**Technical Decisions Required**:
- Confirm `gs_texture_create` signature (see Open Question 3)
- Confirm premultiplied alpha conversion formula (CPU-side uint8 multiply before `gs_texture_create`)
- Decide source default dimensions (512×512 or 320×240)

**Dependencies**:
- Internal: Phase 1 (plugin skeleton, build system)
- External: `stb_image.h` (vendored), libobs `graphics/graphics.h`

**Risks**:
- RISK-01: `gs_*` outside graphics context — mitigated by calling `obs_enter_graphics()` in `TextureCache::loadTexture()` and `TextureCache::clear()`; only `video_render` calls compositor
- RISK-07: GPU resource leak on reload — mitigated by test 2.10 (VRAM check after delete cycles)

**Validation Criteria (observable)**:
1. OBS source renders 3 test texture layers in back-to-front order with correct alpha transparency
2. Top layer partially transparent shows layer below it correctly composited
3. Source dimensions are correct (matches configured width/height)
4. GPU-Z shows no VRAM increase after 10 delete/recreate cycles
5. No `gs_*` calls exist outside `video_render` or `obs_enter_graphics()` guards (code review)

**Definition of Done**: Multi-layer PNG compositing works correctly in OBS, alpha is correct, GPU resources clean up on destroy, and threading rules are documented and enforced.

---

### Phase 3: Character Asset System

**Goal**: The plugin reads a `character.json` file, loads the specified PNG textures, and renders the character layers in OBS. Character selection is available in OBS properties. Hot-reload works when the user changes character selection. Missing or invalid assets degrade gracefully.

**Why it exists / what it unblocks**: Before animation or input can be tested, the character definition must exist (it defines which parameter channels correspond to which layers). Phases 4–6 depend on knowing what `arm_right.rotation` means for the current character.

**Prerequisites**: Phase 2 complete (render pipeline with FBO compositing and texture loading).

**Detailed Task List**:
- 3.1: Design `character.json` schema: `version` (int), `name` (string), `layers` (array of `{id, texture, pivot_x, pivot_y, default_x, default_y, default_rotation, default_scale_x, default_scale_y, param_channels: {x, y, rotation, scale_x, scale_y, opacity}}`)
- 3.2: Create `src/engine/CharacterDefinition.h/.cpp`: `CharacterDefinition::parse(path) → std::optional<CharacterDefinition>`; uses nlohmann/json; reads version field first; validates required fields; produces detailed `LOG_ERROR` on failure; returns `std::nullopt` on any error
- 3.3: Implement `CHAR-07`: reject unsupported version with clear error message
- 3.4: Create `src/engine/CharacterDiscovery.h/.cpp`: `discover(directory) → std::vector<CharacterInfo>`; uses `std::filesystem::directory_iterator`; looks for subdirectories containing `character.json`; returns name list
- 3.5: Create default `characters/default/character.json` with placeholder character (colored rectangles as body/head/arm layers); create matching placeholder PNGs
- 3.6: Wire `CharacterDefinition` into `AvatarSource::create()` and `AvatarSource::update()`: load character on create; reload on character selection change
- 3.7: Wire `TextureCache` to load textures from `character.json` `texture` paths (relative to `character.json` directory)
- 3.8: Wire `RenderCompositor` to use `CharacterDefinition::layers` as the layer list; read default transforms from character definition
- 3.9: Implement `CHAR-04`: missing texture → `LOG_WARNING` + render layer as transparent (pass null texture to compositor, compositor skips draw for null textures)
- 3.10: Implement `CHAR-05`: invalid JSON → `LOG_ERROR` + load in blank/fallback state (empty compositor layer list)
- 3.11: Implement `CHAR-06` hot-reload: `update()` is called by OBS on every property change; compare new character path to current; if different, release old textures inside graphics lock, load new character definition and textures
- 3.12: Implement `CHAR-03` + `CFG-01`: `PropertiesPanel` calls `CharacterDiscovery::discover()` to populate character dropdown; calls `obs_property_list_add_string` for each found character
- 3.13: Test: add source, select default character, verify placeholder character renders
- 3.14: Test: point to directory with missing texture; verify warning in OBS log, layer renders transparent, no crash
- 3.15: Test: point to corrupt `character.json`; verify error in OBS log, blank source, no crash

**Files/Modules Created or Modified**:
- `src/engine/CharacterDefinition.h/.cpp` (new)
- `src/engine/CharacterDiscovery.h/.cpp` (new)
- `src/obs-integration/PropertiesPanel.h/.cpp` (new — basic character dropdown)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: wire character loading into create/update)
- `characters/default/character.json` (new)
- `characters/default/textures/*.png` (new — placeholders)

**Technical Decisions Required**:
- Finalize `character.json` schema version number (start at 1)
- Decide mouth animation model: discrete texture swap vs UV-parameterized (Open Question 4) — must decide before this phase to include correct layer naming in default character
- Decide `characters/` directory location: relative to OBS data directory or configurable absolute path

**Dependencies**:
- Internal: Phase 2 (render pipeline, texture loading)
- External: nlohmann/json (vendored), C++20 `<filesystem>`

**Risks**:
- RISK-10: Character format version incompatibility — mitigated by `version` field check first in parser
- RISK-07: GPU resource leak on hot-reload — mitigated by graphics-lock texture release in `update()`
- Mouth model decision (Open Question 4) must be resolved before authoring `character.json` schema; delaying blocks Phase 4 parameter channel naming

**Validation Criteria (observable)**:
1. Default character renders all layers in OBS from `character.json` definition
2. Changing character selection in OBS properties hot-reloads the new character within one frame
3. Deleting a texture file and reloading produces a `LOG_WARNING` and renders that layer transparent (no crash)
4. Pointing to a corrupt `character.json` produces a `LOG_ERROR` and loads a blank source (no crash)
5. Character dropdown in OBS properties shows all `characters/` subdirectories containing valid `character.json`

**Definition of Done**: Character definition parser is complete, hot-reload works, all error cases handled gracefully, and the default character renders correctly from JSON definition.

---

### Phase 4: Animation System

**Goal**: The `ParameterStore`, `AnimationClip` evaluator, and 4-track `AnimationSystem` are implemented. The idle animation plays continuously and visibly drives character layer transforms (breathing, blink, head sway). Animation state is driven by `video_tick` delta time.

**Why it exists / what it unblocks**: The animation system is the core logic that Phases 5 (audio), 6 (input), and 7 (properties) write into. Without it, there is nothing to wire audio RMS or keyboard events to.

**Prerequisites**: Phase 3 complete (character definition loaded, layer parameter channels named).

**Detailed Task List**:
- 4.1: Create `src/engine/Easing.h`: constexpr functions for `linear`, `easeIn`, `easeOut`, `easeInOut`, `hold`; all take `float u` (0–1) and return `float`
- 4.2: Create `src/engine/AnimationClip.h/.cpp`: `struct Keyframe{float time; float value; EasingType easing;}`; `AnimationClip{string channel; vector<Keyframe> keyframes; bool loop;}`; `evaluate(float t) → float`; binary search over keyframes; applies easing between bracket pair; handles loop (fmod) and one-shot (clamp + `complete_` flag)
- 4.3: Create `src/engine/ParameterStore.h/.cpp`: `unordered_map<string, float> values_`; `unordered_map<string, float> defaults_`; `set(channel, value)`, `get(channel) → float`, `resetToDefaults()`, `registerDefault(channel, float)`
- 4.4: Create `src/engine/AnimationTrack.h/.cpp`: `int priority`; `vector<AnimationClip> activeClips_`; `tick(delta) → void`: advances all clip clocks, removes completed one-shots; `writeToStore(ParameterStore&) → void`: evaluates each clip and calls `store.set(channel, value)`
- 4.5: Create `src/engine/AnimationSystem.h/.cpp`: `array<AnimationTrack, 4> tracks_` (Idle=0, Audio=1, Mouse=2, Keyboard=3); `tick(float delta)`: calls `store_.resetToDefaults()` first, then `tracks_[i].tick(delta)` and `tracks_[i].writeToStore(store_)` in priority order 0→3
- 4.6: Define idle animation clips in code (or small JSON file): breathing (`body.y` ±3px, 4s loop), head sway (`head.rotation` ±2deg, 6s loop), random blink (`blink` 0→1→0, 150ms one-shot, triggered every 3–6s randomly)
- 4.7: Implement `ANIM-07` / `ANIM-08`: idle track always active; `AnimationSystem` initializes idle clips on construction; idle never removed
- 4.8: Wire `AnimationSystem::tick(delta)` call into `AvatarSource::video_tick(delta)`
- 4.9: Wire `ParameterStore` reads into `RenderCompositor`: for each layer in character definition, read `{layer_id}.x`, `{layer_id}.y`, `{layer_id}.rotation`, `{layer_id}.scale_x`, `{layer_id}.scale_y`, `{layer_id}.opacity` from store; use as layer transform in matrix stack
- 4.10: Register `ParameterStore` defaults from `CharacterDefinition::layers[i].default_*` fields on character load
- 4.11: Implement random blink timer in idle track: `float nextBlinkTimer_`; decremented each tick; when reaches 0, push blink one-shot clip; reset to `random_uniform(3.0, 6.0)` seconds
- 4.12: Test: add source in OBS, observe idle breathing and head movement on character layers
- 4.13: Test: observe random blink at configurable interval

**Files/Modules Created or Modified**:
- `src/engine/Easing.h` (new)
- `src/engine/AnimationClip.h/.cpp` (new)
- `src/engine/ParameterStore.h/.cpp` (new)
- `src/engine/AnimationTrack.h/.cpp` (new)
- `src/engine/AnimationSystem.h/.cpp` (new)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: wire animation tick and param store reads)
- `src/render/RenderCompositor.h/.cpp` (expand: read ParameterStore for per-layer transforms)

**Technical Decisions Required**:
- Idle animation authoring: hardcoded clip data in C++ vs small JSON animation definition file
- Random blink: use `std::mt19937` seeded from `std::random_device` for per-instance variation
- Animation clip channel naming convention: `{layer_id}.rotation` vs `{layer_id}_rotation`

**Dependencies**:
- Internal: Phase 3 (character definition, layer parameter channel names, ParameterStore defaults)
- External: C++20 stdlib only (`<random>`, `<algorithm>`, `<vector>`, `<unordered_map>`)

**Risks**:
- Incorrect easing function implementations produce subtle animation artifacts; mitigated by unit tests (Phase 8 formalizes; basic sanity tests should be written now in `tests/`)
- Animation clip binary search edge case at exact keyframe time boundaries — test boundary values in unit tests

**Validation Criteria (observable)**:
1. Character layers visibly animate in idle mode: body breathes (vertical oscillation), head has subtle sway
2. Blink animation fires approximately every 3–6 seconds with correct layering on the eye layer
3. Idle animation continues running on non-conflicting channels when a higher-priority clip is manually triggered (test by temporarily hardcoding a keyboard track clip)
4. `video_tick` advances animation state; `video_render` only reads ParameterStore — no animation logic in render callback (verified by code inspection)
5. Delta-time is correctly passed from `video_tick` to `AnimationSystem::tick` (verify by slowing to 30fps and confirming animation speed is unchanged)

**Definition of Done**: Idle animation plays visibly and correctly on the default character. ParameterStore, AnimationClip, AnimationTrack, and AnimationSystem pass basic unit tests. Render pipeline reads ParameterStore and applies transforms per layer.

---

### Phase 5: Audio / Lip Sync

**Goal**: The plugin taps a user-selected OBS audio source via `obs_source_add_audio_capture_callback`, computes RMS amplitude per audio frame, communicates it to the render thread via `std::atomic<float>`, applies attack/release smoothing, and drives 4 discrete mouth states on the Audio animation track.

**Why it exists / what it unblocks**: Audio lip sync is simpler than global input (it uses the OBS audio API directly, no Win32 threading). Completing it before Phase 6 gives a working, visually complete plugin before tackling the most complex phase (Raw Input threading).

**Prerequisites**: Phase 4 complete (AnimationSystem with Audio track at priority 1, ParameterStore with `mouth_open` parameter).

**Detailed Task List**:
- 5.1: Create `src/obs-integration/AudioBridge.h/.cpp`: owns `obs_source_t* audioSource_` (weak reference, not addref); `std::atomic<float> rmsAmplitude_`; `float smoothedRms_`, `attackCoeff_`, `releaseCoeff_`, `silenceThreshold_`, `sensitivity_`
- 5.2: Implement `AudioBridge::setSource(obs_source_t*)`: remove callback from old source; add callback to new source; store weak pointer
- 5.3: Implement static `AudioBridge::audioCallback(void* param, obs_source_t*, const audio_data* data, bool muted)`: compute RMS over `data->data[0]` frames (32-bit float); apply silence threshold; apply sensitivity; write to `rmsAmplitude_.store(rms, memory_order_relaxed)`; no mutex, no alloc, no OBS API calls
- 5.4: Implement `AudioBridge::update(float delta)` (called from `video_tick`): read `rmsAmplitude_.load()`; apply attack/release smoothing formula; map smoothed RMS to mouth state enum (`mouth_closed`, `mouth_small`, `mouth_open`, `mouth_wide`) using configurable thresholds; write `mouth_open` parameter to Audio track
- 5.5: Implement audio source disconnection safety: if `audioSource_` pointer becomes invalid (OBS source removed), handle via `OBS_SIGNAL_HANDLER` `source_remove` signal or check-before-use pattern; `AUDIO-07`
- 5.6: Define `mouth_open` parameter channel mapping in `RenderCompositor`: when `mouth_open` < 0.17 show `mouth_closed` texture; < 0.5 show `mouth_small`; < 0.83 show `mouth_open`; ≥ 0.83 show `mouth_wide` — requires character to have separate mouth layer textures per state
- 5.7: Wire `AudioBridge` into `AvatarSource`: create on source create; call `setSource()` from `update()` when audio source setting changes; call `update(delta)` from `video_tick`; destroy on source destroy
- 5.8: Implement `CFG-03`: audio source dropdown in `PropertiesPanel`: populate with OBS audio sources via `obs_enum_sources`; filter to sources with audio output flag
- 5.9: Implement `CFG-04`: lip-sync controls in `PropertiesPanel`: sensitivity slider (0.1–5.0, default 1.0), silence threshold slider (0.0–0.1, default 0.01), attack time slider (1–500ms, default 10ms), release time slider (10–2000ms, default 200ms)
- 5.10: Convert attack/release ms values to per-tick coefficients in `AudioBridge::configure()`
- 5.11: Test: select microphone in OBS properties; speak; verify mouth opens and closes with speech; verify silence closes mouth promptly
- 5.12: Test: remove selected audio source from OBS sources list; verify no crash (`AUDIO-07`)

**Files/Modules Created or Modified**:
- `src/obs-integration/AudioBridge.h/.cpp` (new)
- `src/obs-integration/PropertiesPanel.h/.cpp` (expand: audio source dropdown, lip-sync sliders)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: wire AudioBridge)
- `src/render/RenderCompositor.h/.cpp` (expand: mouth state texture selection logic)
- `characters/default/textures/` (add `mouth_closed.png`, `mouth_small.png`, `mouth_open.png`, `mouth_wide.png`)
- `characters/default/character.json` (add mouth layer with state-keyed texture paths)

**Technical Decisions Required**:
- Confirm `audio_data` struct field access pattern against current libobs `media-io/audio-io.h`
- Attack/release coefficient calculation: `coeff = 1 - exp(-delta / time_constant)` vs simpler linear approximation; exponential is perceptually more natural
- Mouth layer rendering: separate texture per state (discrete swap) vs single texture with UV offset — confirm Open Question 4 resolved before this phase

**Dependencies**:
- Internal: Phase 4 (AnimationSystem Audio track, ParameterStore `mouth_open` channel)
- External: libobs `obs-source.h`, `media-io/audio-io.h`

**Risks**:
- RISK-02: Mutex on audio thread — `AudioBridge::audioCallback` must never lock; enforced by architecture (only `std::atomic` write in callback path)
- Source lifetime: `obs_source_t*` pointer from audio source dropdown may become stale if user deletes the source; mitigated by `source_remove` signal handler

**Validation Criteria (observable)**:
1. User can select a microphone from the OBS properties dropdown
2. Mouth opens visibly when the user speaks into the microphone
3. Mouth closes within the configured release time after silence
4. Mouth cycles through all 4 states (closed → small → open → wide) during a range of speaking volumes
5. Removing the selected audio source from OBS does not crash the plugin
6. Lip-sync sliders in properties visibly change mouth behavior in real time

**Definition of Done**: Lip sync pipeline works end-to-end without mutex on audio thread, all 4 mouth states drive correctly, source disconnect is handled cleanly, and audio properties panel is complete.

---

### Phase 6: Global Input Capture (Windows Raw Input)

**Goal**: A dedicated input thread with a message-only HWND captures global keyboard and mouse events via `RegisterRawInputDevices` with `RIDEV_INPUTSINK`. Key identity is discarded at the input boundary. Events flow through an SPSC ring buffer to the render thread, driving typing and mouse animations. Clean RAII unregistration on plugin unload.

**Why it exists / what it unblocks**: This is the most complex phase due to Win32 threading, privacy enforcement, and RAII lifecycle requirements. It is isolated to its own phase to contain risk. Phase 7 (properties) depends on having keyboard/mouse enable toggles that actually wire to real input capture.

**Prerequisites**: Phase 5 complete. Run Raw Input proof-of-concept spike before writing production code (per research recommendation): minimal Win32 app confirming `RIDEV_INPUTSINK` delivers background events.

**Detailed Task List**:
- 6.1: (Spike) Write `tests/spike_rawinput.cpp`: standalone Win32 console app; registers `RIDEV_INPUTSINK` on a message-only window; runs GetMessage loop; confirms events arrive when another window is focused; confirms VKey is present and discardable; run, verify, delete spike
- 6.2: Create `src/platform/InputCapture.h`: abstract interface `class IInputCapture { virtual void start(InputEventCallback) = 0; virtual void stop() = 0; virtual ~IInputCapture() = default; }`; define `InputEvent{enum EventType {KEY_DOWN, KEY_UP, MOUSE_MOVE, MOUSE_CLICK}; int16_t dx; int16_t dy; MouseButton button;}`
- 6.3: Create `src/platform/win32/SpscQueue.h`: header-only fixed-capacity SPSC ring buffer; template on element type and capacity; `push(T) → bool` (returns false if full); `pop() → std::optional<T>`; lock-free using `std::atomic<size_t>` head/tail
- 6.4: Create `src/platform/win32/Win32InputCapture.h/.cpp`: owns `HWND hwnd_` (message-only), `HANDLE thread_`, `SpscQueue<InputEvent, 256> queue_`
- 6.5: Implement `Win32InputCapture::start()`: launches `std::thread` that calls `threadProc()`; waits on a startup barrier for HWND creation to complete before returning
- 6.6: Implement `Win32InputCapture::threadProc()`: calls `CreateWindowEx(0, L"STATIC", nullptr, 0, 0,0,0,0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr)` to create message-only HWND; calls `RegisterRawInputDevices` with 2 devices (keyboard `HID_USAGE_PAGE_GENERIC / HID_USAGE_GENERIC_KEYBOARD`, mouse `HID_USAGE_GENERIC_MOUSE`), both with `RIDEV_INPUTSINK` and `hwndTarget = hwnd_`; signals startup barrier; enters `GetMessage` loop
- 6.7: Implement `Win32InputCapture::handleRawInput(LPARAM lParam)`: calls `GetRawInputData(hRawInput, RID_INPUT, ...)` — for keyboard: reads `raw.data.keyboard.Flags`; derives `KEY_DOWN` or `KEY_UP` from `RI_KEY_BREAK`; **discards `raw.data.keyboard.VKey` and `raw.data.keyboard.MakeCode`**; constructs `InputEvent{KEY_DOWN|KEY_UP, 0, 0, NONE}` and pushes to queue. For mouse: reads `lLastX`, `lLastY`; reads button flags; constructs `InputEvent{MOUSE_MOVE|MOUSE_CLICK, dx, dy, button}` and pushes to queue.
- 6.8: Implement `Win32InputCapture::stop()`: posts `WM_QUIT` to `hwnd_`; joins thread; calls `RIDEV_REMOVE` for both devices; calls `DestroyWindow(hwnd_)`
- 6.9: Create `src/platform/null/NullInputCapture.h/.cpp`: no-op implementation; `start()` and `stop()` do nothing; used in unit test builds and non-Windows platforms
- 6.10: Create `src/obs-integration/InputBridge.h/.cpp`: holds reference to `IInputCapture`; `drain(AnimationSystem&, float delta)` called from `video_tick`; pops all events from queue; maintains `keyPressActive_` bool and `cooldownTimer_` float; on `KEY_DOWN`: sets `keyPressActive_ = true`, resets `cooldownTimer_`; on tick: if `keyPressActive_` and `cooldownTimer_ > 0`, writes typing clip to Keyboard track; decrements `cooldownTimer_`; when reaches 0, returns arms to baseline
- 6.11: Implement mouse delta accumulation in `InputBridge`: accumulates `dx`/`dy` over current tick; scales by mouse intensity setting; writes `hand_offset_x` / `hand_offset_y` to Mouse track; decays toward 0 each tick for smooth return-to-neutral
- 6.12: Implement click one-shots in `InputBridge`: on `MOUSE_CLICK` event, push a short one-shot click animation clip to Mouse track for the appropriate button
- 6.13: Wire `Win32InputCapture` into `AvatarSource`: create on source create (if keyboard or mouse enabled); start input capture; wire `InputBridge::drain()` call in `video_tick`; stop and destroy on source destroy
- 6.14: Implement enable/disable: `update()` checks keyboard/mouse enable settings; if disabled, calls `stop()` on input capture (or creates `NullInputCapture`); if enabled, creates `Win32InputCapture` and calls `start()`
- 6.15: Test: type in any application; verify arm animation activates in OBS preview
- 6.16: Test: move mouse; verify hand offset animation
- 6.17: Test: click left/right/middle; verify click animations
- 6.18: Test: reload plugin (disable/re-enable in OBS settings); verify no access violation on second load

**Files/Modules Created or Modified**:
- `src/platform/InputCapture.h` (new — interface)
- `src/platform/win32/SpscQueue.h` (new)
- `src/platform/win32/Win32InputCapture.h/.cpp` (new)
- `src/platform/null/NullInputCapture.h/.cpp` (new)
- `src/obs-integration/InputBridge.h/.cpp` (new)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: wire input capture lifecycle)
- `CMakeLists.txt` (add platform/win32 sources conditionally on WIN32)

**Technical Decisions Required**:
- Startup barrier implementation: `std::promise`/`std::future` or manual `std::atomic<bool>` spin
- SPSC queue capacity: 256 events (human HCI rate at 60fps renders ~16 events/frame max in sustained typing)
- Mouse delta scaling factor: start at 0.5px per raw count; tune in testing

**Dependencies**:
- Internal: Phase 4 (AnimationSystem Keyboard/Mouse tracks, ParameterStore)
- External: Win32 API (`<windows.h>`, `<hidusage.h>`); C++20 `<thread>`, `<atomic>`

**Risks**:
- RISK-03: Raw Input unregistration crash — mitigated by `stop()` posting WM_QUIT and joining thread before RIDEV_REMOVE; test 6.18 validates
- RISK-06: Privacy violation (VKey logged) — mitigated by strict discard in `handleRawInput()`; Phase 6 code review checklist
- RISK-04: Anti-cheat software interference — documented limitation; not a plugin bug; mitigated by enable/disable toggle

**Validation Criteria (observable)**:
1. Typing in any application (including OBS itself) activates arm animation in OBS source preview
2. Typing animation persists while typing; stops after 500ms cooldown of no key presses
3. Mouse movement drives hand offset animation; hand returns to neutral when mouse is still
4. Left, right, and middle clicks each produce a visible click animation
5. Disabling keyboard reactions in OBS properties stops arm animation (mouse still works)
6. Disabling mouse reactions stops hand movement (keyboard still works)
7. Reloading OBS or the plugin produces no crash; subsequent typing and mouse movement work correctly
8. OBS log shows no VKey values at any log level — only event types

**Definition of Done**: Global Raw Input capture works correctly without kernel hooks, privacy enforcement is verified by code review, RAII cleanup passes plugin reload test, and all 5 input interaction modes animate correctly.

---

### Phase 7: Properties, Polish & Error Handling

**Goal**: The complete OBS properties panel is implemented with all settings, defaults, and validation. Comprehensive error handling and logging (`[obs-avatar]` prefix, log categories, debug mode gate) are applied throughout all prior phases. Graceful degradation paths are tested.

**Why it exists / what it unblocks**: Properties and error handling are deliberately done last so they harden a working system rather than scaffolding speculative features. By Phase 7, every system is proven to work; this phase makes it production-quality.

**Prerequisites**: Phases 1–6 complete (all subsystems functional).

**Detailed Task List**:
- 7.1: Complete `PropertiesPanel`: implement all CFG-01 through CFG-07 properties
  - Character dropdown (CHAR-03 discovery, hot-reload)
  - Source width/height integer inputs
  - Scale float slider (0.1–4.0, default 1.0)
  - Enable/disable toggles: keyboard, mouse, microphone
  - Audio source dropdown (AUDIO-01)
  - Lip-sync controls: sensitivity, threshold, attack, release (CFG-04)
  - Animation intensity sliders: keyboard, mouse, idle (CFG-05)
  - Debug mode toggle (CFG-06)
  - Property groups: collapsible groups for "Animation", "Audio / Lip Sync", "Input", "Advanced"
- 7.2: Implement `get_defaults` callback (CFG-07): set all `obs_data_t` defaults for every setting; verify first-time use shows correct default values
- 7.3: Review and harden all `LOG_*` calls across all modules:
  - Every `obs_enter_graphics()` failure → `LOG_ERROR` + return false
  - Every `nlohmann::json::parse` exception → `LOG_ERROR` with path and exception message
  - Character loaded → `LOG_INFO [character]`
  - Missing texture → `LOG_WARNING [character]`
  - Input capture start/stop → `LOG_INFO [input]`
  - Audio source connect/disconnect → `LOG_INFO [audio]`
  - Source created/destroyed → `LOG_INFO [init]`
- 7.4: Implement debug mode gate (LOG-03): add `bool debugMode_` member to `AvatarSource`; per-frame log calls (`[render]` frame time, `[animation]` param values) are wrapped in `if (debugMode_)` guard
- 7.5: Implement graceful degradation for all LOG-04 cases:
  - GPU resource failure (`gs_texture_create` returns null): `LOG_WARNING [render]` + skip layer draw
  - `nlohmann::json` parse error: `LOG_ERROR [character]` + load blank source
  - `RegisterRawInputDevices` failure: `LOG_WARNING [input]` + continue without input capture (no crash)
  - Audio callback delivers null `data->data[0]`: `LOG_WARNING [audio]` (once only, not per-frame) + skip RMS computation
- 7.6: Implement LOG-05: `obs_module_load` returns false with `LOG_ERROR` if libobs API version check fails
- 7.7: Audit all `update()` paths for idempotency: verify no double-allocation of GPU resources on repeated `update()` calls
- 7.8: Audit OBS source duplication (OBS-04): add `copy` callback to `obs_source_info`; ensure textures and state are properly duplicated (deep copy of character definition; `TextureCache` reload rather than pointer copy)
- 7.9: Add locale strings to `data/locale/en-US.ini` for all OBS properties labels and tooltips
- 7.10: Test: first-time use (no saved settings); verify all defaults load correctly
- 7.11: Test: OBS restart; verify all settings persist via serialization
- 7.12: Test: duplicate source; verify both instances animate independently
- 7.13: Test: debug mode on; verify per-frame log output appears in OBS log

**Files/Modules Created or Modified**:
- `src/obs-integration/PropertiesPanel.h/.cpp` (major expansion: all CFG-01–07)
- `src/obs-integration/AvatarSource.h/.cpp` (expand: get_defaults, copy callback, duplication support)
- `src/plugin-support.h` (expand: log category macros, debug mode helpers)
- `data/locale/en-US.ini` (complete all labels)
- All subsystem files (audit and add missing LOG_* calls)

**Technical Decisions Required**:
- Property group/collapse UI: OBS supports `obs_property_group_type` (collapsible sections); decide which settings to group and whether to use groups
- `copy` callback implementation: whether to reload textures from disk or share the `TextureCache` instance (separate caches are simpler and avoid shared-ownership complexity)

**Dependencies**:
- Internal: All prior phases (every system gets logging hardened)
- External: libobs properties API, locale system

**Risks**:
- `update()` non-idempotency causing texture leaks — mitigated by task 7.7 audit
- Missing `get_defaults` causing first-time use crashes — mitigated by task 7.10 test

**Validation Criteria (observable)**:
1. First-time source add shows all default values in properties (no blank/zero fields)
2. All settings persist correctly across OBS restart
3. Duplicate source creates an independent instance that animates separately
4. Debug mode toggle enables/disables per-frame log output visibly in OBS log
5. Every error scenario (missing asset, bad JSON, input registration failure, GPU failure) produces exactly one log message at appropriate severity with `[obs-avatar]` prefix — no crash in any case
6. All OBS properties labels display localized strings from `en-US.ini` (no raw key strings visible)

**Definition of Done**: Properties panel is complete and polished, all error cases produce graceful degradation with appropriate log messages, source duplication works, and the plugin is ready for end-to-end integration testing.

---

### Phase 8: Testing, Packaging & Release

**Goal**: Unit test suite (GoogleTest), integration test, and manual test checklist are complete. GitHub Actions CI runs tests and produces a versioned ZIP artifact on every build. README and minimal documentation are complete.

**Why it exists / what it unblocks**: This phase turns a working plugin into a distributable, verifiable artifact. CI catches regressions. The ZIP is the deliverable that end users install.

**Prerequisites**: Phase 7 complete (all systems functional and polished).

**Detailed Task List**:
- 8.1: Add GoogleTest (or Catch2) to `tests/CMakeLists.txt`; add `enable_testing()` and `add_test()` in top-level CMake; wire `NullInputCapture` for test builds (no Win32 dependency)
- 8.2: Write `tests/test_easing.cpp` (all 5 modes, boundary values — per Testing Strategy)
- 8.3: Write `tests/test_animation_clip.cpp` (evaluation, loop, one-shot, binary search edge cases)
- 8.4: Write `tests/test_parameter_store.cpp` (priority blending, default reset, multi-parameter frame)
- 8.5: Write `tests/test_rms.cpp` (RMS formula, silence threshold, attack/release, sensitivity)
- 8.6: Write `tests/test_character_parser.cpp` (valid/invalid/missing/wrong-version JSON)
- 8.7: Run all unit tests; fix all failures; achieve 100% pass rate
- 8.8: Implement integration test `tests/test_integration_load.cpp` (per Testing Strategy); requires OBS test harness or mock; stub if OBS test harness unavailable
- 8.9: Set plugin version `1.0.0-alpha.1` in `CMakeLists.txt`; propagate to `version.h` via `configure_file`; display in `obs_module_load` log and as a read-only property in the panel
- 8.10: Implement ZIP packaging in CI: `cmake --install` to staging directory; zip with correct layout (per Build Strategy ZIP layout)
- 8.11: Update `.github/workflows/build.yml`: add `ctest` step; add artifact upload for ZIP; add tag-triggered release step
- 8.12: Write `README.md`: install instructions, OBS version requirements, properties documentation, default character pack format specification, privacy notice, known limitations (anti-cheat)
- 8.13: Validate ZIP: extract to OBS directory, launch OBS, verify plugin loads without other steps
- 8.14: Execute manual test checklist (per Testing Strategy section) and record results
- 8.15: Fix all blockers found during manual testing

**Files/Modules Created or Modified**:
- `tests/CMakeLists.txt` (new)
- `tests/test_easing.cpp` (new)
- `tests/test_animation_clip.cpp` (new)
- `tests/test_parameter_store.cpp` (new)
- `tests/test_rms.cpp` (new)
- `tests/test_character_parser.cpp` (new)
- `tests/test_integration_load.cpp` (new or stubbed)
- `CMakeLists.txt` (version propagation, enable_testing)
- `src/version.h.in` (configure_file template)
- `.github/workflows/build.yml` (ctest, artifact upload, release)
- `README.md` (complete)

**Technical Decisions Required**:
- Test framework: GoogleTest (easier CI integration via FetchContent) vs Catch2 (header-only, no build step) — recommend GoogleTest for CMake integration
- OBS integration test harness: whether to use `obs_init` in test mode or write a minimal mock; if OBS test harness is too complex to set up in CI, stub the integration test and document as manual-only

**Dependencies**:
- Internal: All prior phases (tests exercise all modules)
- External: GoogleTest (FetchContent in CMake), CMake `ctest`

**Risks**:
- Integration test requiring OBS in CI is complex; mitigated by stubbing if CI OBS setup is impractical
- Manual test checklist failures requiring Phase 7 fixes — budget time for at least one round-trip

**Validation Criteria (observable)**:
1. `ctest` on the CI matrix reports 0 failures across all unit tests
2. GitHub Actions CI passes on every commit to main branch
3. Release ZIP artifact is produced on tag push and named correctly (`AnimatedAvatarPlugin-1.0.0-alpha.1-windows.zip`)
4. Installing the ZIP (extract to OBS directory) and launching OBS loads the plugin with no additional steps
5. All manual test checklist items pass (keyboard, mouse, audio, OBS lifecycle sections)
6. README is complete and accurate (install path, OBS version, privacy notice)

**Definition of Done**: Unit tests pass in CI, release ZIP installs and works correctly from cold extract, manual checklist is fully green, and README documents the plugin for end users.

**UI hint**: no

---

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Repository Foundation & Build System | 0/3 | Not started | - |
| 2. Core Rendering Pipeline | 0/3 | Not started | - |
| 3. Character Asset System | 0/3 | Not started | - |
| 4. Animation System | 0/3 | Not started | - |
| 5. Audio / Lip Sync | 0/3 | Not started | - |
| 6. Global Input Capture (Windows Raw Input) | 0/3 | Not started | - |
| 7. Properties, Polish & Error Handling | 0/3 | Not started | - |
| 8. Testing, Packaging & Release | 0/3 | Not started | - |
