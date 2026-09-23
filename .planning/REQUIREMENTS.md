# Requirements: OBS Animated Avatar Plugin

**Defined:** 2026-09-22
**Core Value:** A zero-overhead, privacy-safe native OBS avatar that reacts to user activity in real time without requiring a separate capture window, external application, or game engine.

---

## v1 Requirements

### OBS Integration

- [x] **OBS-01**: Plugin registers as a native OBS Source (`Sources → Add → Animated Avatar`) using `obs_source_info` with `OBS_SOURCE_TYPE_INPUT | OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`
- [ ] **OBS-02**: Source supports full OBS lifecycle: `create`, `destroy`, `update`, `get_width`, `get_height`, `video_render`, `video_tick`, `show`, `hide`
- [ ] **OBS-03**: Source settings and all user configuration persists across OBS restarts via `obs_data_t` serialization
- [ ] **OBS-04**: Source can be duplicated within a scene and used in multiple scenes simultaneously
- [ ] **OBS-05**: Source exposes a properties panel (`obs_properties_t`) with all user-facing configuration options
- [x] **OBS-06**: Plugin loads and unloads cleanly via `obs_module_load` / `obs_module_unload` without crashing OBS on repeated load/unload cycles
- [ ] **OBS-07**: All GPU resource creation and destruction occurs inside `obs_enter_graphics()` / `obs_leave_graphics()` guards (or inside `video_render`) to prevent graphics context crashes

### Rendering

- [ ] **REND-01**: Avatar renders as a composited 2D layered image using `gs_texrender_t` off-screen FBO; layers are blitted to OBS output in `video_render`
- [ ] **REND-02**: Each character layer supports independent transform: position (x, y), rotation (degrees), scale (x, y), opacity (0.0–1.0)
- [ ] **REND-03**: Layer draw order matches the order defined in `character.json` (back-to-front)
- [ ] **REND-04**: PNG textures with straight alpha are converted to premultiplied alpha at load time for correct OBS compositor blending
- [ ] **REND-05**: Textures are loaded via `stb_image` into `gs_texture_t` on the graphics thread; textures are cached and not reloaded every frame
- [ ] **REND-06**: GPU resources are destroyed on source destruction inside the graphics lock; no GPU resource leaks on source delete or OBS restart
- [ ] **REND-07**: Rendering performance does not degrade with up to 12 simultaneous character layers at 1080p/60fps

### Character Asset System

- [ ] **CHAR-01**: Character is defined by an external `character.json` file specifying: layer list, layer ordering, texture paths, anchor/pivot points, default transforms, animation clip references
- [ ] **CHAR-02**: Character textures are loaded from a `textures/` subdirectory relative to `character.json`
- [ ] **CHAR-03**: Character discovery: plugin scans a configurable `characters/` directory and lists available characters in the OBS properties dropdown
- [ ] **CHAR-04**: Missing texture files produce an OBS log warning and render the layer as transparent (not a crash)
- [ ] **CHAR-05**: Invalid or unparseable `character.json` produces an OBS log error and loads the plugin in a safe fallback state (blank source, no crash)
- [ ] **CHAR-06**: Character can be hot-reloaded when the user changes the character selection in OBS properties
- [ ] **CHAR-07**: Character format version field in `character.json`; unsupported versions produce a clear log error

### Animation System

- [ ] **ANIM-01**: Animation clips are defined as sparse keyframe sequences per named parameter channel (e.g., `right_arm.rotation`, `mouth_open`, `body.y`)
- [ ] **ANIM-02**: Keyframe interpolation supports: linear, ease-in, ease-out, ease-in-out (cubic), and hold (step)
- [ ] **ANIM-03**: Animation clips support looping and one-shot modes; one-shot clips return to baseline after completion
- [ ] **ANIM-04**: Animation state is advanced in `video_tick` using delta-time (seconds); not in `video_render`
- [ ] **ANIM-05**: A `ParameterStore` holds all named animation parameters as floats or 2D vectors; renderer reads from it each frame
- [ ] **ANIM-06**: At least 4 independent animation tracks with priority: Idle (lowest), Audio, Mouse, Keyboard (highest). Higher-priority tracks override lower-priority tracks on the same parameter channel
- [ ] **ANIM-07**: Idle animation plays continuously as the base layer: breathing, subtle body/head movement, and random blink at configurable interval
- [ ] **ANIM-08**: Idle animation continues playing on non-conflicting parameter channels while interaction animations play on other channels

### Audio / Lip Sync

