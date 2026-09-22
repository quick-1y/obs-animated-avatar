# Architecture Research: Layered 2D Rendering & Animation System

**Project:** OBS Animated Avatar Plugin
**Researched:** 2026-09-22
**Confidence note:** WebSearch and WebFetch were unavailable during this research session.
All findings are drawn from training knowledge of OBS Studio source code (through mid-2025),
the libobs API, C++ animation system literature, and cross-referenced internal consistency.
Confidence levels are assigned honestly per the research philosophy. Claims specific to
exact OBS API signatures should be verified against the current obs-studio GitHub source
before implementation.

---

## Layered 2D Rendering in libobs

### Coordinate System (MEDIUM-HIGH confidence)

libobs uses a **top-left origin, Y-axis pointing down** coordinate system, matching the
standard 2D screen convention. Units are pixels matching the output canvas resolution
(e.g. 1920x1080). The `obs_source_info.get_width` / `get_height` callbacks define the
source's logical dimensions; OBS scales this into the canvas.

- Origin: top-left corner of the source's bounding box
- X increases right, Y increases down
- Rotation is clockwise when viewed on screen
- The projection matrix set by OBS before calling `video_render` maps this to NDC
  (normalized device coordinates) for the underlying graphics backend (D3D11 on Windows,
  OpenGL on Linux/macOS)

### Matrix Stack: gs_matrix_push / gs_matrix_pop (HIGH confidence)

The standard, idiomatic approach for per-layer transforms in libobs plugins is:

```cpp
// Push a transform for one layer
gs_matrix_push();
gs_matrix_translate3f(pivot_x + offset_x, pivot_y + offset_y, 0.0f);
gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, rotation_radians);
gs_matrix_scale3f(scale_x, scale_y, 1.0f);
gs_matrix_translate3f(-pivot_x, -pivot_y, 0.0f); // translate back from pivot

// Draw using the current effect/shader
obs_source_draw(texture, 0, 0, width, height, false);
// or gs_draw_sprite(texture, flip_flags, width, height);

gs_matrix_pop();
```

This is the approach used by virtually every OBS plugin that draws layered 2D content
including obs-move-transition (transition overlays), StreamFX (shader-filtered layers),
and the built-in image source. The matrix stack is the canonical libobs mechanism —
do not attempt to manage transforms via shader uniforms for individual layers.

**Pivot-relative rotation:** Always translate to pivot, rotate, translate back. This is
standard and matches how animation rigs work.

### gs_draw_sprite vs obs_source_draw (MEDIUM confidence)

Two functions draw textured quads:

- `gs_draw_sprite(texture, flip, width, height)` — low-level, no effect/shader binding
  needed beyond what the caller sets up. Draws a unit quad scaled to width×height.
- `obs_source_draw(texture, x, y, cx, cy, flip)` — higher-level helper that also handles
  some coordinate mapping. Used in image-source and scene rendering internally.

**Recommendation:** Use `gs_draw_sprite` with an explicit effect (`gs_effect_t*`) for
maximum control over the shader. Load a custom `.effect` file (libobs HLSL-like shading
language) or use the built-in `obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA)` for
standard alpha-blended layers. Bind the texture to the shader's `image` parameter before
drawing.

### Premultiplied Alpha (HIGH confidence)

libobs uses **premultiplied alpha internally**. This is a load-bearing architectural fact:

- OBS's compositing pipeline assumes all source output is premultiplied alpha
- PNG files loaded from disk are **straight alpha** (standard PNG format)
- You **must** convert straight-alpha PNGs to premultiplied alpha either at load time or
  in a shader

**At load time (CPU, done once):**
```cpp
// After decoding PNG pixels (r, g, b, a per channel, 0-255):
for each pixel:
    r = (r * a + 127) / 255;
    g = (g * a + 127) / 255;
    b = (b * a + 127) / 255;
// then upload to GPU texture
```

**In shader (GPU, every frame — avoid for static textures):**
Use OBS's built-in premultiplied alpha effect: `obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA)`
— this effect applies the premultiplication in the shader when drawing. This is acceptable
if you cannot convert at load time, but creates unnecessary per-frame GPU work for static
avatar layer textures.

