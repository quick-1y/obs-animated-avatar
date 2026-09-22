# OBS Animated Avatar Plugin

## What This Is

A native OBS Studio plugin that provides an animated 2D layered avatar as a first-class OBS Source. The avatar reacts in real time to keyboard input, mouse movement and clicks, and microphone audio amplitude — behaving like a simple animated character sitting at a computer. Users add it via Sources → Animated Avatar with no separate application required.

## Core Value

A zero-overhead, privacy-safe native OBS avatar that reacts to user activity in real time without requiring a separate capture window, external application, or game engine.

## Architecture Principle

Two major layers:
- **Avatar Engine** — reusable core handling state, animation, blending, rendering, audio analysis, and character definitions. No hard OBS dependency.
- **OBS Integration** — thin wrapper registering the OBS Source, managing properties/settings/lifecycle, forwarding audio data, and driving the render callback.

## Requirements

### Validated

(None yet — ship to validate)

### Active

**OBS Integration**
- [ ] Native OBS Source registerable via Sources → Add → Animated Avatar
- [ ] Source creation, destruction, rendering, properties, settings, serialization, duplication
- [ ] OBS audio input (user-selectable audio source for lip-sync)
- [ ] OBS properties panel for all user-facing configuration

**Rendering**
- [ ] Layered 2D rendering via libobs graphics API (textures, effects, GPU transforms)
- [ ] Correct draw order, alpha transparency, per-layer transforms (position, rotation, scale, opacity)
- [ ] Texture caching and proper GPU resource lifetime management

**Character Asset System**
- [ ] External character definition (JSON or chosen format): layers, pivots, default transforms, animation refs
- [ ] Multiple character pack support with discovery, validation, loading, fallback
- [ ] Texture loading from character directory (PNG with alpha)

**Animation System**
- [ ] Keyframe animation with duration, interpolation/easing, looping, one-shot
- [ ] Animation parameter system (mouth_open, head_rotation, arm_position, etc.)
- [ ] Multi-track blending: keyboard→arms/hands, mouse→right hand, audio→mouth, idle→body/head
- [ ] Animation priorities and conflict resolution

**Input — Keyboard (Windows)**
- [ ] Global keyboard event capture (key down/up) without recording key content
- [ ] Generic typing animation; architecture supports key-specific hand mapping
- [ ] Privacy boundary: event type only, no keystroke history

**Input — Mouse (Windows)**
- [ ] Global mouse delta capture (dx/dy), left/right/middle click events
- [ ] Delta-driven hand animation with smooth return-to-neutral

**Audio / Lip Sync**
- [ ] OBS audio sample access from selected source
- [ ] RMS/peak amplitude calculation with smoothing, attack/release, noise threshold
- [ ] Discrete mouth states: closed, small, open, wide
- [ ] Configurable sensitivity, threshold, smoothing parameters

**Idle Animation**
- [ ] Base idle loop (breathing, slight body/head movement, blinking)
- [ ] Idle continues as background layer during active interactions

**Configuration**
- [ ] OBS properties: character, scale, enable/disable keyboard/mouse/mic, audio source, lip-sync params, animation intensities, debug mode
- [ ] Settings persist via OBS serialization

**Quality**
- [ ] Logging via OBS log facilities (init, character, input, audio, animation, rendering)
- [ ] Graceful error handling: missing assets, invalid config, unavailable APIs, GPU failures
- [ ] Unit tests (animation math, audio processing, config parsing) and integration tests
- [ ] Windows-first; platform abstraction for future Linux/macOS

### Out of Scope (MVP)

- Speech recognition — not needed, amplitude-only lip sync
- Linux/macOS implementations — architecture allows it, not built now
- Visual animation editor — future feature
- Webcam/eye/face tracking — future feature
- Key-specific hand mapping UI — architecture supports, not exposed in MVP
- Marketplace/scripting integrations — future feature
- Qt, Electron, game engine dependencies — explicitly excluded

## Context

- Target: OBS Studio (latest stable, plugin API current as of 2024-2025)
- Language: C++20, CMake
- Platform: Windows 11/10 first-class, MSVC compiler
- libobs provides the graphics context, audio pipeline, and source lifecycle
- Global input capture on Windows requires careful API selection (Raw Input vs hooks) and privacy design
- Avatar Engine must be decoupled from OBS so it could theoretically power a standalone app

## Constraints

- **Tech stack**: C++20 + CMake + libobs only; no Qt/game engine/Electron
- **Privacy**: keyboard events used for animation only; no keystroke content stored or transmitted
- **Performance**: plugin must not measurably impact OBS streaming performance; render path must never block on input/audio
- **Compatibility**: must work as a standard OBS plugin installable from a ZIP or installer
- **Platform**: Windows-first; no Linux/macOS code in MVP unless required by architecture
- **Dependencies**: minimize external deps; prefer vendored-small or header-only libraries

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Two-layer architecture (Engine + OBS Integration) | Decouples reusable avatar logic from OBS-specific APIs | — Pending |
| Windows Raw Input vs low-level hooks for keyboard/mouse | Requires research — performance, security, anti-cheat, focus behavior differ | — Pending |
| libobs graphics API vs external renderer | Investigate in research phase | — Pending |
| Character definition format (JSON vs alternatives) | Investigate in research phase | — Pending |
| Animation blending model (parameter ownership/priorities) | Core architectural decision for multi-track animations | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-09-22 after initialization*