- [ ] **AUDIO-01**: User can select any OBS audio source (e.g., Mic/Aux) from the properties panel; plugin taps it via `obs_source_add_audio_capture_callback`
- [ ] **AUDIO-02**: Audio callback computes RMS amplitude per frame (32-bit float planar PCM); value is communicated to the animation thread via `std::atomic<float>` (no mutex on audio thread)
- [ ] **AUDIO-03**: Mouth parameter is driven by smoothed RMS amplitude with configurable attack and release time constants
- [ ] **AUDIO-04**: Silence threshold: RMS values below threshold snap mouth to closed; eliminates ambient noise driving the mouth open
- [ ] **AUDIO-05**: Mouth maps to 4 discrete states: `mouth_closed`, `mouth_small`, `mouth_open`, `mouth_wide`, driven by configurable amplitude thresholds
- [ ] **AUDIO-06**: User-configurable lip-sync sensitivity multiplier (scales RMS before threshold comparison)
- [ ] **AUDIO-07**: When no audio source is selected or the selected source is removed, lip-sync silently disables; no crash

### Keyboard Input (Windows)

- [ ] **KBD-01**: Global keyboard events (key-down, key-up) are captured via Windows Raw Input API (`RegisterRawInputDevices` with `RIDEV_INPUTSINK` flag) on a dedicated input thread with a message-only window
- [ ] **KBD-02**: Input thread discards all key identity information (VKey, scan code) at the boundary; only `{event_type: key_down|key_up}` enters the SPSC ring buffer to the animation thread
- [ ] **KBD-03**: Keyboard activity triggers a typing animation on the arms/hands animation track
- [ ] **KBD-04**: A "typing cooldown" period (configurable, default 500ms): avatar returns arms/hands to neutral after cooldown of no key presses
- [ ] **KBD-05**: Keyboard capture can be enabled or disabled via OBS properties without restarting OBS
- [ ] **KBD-06**: Raw Input device is cleanly unregistered (`RIDEV_REMOVE`) and the input thread joined on source destruction or plugin unload; no dangling hooks

### Mouse Input (Windows)

- [ ] **MOUSE-01**: Global mouse movement is captured via Windows Raw Input (same device thread as keyboard); movement is communicated as relative deltas (dx, dy), not absolute coordinates
- [ ] **MOUSE-02**: Mouse delta drives a "hand offset" parameter on the mouse animation track; delta is scaled and smoothed so the hand follows movement loosely and returns to neutral when the mouse stops
- [ ] **MOUSE-03**: Left click, right click, and middle click each trigger a short one-shot animation on the hand (configurable intensity)
- [ ] **MOUSE-04**: Mouse capture can be enabled or disabled independently of keyboard capture via OBS properties
- [ ] **MOUSE-05**: Mouse input cleanup follows the same RAII/unregister pattern as keyboard input

### OBS Properties / Configuration

- [ ] **CFG-01**: Properties panel exposes: Character selection (dropdown), Source width/height (integer), Scale (float slider)
- [ ] **CFG-02**: Enable/disable toggles for: Keyboard reactions, Mouse reactions, Microphone/lip-sync
- [ ] **CFG-03**: Audio source selection dropdown (populated from available OBS audio sources)
- [ ] **CFG-04**: Lip-sync controls: sensitivity (float slider), silence threshold (float slider), attack time (float slider, ms), release time (float slider, ms)
- [ ] **CFG-05**: Animation intensity controls: keyboard intensity, mouse intensity, idle intensity (float sliders, 0.0–2.0)
- [ ] **CFG-06**: Debug mode toggle that enables verbose per-frame logging (disabled by default)
- [ ] **CFG-07**: All settings load their defaults via `get_defaults` callback on first-time use

### Error Handling & Logging

- [ ] **LOG-01**: Plugin uses `blog()` / `obs_log()` for all diagnostic output with consistent prefix `[obs-avatar]`
- [ ] **LOG-02**: Log categories: `[init]`, `[character]`, `[input]`, `[audio]`, `[animation]`, `[render]`, `[config]`; each emitted at `LOG_INFO` or `LOG_WARNING` severity as appropriate
- [ ] **LOG-03**: Frame-rate logging (per-frame debug output) is gated behind debug mode; default log level produces at most one log line per event
- [ ] **LOG-04**: Missing character files, invalid JSON, GPU resource failures, and unavailable input APIs produce `LOG_WARNING` entries and degrade gracefully (no crash)
- [ ] **LOG-05**: Unrecoverable failures (e.g., libobs API not available) produce `LOG_ERROR` and return false from `obs_module_load`

### Platform / Build