**Recommended approach:** Convert at texture load time (CPU, once). Upload as
premultiplied. Use `OBS_EFFECT_DEFAULT` or a custom effect for rendering.

**Blend state:** OBS sets `GS_BLEND_ONE, GS_BLEND_INVSRCALPHA` (premultiplied alpha
blending) as the default blend state before source `video_render`. Do not change this
unless you fully understand the compositing implications.

### Off-Screen Compositing with gs_texrender_t (HIGH confidence)

A plugin can absolutely create render targets (FBOs) to composite layers off-screen before
presenting to OBS. This is the correct approach for complex effects, post-processing, or
when you need to sample the composited result in a subsequent shader pass.

```cpp
// In source_create or first render:
gs_texrender_t* texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

// In video_render:
if (gs_texrender_begin(texrender, output_width, output_height)) {
    // Clear with transparent black (premultiplied: all zeros)
    vec4 zero = {0};
    gs_clear(GS_CLEAR_COLOR, &zero, 1.0f, 0);

    // Set up orthographic projection matching output size
    gs_ortho(0.0f, (float)output_width, 0.0f, (float)output_height, -100.0f, 100.0f);

    // Draw all layers in order (back to front)
    for (auto& layer : layers_back_to_front) {
        draw_layer(layer);
    }

    gs_texrender_end(texrender);
}

// Get the composited texture and draw to OBS output
gs_texture_t* tex = gs_texrender_get_texture(texrender);
// Draw tex to the final output using gs_draw_sprite or obs_source_draw
```

**Important:** Call `gs_texrender_reset(texrender)` each frame before `gs_texrender_begin`
to invalidate the cached texture and force a redraw.

**Lifetime:** Destroy with `gs_texrender_destroy(texrender)` in `source_destroy`.

**Verdict for this project:** Use `gs_texrender_t` for the avatar composition. Each frame,
composite all layers into the render target, then blit to OBS output. This is cleaner than
drawing directly to OBS output when N layers have different blend modes or effects.

---

## Character Asset Format Decision

### Comparison: JSON vs TOML vs Binary

| Criterion | JSON (nlohmann/json) | TOML (toml++) | Binary (custom) |
|-----------|---------------------|---------------|-----------------|
| Human authoring | Good — universal tooling | Excellent — comments, clean syntax | Not viable |
| Version control diffs | Acceptable — noisy on whitespace | Excellent — cleaner structure | Binary diffs useless |
| C++ parsing deps | nlohmann/json (header-only, ~1MB) | toml++ (header-only, ~500KB) | Custom code |
| Parse speed | Fast enough for 1-time load | Fast enough for 1-time load | Fastest |
| Schema validation | Manual or JSON Schema (separate) | Manual | Manual |
| Tooling ecosystem | Massive (editors, linters, validators) | Growing | None |
| Embeddability | Yes, header-only | Yes, header-only | Yes |
| Comments | No | Yes | N/A |
| Complexity for nested data | Good | Good for flat, awkward for deep arrays | Custom |

**Recommendation: JSON with nlohmann/json.**

Rationale:
- The character definition file is loaded once at plugin startup — parse speed is irrelevant
- JSON is the universal choice in game asset pipelines (Unity, Godot, Spine all use JSON for assets)
- nlohmann/json is a single header-only include with no external deps — fits the "minimize deps" constraint
- Tool support is unmatched: VS Code, JSON Schema validators, Python scripts for asset tools
- TOML's advantage (comments) matters for config files; for structured asset data with arrays of keyframes,
  JSON is superior
- Binary format offers no meaningful advantage for a file loaded once per character switch

**File structure recommendation:**
```json
{
  "version": 1,
  "name": "DefaultCharacter",
  "canvas": { "width": 512, "height": 512 },
  "layers": [
    {
      "id": "body",
      "texture": "body.png",
      "pivot": [256, 320],
      "default_transform": { "position": [0, 0], "rotation": 0.0, "scale": [1.0, 1.0], "opacity": 1.0 },
      "z_order": 0
    }
  ],
  "parameters": {
    "mouth_open": { "type": "float", "range": [0.0, 1.0], "default": 0.0 },
    "head_rotation": { "type": "float", "range": [-45.0, 45.0], "default": 0.0 }
  },
  "animations": [
    { "id": "idle", "file": "anim_idle.json", "loop": true }
  ]
}
```

---

## PNG Texture Loading

### Options Compared

| Option | Deps | Complexity | Windows-specific | Verdict |
|--------|------|------------|------------------|---------|
| stb_image | None (header-only) | Very low | No | **Best** |
| libpng | External CMake dep | Medium | No | Avoid |
| WIC (Windows Imaging Component) | COM/Windows SDK | High | Yes — breaks Linux future | Avoid |
| OBS built-in (obs_load_module_file + image-source internals) | libobs internal | Medium; fragile | No | Avoid |

**Recommendation: stb_image (stb_image.h, single header).**

```cpp
// stb_image usage:
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int width, height, channels;
// Force 4 channels (RGBA)
unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
if (!data) {
    // log error
    return nullptr;
}

// Convert straight alpha to premultiplied (CPU, once)
for (int i = 0; i < width * height; i++) {
    unsigned char a = data[i*4 + 3];
    data[i*4 + 0] = (uint8_t)((data[i*4 + 0] * a + 127) / 255);
    data[i*4 + 1] = (uint8_t)((data[i*4 + 1] * a + 127) / 255);
    data[i*4 + 2] = (uint8_t)((data[i*4 + 2] * a + 127) / 255);
}

// Upload to GPU (must be on render thread — see threading section)
obs_enter_graphics();
gs_texture_t* tex = gs_texture_create(width, height, GS_RGBA, 1,
                                       (const uint8_t**)&data, 0);
obs_leave_graphics();

stbi_image_free(data);
```

stb_image is the standard choice for exactly this use case: header-only, no deps, handles
PNG/JPEG/etc., reliable, already vendored in dozens of OBS plugins.

### Texture Atlas vs Individual Files

For an avatar with ~10 layers:

**Individual PNG files per layer — recommended for MVP.**

Rationale:
- 10 textures is trivially small; GPU texture binding overhead at 10 draw calls per frame
  at 60fps is negligible (microseconds)
- Individual files allow hot-reloading of individual layers without invalidating the whole atlas
- Simpler loading code, no UV coordinate math required
- Atlases provide measurable benefit at 100+ sprites per frame with batched draw calls —
  not this use case

**When to switch to an atlas:** If future animation requires many mouth shapes (10+ frames
of animation where the mouth is a separate texture per shape), an atlas for just the
animated-region textures would reduce texture switches. Not needed for MVP.

---

## Animation System Design

### Minimal Viable Keyframe System

**Data structures:**

```cpp
enum class EasingType { Linear, EaseIn, EaseOut, EaseInOut, CubicBezier };

struct Keyframe {
    float time;          // seconds from clip start
    float value;         // parameter value at this keyframe
    EasingType easing;   // interpolation to NEXT keyframe
    // For CubicBezier:
    float cp1t, cp1v;    // control point 1 (time, value)
    float cp2t, cp2v;    // control point 2 (time, value)
};

struct ParameterChannel {
    std::string parameter_id;  // e.g. "mouth_open"
    std::vector<Keyframe> keyframes;  // sorted by time
};

struct AnimationClip {
    std::string id;
    float duration;       // seconds
    bool loop;
    std::vector<ParameterChannel> channels;
};
```

**Evaluation (sampling a channel at time t):**
1. Binary search keyframes to find the bracketing pair [k0, k1] where k0.time <= t < k1.time
2. Compute normalized t_local = (t - k0.time) / (k1.time - k0.time)  in [0,1]
3. Apply easing to get t_eased
4. Lerp: value = k0.value + (k1.value - k0.value) * t_eased

**Sparse keyframe storage is the correct choice** for this use case. Dense (sampled-per-frame)
storage wastes memory and makes editing impossible. Sparse keyframes of 3-10 per parameter
channel per clip are standard in all professional animation runtimes (Spine, Unity, Godot).