- [x] **BUILD-01**: Plugin builds on Windows with VS 2022, CMake 3.24+, C++20, using `obs-plugintemplate` scaffold and `buildspec.json` for OBS dev package pinning
- [x] **BUILD-02**: `cmake --preset windows-x64` produces a `.dll` that OBS 30.0+ can load without additional dependencies
- [x] **BUILD-03**: Platform-specific code (Raw Input, Win32 HWND) is isolated behind a `platform/` abstraction interface; engine core has no `#include <windows.h>` calls
- [x] **BUILD-04**: Debug and Release configurations supported; no CRT mismatch (`/MD` both configs)
- [ ] **BUILD-05**: Build produces versioned ZIP artifact: `AnimatedAvatarPlugin-{version}-windows.zip` containing the `.dll` and a `characters/default/` pack

### Testing

- [ ] **TEST-01**: Unit tests cover: keyframe interpolation (all easing modes), RMS calculation, smoothing (attack/release), animation track blending (priority ordering), character JSON parser
- [ ] **TEST-02**: Integration test: plugin `.dll` loads into OBS, source creates, properties render, settings persist across OBS restart, source deletes cleanly
- [ ] **TEST-03**: Manual test checklist covering all interaction modes (keyboard typing, rapid typing, mouse movement, clicks, speech, silence, background noise, scene switching, source duplication/deletion)

---

## v2 Requirements

### Multiple Characters & Editor

- **MULTI-01**: Character selection UI with preview thumbnails
- **MULTI-02**: Visual character editor for creating/modifying character packs without editing JSON manually
- **MULTI-03**: Animation clip editor (visual keyframe editor)

### Advanced Input

- **ADV-01**: Per-key hand mapping (e.g., WASD → left hand, arrow keys → right hand); configurable via JSON or UI
- **ADV-02**: Gamepad/controller input reactions
- **ADV-03**: OBS event reactions (scene switch, recording start/stop)

### Advanced Animation & Expression

- **ADV-04**: Emotion/expression system (happy, surprised, focused states)
- **ADV-05**: Eye tracking or simulated eye movement toward mouse cursor
- **ADV-06**: Webcam-driven face tracking for head pose

### Advanced Audio

- **ADV-07**: Phoneme-based lip sync (formant analysis or external speech recognition)
- **ADV-08**: Multiple audio source blending for lip sync

### Platform

- **PLT-01**: Linux support (input via evdev or X11, audio via PulseAudio/JACK callback)
- **PLT-02**: macOS support (input via CGEventTap, audio via CoreAudio)

### Distribution

- **DIST-01**: Windows NSIS/WiX installer with OBS auto-detection and install path
- **DIST-02**: OBS Plugin Browser integration (plugin browser metadata and listing)
- **DIST-03**: Character pack marketplace / download UI

---

## Out of Scope (MVP)

| Feature | Reason |
|---------|--------|
| Qt framework | No UI outside OBS properties panel needed; OBS properties API is sufficient |
| Game engine (Unity, Godot) | Introduces massive dependency; libobs GPU pipeline is adequate for 2D layered rendering |
| Speech recognition | Amplitude-based lip sync is sufficient for MVP; full ASR is v2+ |
| WH_KEYBOARD_LL hooks | Live-verified: silently removed after 1000ms on Win10 1709+. Raw Input is the correct approach |
| Absolute cursor position tracking | Privacy risk; delta-based movement is sufficient and privacy-safe |
| Keystroke content recording | Privacy boundary: key identity discarded at input thread |
| Linux / macOS implementations | Architecture designed for it; not built in MVP |
| Visual animation editor | Future feature; JSON authoring is sufficient for MVP |
| Per-key hand mapping UI | Architecture supports it; not exposed in MVP properties panel |
| Installer (.exe / NSIS) | ZIP distribution is sufficient for MVP |
| OBS Plugin Browser listing | Requires OBS review process; post-MVP |
| `OBS_SOURCE_COMPOSITE` flag | Requires `audio_render` callback; not needed for standalone avatar source |

---

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| OBS-01 – OBS-07 | Phase 1 | Pending |
| REND-01 – REND-07 | Phase 2 | Pending |
| CHAR-01 – CHAR-07 | Phase 3 | Pending |
| ANIM-01 – ANIM-08 | Phase 4 | Pending |
| AUDIO-01 – AUDIO-07 | Phase 5 | Pending |
| KBD-01 – KBD-06 | Phase 6 | Pending |
| MOUSE-01 – MOUSE-05 | Phase 6 | Pending |
| CFG-01 – CFG-07 | Phase 7 | Pending |
| LOG-01 – LOG-05 | Phase 7 | Pending |
| BUILD-01 – BUILD-05 | Phase 8 | Pending |
| TEST-01 – TEST-03 | Phase 8 | Pending |

**Coverage:**

- v1 requirements: 55 total
- Mapped to phases: 55
- Unmapped: 0 ✓

---
*Requirements defined: 2026-09-22*
*Last updated: 2026-09-22 after initial definition from spec + research*