### Standard Easing Functions (HIGH confidence)

All five are worth implementing; they cover 95% of animation needs:

```cpp
// t is in [0.0, 1.0]
float ease_linear(float t)    { return t; }
float ease_in(float t)        { return t * t; }  // quadratic in
float ease_out(float t)       { return t * (2.0f - t); }  // quadratic out
float ease_in_out(float t)    { return t < 0.5f ? 2*t*t : -1+(4-2*t)*t; }

// Cubic bezier — the gold standard for designer control
// cp0=(0,0), cp3=(1,1), cp1 and cp2 are the control points
float cubic_bezier(float t, float x1, float y1, float x2, float y2) {
    // Solve for t_b where bezier_x(t_b) = t, then return bezier_y(t_b)
    // Use Newton-Raphson iteration (3-5 iterations sufficient for game use)
    // ... standard implementation ...
}
```

**Header-only easing libraries:** The common ones (easing.h by warrenm, or the cubic-bezier
implementations) are all trivially small. Given the "minimize deps" constraint, implement
these directly in ~50 lines rather than adding a library dependency. The math is
standardized and well-tested.

**CSS Easing equivalents for designer familiarity:**
- `ease-in` = cubic-bezier(0.42, 0, 1.0, 1.0)
- `ease-out` = cubic-bezier(0.0, 0.0, 0.58, 1.0)
- `ease-in-out` = cubic-bezier(0.42, 0, 0.58, 1.0)

---

## Animation Blending Model

### Track / Layer Model Comparison

**Unity Animator approach:**
- Animation layers with a weight (0.0-1.0) per layer
- Each layer has a blending mode: Override (replaces) or Additive (adds to below)
- Avatar masks restrict which parameters a layer affects
- State machine per layer drives which clip plays

**Spine track approach:**
- Numbered tracks (0 = base, 1 = override layer, etc.)
- Higher tracks override lower tracks for the same bone
- Alpha (weight) per track
- Mixed entry/exit with cross-fade timelines

**Simple priority queue:**
- Each animation write request has a priority integer
- Highest priority wins; lower priority only fills parameters the higher didn't set

**Recommendation for this project: Spine-inspired track model with weighted blending.**

This project's animation sources are well-defined and static (idle, keyboard, mouse, audio).
A track model maps naturally:

```
Track 0: idle     — always playing, low priority, full weight
Track 1: keyboard — plays when typing, overrides arm/hand parameters
Track 2: mouse    — plays on mouse move, overrides right_hand parameters
Track 3: audio    — always active, exclusively owns mouth_* parameters
```

**Implementation:**

```cpp
struct AnimationTrack {
    int priority;               // lower number = higher priority
    float weight;               // 0.0-1.0 blend weight
    AnimationClip* clip;
    float current_time;
    bool active;
    std::vector<std::string> owned_parameters;  // parameters this track can write
};

// Parameter ownership map — built from track definitions at load time
// Maps parameter_id -> list of tracks that can write it, sorted by priority
```

**Blending strategy per parameter:**

| Strategy | When to use | How |
|----------|-------------|-----|
| Override (highest priority wins) | Position, rotation, discrete states | Highest-priority active track's value wins |
| Weighted average | Opacity, smooth blends | Sum(weight_i * value_i) / sum(weight_i) for active tracks |
| Additive | Layered motion (e.g. breathing on top of arm pose) | Base value + delta from additive tracks |
| Last-write-wins | Simplest, lowest overhead | Each active track writes in priority order; last writer wins |

**Recommended blend strategy:** Use **override (priority-based)** as the default. Implement
weighted average for the idle track blending with active tracks (so idle smoothly dims when
the user starts typing). Additive blending is a future enhancement.

**Conflict resolution:** A parameter written by a higher-priority track is not written by
lower-priority tracks in the same frame. This avoids the "flickering" problem.

**The owned_parameters list is optional but recommended** — it lets the idle animation
write `head_tilt` without conflicting with a keyboard animation that also claims `head_tilt`,
by declaring explicit ownership in the character definition JSON.

---

## Threading Model

### OBS Thread Architecture (HIGH confidence)

OBS Studio runs several threads relevant to plugin authors:

| Thread | Purpose | Your plugin callbacks called here |
|--------|---------|-----------------------------------|
| **UI thread** | Qt main loop, properties, settings UI | `get_properties`, `update` (settings change), `defaults` |
| **Render thread** | GPU commands, frame composition | `video_render`, `video_tick` |
| **Audio thread** | Audio mixing and encoding | `filter_audio` (for audio filter sources) |
| **Output threads** | Encoding, muxing, streaming | Not called for sources |
| **Input thread** (yours) | Your global keyboard/mouse hook | Not an OBS thread — you create this |

**Critical facts:**
- `video_render` is called from the render thread — all `gs_*` calls must happen here
  (or inside `obs_enter_graphics()` / `obs_leave_graphics()`)
- `video_tick(source, seconds)` is called from the render thread immediately before
  `video_render` — this is where animation state should be advanced
- `update` (settings changed) is called from the UI thread — never call `gs_*` here
  without entering the graphics context
- Audio callbacks are called from the audio thread with real-time constraints

### video_tick: Thread and Frequency (HIGH confidence)

`video_tick` is called from the **render thread**, once per video frame, before
`video_render`. Its `seconds` parameter is the elapsed time since the last tick (delta time).

```c
// In obs_source_info:
.video_tick = my_source_video_tick,

static void my_source_video_tick(void* data, float seconds) {
    // This IS the render thread (or graphics-safe context)
    // Update animation state here
    // seconds = delta time (typically 1/60 or 1/30)
    auto* ctx = static_cast<AvatarSource*>(data);
    ctx->avatar_engine.tick(seconds);  // advance animation tracks
}
```

**Yes, `video_tick` is the correct place to run animation updates.** It is called at the
output frame rate (typically 60fps) from the render thread, giving you a natural game-loop
tick with correct delta time.

**Do not** drive animation from a separate timer thread and then try to synchronize with
the render thread — this adds unnecessary complexity and latency.

### Input Thread → Render Thread Communication (HIGH confidence)

Global keyboard/mouse hooks (Windows Raw Input or SetWindowsHookEx) run on whichever thread
processes the hook's message loop — you'll create a dedicated input thread for this. These
events must reach `video_tick` safely.

**Recommended approach: Single-producer single-consumer (SPSC) lock-free ring buffer.**

For this use case:
- Producer: input thread (writes events)
- Consumer: render thread in `video_tick` (reads events)
- SPSC allows lock-free implementation with just std::atomic

```cpp
// Minimal SPSC ring buffer for input events
struct InputEvent {
    enum class Type { KeyDown, KeyUp, MouseMove, MouseClick } type;
    int dx, dy;          // for MouseMove
    int button;          // for MouseClick
    // No key content — privacy boundary enforced in event type
};

template<typename T, size_t N>
class SPSCRingBuffer {
    std::array<T, N> buffer;
    std::atomic<size_t> head{0};  // written by producer
    std::atomic<size_t> tail{0};  // written by consumer
public:
    bool push(const T& item) {  // called from input thread
        size_t h = head.load(std::memory_order_relaxed);
        size_t next = (h + 1) % N;
        if (next == tail.load(std::memory_order_acquire)) return false; // full
        buffer[h] = item;
        head.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {         // called from render thread
        size_t t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false; // empty
        item = buffer[t];
        tail.store((t + 1) % N, std::memory_order_release);
        return true;
    }
};

// In video_tick:
InputEvent evt;
while (input_queue.pop(evt)) {
    avatar_engine.handle_input(evt);
}
```

**Ring buffer size:** 256 events is more than sufficient. Input arrives at human speeds
(~200 keypresses/second maximum bursts); video_tick drains at 60fps.

**moodycamel::ConcurrentQueue** is a well-regarded MPMC (multi-producer, multi-consumer)
lock-free queue and would work here, but SPSC is simpler, faster, and has no external
dependency. Use SPSC unless you need multiple producers.

**std::mutex alternative:** A simple mutex-protected deque would also work given the
very low event rate. The performance difference between mutex and lock-free at human-scale
input rates is unmeasurable. Choose SPSC for correctness clarity; mutex for simplicity.
Both are correct. This research recommends SPSC as the slightly cleaner design.

---

## GPU Resource Management

### obs_enter_graphics / obs_leave_graphics (HIGH confidence)

All libobs GPU resource operations (texture creation, texture destruction, effect loading,
render target creation) **must** happen either:
1. On the render thread (inside `video_render` or `video_tick`), OR
2. On any thread, wrapped in `obs_enter_graphics()` / `obs_leave_graphics()`

```cpp
// Correct: loading a texture from the source creation callback (UI thread)
static void* my_source_create(obs_data_t* settings, obs_source_t* source) {
    auto* ctx = new AvatarSource(source);

    // WRONG — this would crash or corrupt GPU state:
    // ctx->texture = gs_texture_create(...);  // UI thread, no graphics context

    // CORRECT — either defer to video_tick, or:
    obs_enter_graphics();
    ctx->load_textures(settings);  // gs_texture_create calls here are safe
    obs_leave_graphics();

    return ctx;
}
```

```cpp
// Correct: destroying textures in source_destroy (called from UI thread)
static void my_source_destroy(void* data) {
    auto* ctx = static_cast<AvatarSource*>(data);

    obs_enter_graphics();
    // gs_texture_destroy, gs_texrender_destroy, etc.
    ctx->release_gpu_resources();
    obs_leave_graphics();

    delete ctx;
}
```

**Texture loading strategy for this plugin:**

1. `source_create` is called on the UI thread — load the character JSON (CPU only),
   identify texture paths
2. Wrap texture loading in `obs_enter_graphics()` / `obs_leave_graphics()` immediately
   within `source_create` — this acquires the graphics lock and makes GPU calls safe
3. Alternatively, set a `needs_load` flag and perform GPU uploads on the first
   `video_tick` call (render thread, no locking required)

**Recommended:** Load in `source_create` with `obs_enter_graphics()` guard. It is simpler
and avoids a "first frame" with no textures. The load happens once and is not on the hot path.

**Effect files (.effect):** Load with `gs_effect_create_from_file()` or
`gs_effect_create()` — also requires graphics context. Load in `source_create`.

---

## Animation Parameter System

### Parameter Map Design (HIGH confidence — standard approach)

The parameter system is the runtime contract between the animation tracks (writers) and the
renderer (reader). Minimal, fast, and named:

```cpp
// Parameter types needed for avatar animation
enum class ParamType { Float, Vec2, Color };

union ParamValue {
    float    f;
    float    v2[2];
    uint32_t color;  // RGBA packed
};

struct AnimParam {
    ParamType type;
    ParamValue value;
    ParamValue default_value;
};

// The parameter store — written by animation, read by renderer
class ParameterStore {
    // Use unordered_map with string_view-compatible keys for ~O(1) lookup
    std::unordered_map<std::string, AnimParam> params;
public:
    void set_float(const std::string& id, float v);
    float get_float(const std::string& id) const;
    void reset_to_defaults();  // called at start of each tick before animation writes
};
```

**Why unordered_map is acceptable here:**
- 10-20 parameters per avatar — map size is tiny
- Lookup is amortized O(1) with no collision cost at this size
- std::unordered_map with std::string keys: hashing cost at 60fps for 20 lookups ≈ ~1 microsecond total. Negligible.

**Performance-critical alternative (if profiling shows hotspot):**
Pre-index parameters as integers at load time. Map `std::string` → `int index` once,
then the hot path uses a `std::vector<AnimParam>` by index. Not needed for MVP.

### Parameter Ownership and Blending (MEDIUM-HIGH confidence)

**Recommended pattern: Reset-then-write with priority drain.**

```cpp
void AvatarEngine::tick(float dt) {
    // 1. Reset all parameters to defaults
    params.reset_to_defaults();

    // 2. Write parameters from lowest to highest priority tracks
    //    (higher priority overwrites lower for the same parameter)
    for (auto& track : tracks_sorted_low_to_high_priority) {
        if (track.active) {
            track.evaluate(dt, params);  // track writes its parameters
        }
    }
    // Result: highest-priority track's value wins for each parameter

    // 3. Blend idle track weight
    // For idle, reduce its weight when higher-priority tracks are active:
    float idle_weight = compute_idle_blend_weight();
    for (auto& [id, blend_param] : idle_channel_params) {
        float current = params.get_float(id);
        float idle_val = idle_channel_params[id];
        params.set_float(id, current * (1.0f - idle_weight) + idle_val * idle_weight);
    }
}
```

**The "write-in-priority-order" approach** is simpler than maintaining a separate
override/weight system and produces correct behavior for all defined use cases:
- Idle runs continuously in background
- Keyboard typing animation fully overrides arm parameters
- Mouse movement overrides right hand
- Audio owns mouth parameters exclusively

**Additive blending for breathing:** The idle animation's breathing (slight body movement)
can be implemented as additive by having the idle track *add* a delta to the current value
rather than set it. This allows breathing to layer on top of whatever pose the arms/body
are in. Implement as a flag per channel in the animation clip definition.

---

## Recommended Architecture Decisions

### Decision 1: Two-Layer Architecture (CONFIRMED)

The project's stated "Avatar Engine + OBS Integration" split is architecturally correct.

```
┌─────────────────────────────────────────────────────┐
│  OBS Integration Layer (obs_avatar_source.cpp)       │
│  - obs_source_info registration                      │
│  - video_render, video_tick, source_create/destroy   │
│  - Properties panel, settings serialization          │
│  - Audio tap setup, obs_enter_graphics guards        │
│  - Input queue (SPSC ring buffer)                    │
└────────────────────┬────────────────────────────────┘
                     │  calls into
┌────────────────────▼────────────────────────────────┐
│  Avatar Engine (avatar_engine.cpp / avatar_engine.h) │
│  - ParameterStore                                    │
│  - AnimationTrack[] — idle, keyboard, mouse, audio   │
│  - AnimationClip evaluator (keyframe interpolation)  │
│  - CharacterDefinition loader (JSON + stb_image)     │
│  - LayerRenderer (uses gs_* via passed context)      │
│  No direct #include of obs headers                   │
└─────────────────────────────────────────────────────┘
```

The Avatar Engine should be compilable without OBS headers by keeping graphics calls
abstracted behind a `IRenderContext` interface or by accepting the gs_* functions as
callbacks. This is the stated project goal and architecturally sound.

### Decision 2: Rendering Pipeline

```
video_tick (render thread):
  1. drain input queue → input events → AvatarEngine::handle_input()
  2. AvatarEngine::tick(dt) → advance track times, evaluate, write ParameterStore

video_render (render thread, immediately after tick):
  3. gs_texrender_reset() + gs_texrender_begin() → enter off-screen FBO
  4. Clear FBO (transparent black, premultiplied)
  5. For each layer (back to front):
       gs_matrix_push()
       apply transform from ParameterStore + layer defaults
       bind texture + effect
       gs_draw_sprite()
       gs_matrix_pop()
  6. gs_texrender_end()
  7. obs_source_draw(gs_texrender_get_texture(...), ...) → output to OBS
```

### Decision 3: Character File Layout

```
character_packs/
  DefaultCharacter/
    character.json        ← master definition
    body.png
    head.png
    right_arm.png
    left_arm.png
    right_hand.png
    left_hand.png
    mouth_closed.png      ← OR: mouth atlas with UV coords in JSON
    animations/
      idle.json
      typing.json
      mouse_move.json
```

Animation clips are separate JSON files referenced from `character.json`. This allows
loading only needed clips and keeps file sizes manageable.

### Decision 4: Effect/Shader Strategy

Use OBS's built-in `OBS_EFFECT_DEFAULT` with premultiplied alpha textures (converted
at load time). Write a single custom `.effect` file only if you need:
- Per-layer color tinting (multiply texture by a color parameter)
- Opacity blending (multiply alpha channel by a float)
- Future: outlines, distortion effects

A custom effect for opacity + tinting is ~30 lines of HLSL-like code and covers all
foreseeable needs.

### Decision 5: Thread-Safety Summary

| Resource | Owner Thread | Access Pattern |
|----------|-------------|----------------|
| GPU textures, effects, texrender | Render thread | Created in source_create (with obs_enter_graphics), used in video_render |
| ParameterStore | Render thread | Written and read only in tick/render |
| AnimationTrack state | Render thread | Evaluated in video_tick |
| SPSC input queue (write side) | Input thread | Lock-free push only |
| SPSC input queue (read side) | Render thread | Lock-free pop in video_tick |
| Character JSON, file paths | UI thread | Read in source_create, then immutable |
| OBS settings (obs_data_t) | UI thread | Only in update/defaults/get_properties |

---

## Sources

**Note on source availability:** WebSearch and WebFetch were denied during this research
session. The following sources represent the authoritative references that should be
consulted to verify specific API signatures before implementation. All architectural claims
are based on training knowledge through mid-2025 and cross-checked for internal consistency.

- **OBS Studio source code** — https://github.com/obsproject/obs-studio
  - `libobs/graphics/graphics.h` — gs_matrix_*, gs_draw_sprite, gs_texrender_*, gs_effect_*
  - `libobs/graphics/graphics.c` — implementation of coordinate system and blend state setup
  - `libobs/obs-source.h` — obs_source_info struct, video_tick, video_render signatures
  - `libobs/obs.h` — obs_enter_graphics, obs_leave_graphics, obs_get_base_effect
  - `plugins/image-source/image-source.c` — reference for texture loading pattern
  - `plugins/obs-transitions/` — reference for gs_texrender_t usage pattern

- **OBS Plugin Development Wiki** — https://obsproject.com/wiki/Plugin-API-Overview
  (confidence: HIGH for API overview, verify specific function signatures against source)

- **nlohmann/json** — https://github.com/nlohmann/json (single-header JSON library)
  (confidence: HIGH — well-established, widely used in OBS ecosystem)

- **stb_image** — https://github.com/nothings/stb/blob/master/stb_image.h
  (confidence: HIGH — de facto standard for single-header image loading in C++)

- **Spine Runtime documentation** — http://en.esotericsoftware.com/spine-runtimes
  (confidence: MEDIUM — training knowledge, sparse keyframe + track model reference)

- **moodycamel::ConcurrentQueue** — https://github.com/cameron314/concurrentqueue
  (confidence: HIGH — widely benchmarked lock-free MPMC queue)

- **C++20 std::atomic memory ordering** — cppreference.com/w/cpp/atomic/memory_order
  (confidence: HIGH — standard reference)

- **StreamFX source code** — https://github.com/Xaymar/obs-StreamFX
  (confidence: MEDIUM — reference for advanced libobs rendering patterns)

- **obs-move-transition** — https://github.com/exeldro/obs-move-transition
  (confidence: MEDIUM — reference for per-element transform patterns in OBS plugins)

---

## Confidence Assessment

| Area | Confidence | Basis |
|------|------------|-------|
| Coordinate system (top-left, Y-down) | MEDIUM-HIGH | Consistent with standard OBS behavior and multiple plugin patterns |
| gs_matrix_push/pop as standard pattern | HIGH | Used in all OBS rendering plugins; well-documented in community |
| Premultiplied alpha internally in OBS | HIGH | Documented in OBS source and multiple community posts |
| gs_texrender_t for off-screen FBO | HIGH | Documented API, used in StreamFX and other plugins |
| video_tick on render thread | HIGH | Explicitly documented in OBS wiki and source |
| obs_enter_graphics requirement for GPU ops | HIGH | Core libobs threading requirement, well-documented |
| SPSC ring buffer for input→render comms | HIGH | Standard lock-free pattern, no OBS-specific uncertainty |
| JSON + nlohmann/json recommendation | HIGH | Based on clear tradeoff analysis |
| stb_image for PNG loading | HIGH | De facto standard for this use case in C++ |
| Sparse keyframe model | HIGH | Industry standard for animation runtimes |
| Track-based blending model | MEDIUM-HIGH | Adapted from Spine/Unity patterns; implementation details may vary |
| Exact OBS API function signatures | MEDIUM | Verify against current obs-studio source before coding |
