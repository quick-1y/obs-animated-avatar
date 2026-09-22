# Features Research: OBS APIs for Animated Avatar Plugin

**Project:** OBS Animated Avatar Plugin
**Researched:** 2026-09-22
**Confidence:** MEDIUM — based on training knowledge of OBS 30.x libobs headers (obs-source.h, obs-module.h, graphics/graphics.h, obs-audio-controls.h). No live source fetch was possible in this session. Cross-reference against current `obsproject/obs-studio` main branch before implementation.

---

## Source Registration & Module Entry Points

### `OBS_DECLARE_MODULE()` Macro

Every OBS plugin must include this macro at file scope (typically in the main `.cpp`):

```c
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("my-plugin", "en-US")
```

`OBS_DECLARE_MODULE()` expands to the required export table (`obs_module_ver`, `obs_module_name`, `obs_module_description`, etc.) that OBS loads via `dlopen`/`LoadLibrary`. Without it, OBS will refuse to load the module.

`OBS_MODULE_USE_DEFAULT_LOCALE(module_name, default_locale)` sets up the locale lookup so `obs_module_text("Key")` resolves correctly.

### Module Entry Points

```c
// Required — called when OBS loads the plugin. Register all sources here.
bool obs_module_load(void);

// Optional but recommended — called when OBS unloads the plugin.
void obs_module_unload(void);
```

`obs_module_load` MUST return `true` on success. Returning `false` aborts load. This is where `obs_register_source()` calls go.

### `obs_register_source()`

```c
void obs_register_source(const struct obs_source_info *info);
```

Called inside `obs_module_load`. Takes a pointer to a static (or file-scope) `obs_source_info` struct. OBS copies the struct, so stack allocation is fine as long as the call completes before the function returns. Best practice: declare as a static global.

`obs_module_text(lookup_string)` returns a localized UTF-8 string from the loaded locale. It is safe to call from property callbacks. Localization is optional at runtime (falls back to the key if the locale file is missing), but required for OBS plugin submission to the official plugin browser.

---

## `obs_source_info` Struct — All Fields

Source: `libobs/obs-source.h`. Confidence: HIGH for OBS 29–30.x.

```c
struct obs_source_info {
    /* --- REQUIRED --- */
    const char *id;           // Unique plugin-scoped ID, e.g. "animated_avatar_source"
    enum obs_source_type type; // OBS_SOURCE_TYPE_INPUT, _FILTER, _TRANSITION, _SCENE
    uint32_t output_flags;    // Bitfield of OBS_SOURCE_* flags (see below)

    // Display name shown in the UI
    const char *(*get_name)(void *type_data);

    // Allocate and return the source's private data
    void *(*create)(obs_data_t *settings, obs_source_t *source);

    // Free private data
    void (*destroy)(void *data);

    /* --- REQUIRED for video sources --- */
    uint32_t (*get_width)(void *data);
    uint32_t (*get_height)(void *data);
    void (*video_render)(void *data, gs_effect_t *effect);

    /* --- STRONGLY RECOMMENDED --- */
    void (*get_defaults)(obs_data_t *settings);
    obs_properties_t *(*get_properties)(void *data);
    void (*update)(void *data, obs_data_t *settings);

    /* --- OPTIONAL --- */
    void (*video_tick)(void *data, float seconds);
    void (*show)(void *data);
    void (*hide)(void *data);
    void (*activate)(void *data);
    void (*deactivate)(void *data);
    void (*save)(void *data, obs_data_t *settings);
    void (*load)(void *data, obs_data_t *settings);
    void (*mouse_click)(void *data, const struct obs_mouse_event *event,
                        int32_t type, bool mouse_up, uint32_t click_count);
    void (*mouse_move)(void *data, const struct obs_mouse_event *event,
                       bool mouse_leave);
    void (*mouse_wheel)(void *data, const struct obs_mouse_event *event,
                        int x_delta, int y_delta);
    void (*focus)(void *data, bool focus);
    void (*key_click)(void *data, const struct obs_key_event *event, bool key_up);
    void (*filter_video)(void *data, struct obs_source_frame *frame);
    void (*filter_audio)(void *data, struct obs_audio_data *audio);
    void (*enum_active_sources)(void *data, obs_source_enum_proc_t enum_callback,
                                void *param);
    void (*enum_all_sources)(void *data, obs_source_enum_proc_t enum_callback,
                             void *param);
    void (*transition_start)(void *data);
    void (*transition_stop)(void *data);
    void (*get_transitions)(void *data, obs_source_enum_proc_t enum_callback,
                            void *param);
    uint32_t (*get_defaults2)(void *type_data, obs_data_t *settings); // OBS 28+
    void *type_data;         // Passed to get_name / get_defaults2; usually NULL
    void (*free_type_data)(void *type_data); // Called if type_data != NULL at unload
    bool (*audio_render)(void *data, uint64_t *ts_out,
                         struct obs_source_audio_mix *audio_output,
                         uint32_t mixers, size_t channels, size_t sample_rate);
    void (*enum_active_trees)(void *data, obs_source_enum_proc_t enum_callback,
                              void *param);
    bool (*audio_mix)(void *data, uint64_t *ts_out,
                      struct audio_output_data *audio_output,
                      size_t channels, size_t sample_rate);
    // OBS 29+: icon type for the source list
    enum obs_icon_type icon_type;
    // OBS 30+: media controls callbacks (for media sources)
    void (*media_play_pause)(void *data, bool pause);
    void (*media_restart)(void *data);
    void (*media_stop)(void *data);
    void (*media_next)(void *data);
    void (*media_previous)(void *data);
    int64_t (*media_get_duration)(void *data);
    int64_t (*media_get_time)(void *data);
    void (*media_set_time)(void *data, int64_t miliseconds);
    enum obs_media_state (*media_get_state)(void *data);
    // OBS 30+: version integer for the plugin
    uint32_t version;
    // OBS 30+: unversioned_id for legacy compat
    const char *unversioned_id;
};
```

**For an animated avatar source, mandatory fields are:**
`id`, `type` (`OBS_SOURCE_TYPE_INPUT`), `output_flags`, `get_name`, `create`, `destroy`, `get_width`, `get_height`, `video_render`.

**Strongly recommended:** `get_defaults`, `get_properties`, `update`, `video_tick`.

**Leave all others as NULL** — OBS checks for NULL before calling.

### Source Type Flags (`output_flags`)

```c
// Produces video frames — required for rendering sources
#define OBS_SOURCE_VIDEO               (1 << 0)

// Source does its own OpenGL/D3D drawing (no automatic texture output)
// Use this for sources that call obs_source_draw() or gs_draw() directly
#define OBS_SOURCE_CUSTOM_DRAW         (1 << 1)

// Source produces audio
#define OBS_SOURCE_AUDIO               (1 << 2)

// Deprecated / internal use — do NOT set unless you know why
#define OBS_SOURCE_ASYNC               (1 << 3)
#define OBS_SOURCE_ASYNC_VIDEO         (OBS_SOURCE_ASYNC | OBS_SOURCE_VIDEO)

// Source should not be composited by OBS — handles its own blending
// NOT needed for avatar — let OBS composite normally
#define OBS_SOURCE_DO_NOT_DUPLICATE    (1 << 4)

// For filter sources — interacts with the filtered source
#define OBS_SOURCE_INTERACTION         (1 << 5)

// Source does not trigger preview rendering (optimization)
#define OBS_SOURCE_CAP_OBSOLETE        (1 << 6)

// OBS 28+: source supports monitor-only output
#define OBS_SOURCE_MONITOR_BY_DEFAULT  (1 << 7)

// OBS 28+: disable automatic volume management
#define OBS_SOURCE_CAP_DISABLED        (1 << 8)
```

**For the avatar plugin:** `output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW`

Setting `OBS_SOURCE_CUSTOM_DRAW` tells OBS that the source drives its own draw calls via `obs_source_draw()` rather than pushing frames via `obs_source_output_video()`. This is correct for a plugin that composites layers in `video_render`.

---

## Source Lifecycle Callbacks (with Exact Signatures)

### `create`

```c
void *create(obs_data_t *settings, obs_source_t *source);
```

Called when OBS instantiates the source (user adds it, or scene loads). Allocate and return private data. `settings` contains saved or default values. `source` is the OBS handle — store it if needed for `obs_source_*` calls later. Return `NULL` to fail creation (OBS will call `destroy` but not other callbacks).

### `destroy`

```c
void destroy(void *data);
```

Called when source is removed. Free all resources: GPU textures (`gs_texture_destroy`), heap allocations, thread handles. OBS does NOT call `update`, `video_render`, or `video_tick` after `destroy` begins.

**Important:** GPU resource destruction must happen on the graphics thread. If you call `gs_texture_destroy` from `destroy`, wrap with `obs_enter_graphics()` / `obs_leave_graphics()`.

### `update`

```c
void update(void *data, obs_data_t *settings);
```

Called when the user changes a property (applies settings) and on load after `create`. Read new values from `settings` and update internal state. May need to rebuild GPU resources if dimensions or textures change.

### `get_width` / `get_height`

```c
uint32_t get_width(void *data);
uint32_t get_height(void *data);
```

Called on the render thread each frame. Must return the rendered output dimensions in pixels. For a fixed-size avatar, return a constant. For a dynamic-size avatar, return the current dimensions.

### `video_render`

```c
void video_render(void *data, gs_effect_t *effect);
```

Called on the render/graphics thread each video frame when the source is visible. `effect` is OBS's default effect (usually ignored when `OBS_SOURCE_CUSTOM_DRAW` is set — you select your own effect). This is where all `gs_*` draw calls go.

**Threading:** This IS the graphics thread. Do NOT call non-thread-safe operations here. The graphics context is active — all `gs_*` calls are valid.

### `video_tick`

```c
void video_tick(void *data, float seconds);
```

Called every video frame interval **before** `video_render`, on the **video output thread** (NOT the render thread). `seconds` is the delta time since the last tick (e.g. ~0.01667s at 60 fps). This is the correct place to advance animation state, process audio amplitude, update transforms. Do NOT call `gs_*` graphics API here — wrong thread.

**Key distinction:** `video_tick` = update game state. `video_render` = draw it.

### `show` / `hide`

```c
void show(void *data);
void hide(void *data);
```

`show` is called when the source becomes visible in the current preview or output (e.g., source is in an active scene). `hide` is called when it is no longer visible. These fire even if the source is active. Use to start/stop expensive processes (e.g., skip audio analysis while hidden).

### `activate` / `deactivate`

```c
void activate(void *data);
void deactivate(void *data);
```

`activate` fires when the source enters an output that is active (live/recording). `deactivate` fires when it leaves all active outputs. More restrictive than show/hide. Useful for starting/stopping input capture threads only when actually streaming.

### `get_defaults`

```c
void get_defaults(obs_data_t *settings);
```

Called once to populate default values. Use `obs_data_set_default_*()` functions. Called before `create`, so defaults are available to `create` via the settings object.

### `get_properties`

```c
obs_properties_t *get_properties(void *data);
```

Called when the user opens the source properties dialog. Build and return a property list. OBS takes ownership — do not free the returned `obs_properties_t`. `data` may be NULL if called before creation.

---

## Properties & Settings System

### Building Properties

```c
obs_properties_t *props = obs_properties_create();

// Text (string) property
obs_properties_add_text(props, "setting_key", "Display Label",
                        OBS_TEXT_DEFAULT);
// OBS_TEXT_DEFAULT = single-line, OBS_TEXT_PASSWORD = hidden, OBS_TEXT_MULTILINE

// Boolean (checkbox)
obs_properties_add_bool(props, "enable_lipsync", "Enable Lip Sync");

// Integer slider
obs_properties_add_int_slider(props, "sensitivity", "Mic Sensitivity",
                              0, 100, 1);
// args: key, label, min, max, step

// Float slider
obs_properties_add_float_slider(props, "smoothing", "Smoothing",
                                0.0, 1.0, 0.01);

// List (dropdown)
obs_property_t *p = obs_properties_add_list(props, "audio_source",
                                             "Audio Source",
                                             OBS_COMBO_TYPE_LIST,
                                             OBS_COMBO_FORMAT_STRING);
obs_property_list_add_string(p, "(None)", "");
// Populate dynamically by enumerating sources (see Audio API section)

// Path (file picker)
obs_properties_add_path(props, "character_file", "Character File",
                        OBS_PATH_FILE, "JSON Files (*.json)", NULL);

// Button
obs_properties_add_button(props, "reload_btn", "Reload Character",
                          reload_button_clicked);

// Group (collapsible section, OBS 28+)
obs_properties_t *group = obs_properties_create();
obs_properties_add_bool(group, "debug_mode", "Debug Mode");
obs_properties_add_group(props, "advanced_group", "Advanced",
                         OBS_GROUP_NORMAL, group);

return props;
```

### Property Callbacks (for reactive UI)

```c
// Callback to show/hide a property based on another property's value
static bool lipsync_toggle_callback(obs_properties_t *props,
                                    obs_property_t *p,
                                    obs_data_t *settings) {
    bool enabled = obs_data_get_bool(settings, "enable_lipsync");
    obs_property_set_visible(obs_properties_get(props, "sensitivity"),
                             enabled);
    return true; // return true to refresh the UI
}
// Register:
obs_property_set_modified_callback(
    obs_properties_get(props, "enable_lipsync"),
    lipsync_toggle_callback);
```

### `obs_data_t` Serialization

OBS serializes source settings to JSON automatically. Access via:

```c
// Reading (in create/update):
const char *char_path = obs_data_get_string(settings, "character_file");
bool lipsync_on      = obs_data_get_bool(settings, "enable_lipsync");
int sensitivity      = (int)obs_data_get_int(settings, "sensitivity");
double smoothing     = obs_data_get_double(settings, "smoothing");

// Writing defaults (in get_defaults):
obs_data_set_default_string(settings, "character_file", "");
obs_data_set_default_bool(settings, "enable_lipsync", true);
obs_data_set_default_int(settings, "sensitivity", 50);
obs_data_set_default_double(settings, "smoothing", 0.3);
```

OBS handles serialization to/from scene JSON transparently — no manual save/load needed unless you use `save`/`load` callbacks for non-settings state.

---

## Graphics / Rendering API

### Core Types

```c
gs_texture_t   // Opaque handle to a GPU texture (2D, volume, cube)
gs_effect_t    // Compiled shader effect (vertex + pixel shader pair)
gs_technique_t // Named technique within an effect
gs_eparam_t    // Effect parameter handle for setting uniforms
```

### Built-In Effects

```c
// Get a built-in effect by enum:
gs_effect_t *obs_get_base_effect(enum obs_base_effect effect);

// Available effects:
// OBS_EFFECT_DEFAULT        — default sRGB-aware effect, premult alpha, 1:1 texture draw
// OBS_EFFECT_DEFAULT_RECT   — same but uses texture2d_rect sampler (for rect textures)
// OBS_EFFECT_OPAQUE         — forces alpha to 1.0 (removes transparency)
// OBS_EFFECT_SOLID          — solid color, no texture
// OBS_EFFECT_BICUBIC        — bicubic upscale filter
// OBS_EFFECT_LANCZOS        — lanczos upscale filter
// OBS_EFFECT_BILINEAR       — bilinear downscale filter
// OBS_EFFECT_REPEAT         — repeating/tiling texture
// OBS_EFFECT_AREA           — area-based downscale
```

For the avatar, `OBS_EFFECT_DEFAULT` is the standard choice — it handles premultiplied alpha correctly for layered sprites.

### Creating Textures

```c
// From raw RGBA pixel data:
gs_texture_t *gs_texture_create(uint32_t width, uint32_t height,
                                 enum gs_color_format color_format,
                                 uint32_t levels,
                                 const uint8_t **data,
                                 uint32_t flags);
// color_format: GS_RGBA for 8-bit RGBA, GS_RGBA16F for float16, etc.
// levels: 1 for no mipmaps
// data: array of pointers to pixel rows, or NULL to allocate empty
// flags: 0 for static, GS_DYNAMIC for frequently updated textures
// MUST be called inside obs_enter_graphics() / obs_leave_graphics()

// Example:
obs_enter_graphics();
const uint8_t *pixel_ptr = pixel_data; // row-major RGBA bytes
gs_texture_t *tex = gs_texture_create(width, height, GS_RGBA, 1,
                                      &pixel_ptr, 0);
obs_leave_graphics();

// Update an existing dynamic texture:
gs_texture_set_image(tex, new_pixel_data, width * 4, false);
// MUST also be inside graphics lock.

// Destroy:
obs_enter_graphics();
gs_texture_destroy(tex);
obs_leave_graphics();
```

Loading from file: libobs does not provide a built-in PNG loader in the graphics API. Use a third-party PNG library (libpng, stb_image) to decode the file to RGBA bytes, then call `gs_texture_create`. The `obs_find_module_file()` function helps locate files relative to the plugin's data directory.

### Drawing a Texture (Sprite)

The standard pattern for rendering a texture in `video_render`:

```c
void video_render(void *data, gs_effect_t *effect) {
    struct my_source *s = data;
    if (!s->texture) return;

    // Select the default effect for textured sprites with alpha
    effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

    // Set the texture parameter
    gs_eparam_t *param = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture(param, s->texture);

    // Draw using the "Draw" technique (or "DrawAlpha" for keyed alpha)
    while (gs_effect_loop(effect, "Draw")) {
        gs_draw_sprite(s->texture, 0, s->width, s->height);
    }
}
```

`gs_draw_sprite(texture, flip_flags, cx, cy)` draws a full-screen quad mapped to the texture at the current matrix transform. `flip_flags`: 0 = normal, `GS_FLIP_U` = flip horizontal, `GS_FLIP_V` = flip vertical.

Alternatively, for sub-regions or custom geometry, use `obs_source_draw()`:

```c
// Draws a texture onto the current render target respecting source size:
void obs_source_draw(gs_texture_t *texture, int x, int y,
                     uint32_t cx, uint32_t cy, bool flip);
```

### Matrix Transforms for Layers

```c
// Push a new transform matrix onto the stack
gs_matrix_push();

// Apply transforms (cumulative, applied to top of stack)
gs_matrix_translate3f(float x, float y, float z);
gs_matrix_scale3f(float x, float y, float z);
gs_matrix_rotaa4f(float x, float y, float z, float rot); // axis-angle rotation in radians

// Draw at transformed position
gs_draw_sprite(texture, 0, width, height);

// Restore previous matrix
gs_matrix_pop();
```

For multi-layer avatar rendering, each layer gets push/transform/draw/pop. Order matters: render back-to-front for correct alpha compositing.

### Blending Modes

```c
// Enable alpha blending (required for transparent sprites):
gs_blend_state_push();
gs_reset_blend_state();
gs_enable_blending(true);
gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA); // premult alpha

// ... draw sprites ...

gs_blend_state_pop();
```

OBS uses premultiplied alpha throughout its render pipeline. PNG images with straight alpha must be converted to premultiplied before upload, or the default effect's "Draw" technique handles it automatically.

### Render Thread Rules

All `gs_*` calls MUST be on the render/graphics thread. OBS provides two mechanisms:

```c
// 1. Inside video_render callback — already on render thread, call freely

// 2. Outside render callbacks — acquire the graphics lock:
obs_enter_graphics();
// ... gs_* calls here ...
obs_leave_graphics();
// Use for: texture creation in create/update, texture destruction in destroy
```

Never call `gs_*` from `video_tick`, `create` (without lock), `update` (without lock), or audio callbacks.

---

## Audio API

### Tapping Audio from Another OBS Source

The primary mechanism for accessing audio from a selected source is `obs_source_add_audio_capture_callback()`:

```c
typedef void (*obs_source_audio_capture_t)(void *param,
                                           obs_source_t *source,
                                           const struct audio_data *audio,
                                           bool muted);

void obs_source_add_audio_capture_callback(
    obs_source_t *source,
    obs_source_audio_capture_t callback,
    void *param);

void obs_source_remove_audio_capture_callback(
    obs_source_t *source,
    obs_source_audio_capture_t callback,
    void *param);
```

**Status:** This API exists in OBS 29–30.x. Confidence: HIGH.

The `audio_data` struct:

```c
struct audio_data {
    uint8_t *data[MAX_AV_PLANES];  // Up to 8 planes; for stereo non-interleaved: data[0]=L, data[1]=R
    uint32_t frames;               // Number of audio frames (samples per channel)
    uint64_t timestamp;            // ns timestamp (obs_get_video_frame_time() base)
};
```

OBS internal audio format: **32-bit float, planar** (non-interleaved). Each channel is a separate float array in `data[i]`. Sample rate is whatever OBS is configured to (44100 or 48000 Hz, typically 44100 or 48000). The number of channels depends on the source's audio output format (mono, stereo, 5.1, etc.). The `audio_output_get_channels()` and `audio_output_get_sample_rate()` functions return the mix's actual values.

```c
// Get OBS global audio output info:
audio_t *obs_get_audio(void);
uint32_t audio_output_get_sample_rate(const audio_t *audio);
size_t   audio_output_get_channels(const audio_t *audio);
```

### Enumerating Audio Sources for a Properties Dropdown

```c
// In get_properties, populate a source list dropdown:
obs_property_t *p = obs_properties_add_list(props, "audio_source_name",
                                            "Audio Source",
                                            OBS_COMBO_TYPE_LIST,
                                            OBS_COMBO_FORMAT_STRING);
obs_property_list_add_string(p, "(None)", "");

// Enumerate all audio capture sources:
obs_enum_sources([](void *param, obs_source_t *src) -> bool {
    obs_property_t *p = (obs_property_t *)param;
    uint32_t flags = obs_source_get_output_flags(src);
    if (flags & OBS_SOURCE_AUDIO) {
        const char *name = obs_source_get_name(src);
        obs_property_list_add_string(p, name, name);
    }
    return true; // continue enumeration
}, p);
```

### Getting a Source Handle by Name (in `update`)

```c
// In update callback, when the user selects a new audio source:
const char *src_name = obs_data_get_string(settings, "audio_source_name");
obs_source_t *audio_src = obs_get_source_by_name(src_name);
if (audio_src) {
    obs_source_add_audio_capture_callback(audio_src,
                                          my_audio_callback, my_data);
    obs_source_release(audio_src); // release ref (callback keeps internal ref)
}
```

**Important:** `obs_get_source_by_name` increments the reference count. Call `obs_source_release` after registering the callback. The callback will continue to fire until removed.

When the audio source selection changes, remove the old callback before adding the new one:

```c
if (s->audio_source) {
    obs_source_remove_audio_capture_callback(s->audio_source,
                                             my_audio_callback, s);
    obs_source_release(s->audio_source);
    s->audio_source = NULL;
}
```

### Audio Callback Threading

The `obs_source_audio_capture_t` callback fires on the **audio thread**, not the video thread and not the render thread. Access shared data (e.g., amplitude value read by `video_tick`) must be synchronized. Use atomic operations or a lightweight mutex:

```c
// Producer side (audio thread):
atomic_store_explicit(&s->amplitude, rms_value, memory_order_relaxed);

// Consumer side (video_tick — video thread):
float amp = atomic_load_explicit(&s->amplitude, memory_order_relaxed);
```

For lip-sync, this is sufficient — exact frame alignment is not critical, and relaxed ordering is safe because there is no dependent write sequencing required.

### Audio Timing Relative to Video Frames

The `audio_data.timestamp` is in nanoseconds on the OBS monotonic clock (`os_gettime_ns()`). OBS's audio pipeline runs with approximately 1–2 video frames of latency relative to the display pipeline. For amplitude-based lip sync (not FFT/phoneme matching), this latency is imperceptible — the mouth animation reacts within 1–2 frames of the audio arriving, which is well within perceptual sync thresholds (typically ~100ms).

---

## Threading Rules Summary

| Thread | When Active | Allowed API Calls | Forbidden |
|--------|-------------|-------------------|-----------|
| **Render/Graphics** | Inside `video_render` | All `gs_*`, `obs_source_draw`, matrix ops | `obs_*` source mutate calls |
| **Video Output** | Inside `video_tick` | Read source state, update animation, atomic writes | All `gs_*` graphics calls |
| **Audio** | Inside audio capture callback | Atomic writes, audio math | `gs_*`, `obs_source_*` mutate |
| **Main/UI** | `create`, `update`, `get_properties` | `obs_*` API, `gs_*` inside `obs_enter_graphics()` lock | `gs_*` without lock |
| **Any other thread** | Plugin's own worker threads | Atomic reads/writes, `os_*` OS API | `gs_*` without lock, `obs_source_*` without care |

**Golden rule:** If you're not inside `video_render`, wrap all `gs_*` calls in `obs_enter_graphics()` / `obs_leave_graphics()`.

**Practical pattern for texture updates from outside render:**

```c
// E.g., in update() when character changes:
obs_enter_graphics();
if (s->old_texture) gs_texture_destroy(s->old_texture);
s->texture = gs_texture_create(w, h, GS_RGBA, 1, &pixels, 0);
obs_leave_graphics();
```

---

## Key API Gaps or Uncertainties

### Confirmed Gaps (require verification against current source)

1. **`obs_source_info` field additions in OBS 30.x.** The struct has grown across versions. Fields like `version`, `unversioned_id`, `icon_type`, and media control callbacks were added in OBS 28–30. Zero-initialize the struct (`= {}`) to ensure new fields default to NULL/0 without compilation warnings across versions.

2. **`gs_texture_create` signature.** The `data` parameter is `const uint8_t **` (pointer-to-pointer) in some versions and `const uint8_t *const *` in others. Check the current `graphics/graphics.h` for the exact declaration before using.

3. **`obs_source_add_audio_capture_callback` thread safety.** Safe to add/remove from the main thread during `update`. Behavior if called from the audio thread itself is undocumented — avoid it.

4. **`OBS_SOURCE_CUSTOM_DRAW` vs letting OBS handle composition.** If `OBS_SOURCE_CUSTOM_DRAW` is NOT set, OBS expects the source to push frames via `obs_source_output_video()`. With it set, `video_render` is the only output path. Confirm this interpretation against current source — it has been stable since OBS 21 but behavior may have edge cases with OBS Studio's new compositing pipeline (obs-scene-item vs obs-source).

5. **`obs_enum_sources` vs `obs_enum_all_sources`.** `obs_enum_sources` enumerates input sources in the current scene. `obs_enum_all_sources` enumerates everything. For a microphone dropdown, `obs_enum_sources` may miss sources not in the current scene — use the global audio output enumeration via `obs_enum_all_sources` instead, filtered by `OBS_SOURCE_AUDIO`.

6. **`gs_matrix_rotaa4f` vs `gs_matrix_rotaa4f_abs`.** Rotation accumulates on the matrix stack; ensure pivot-point transforms (translate to pivot, rotate, translate back) are handled explicitly for per-layer pivot points.

7. **`obs_module_text` requirement.** Technically optional (OBS falls back to the key string). Practically required for plugin review submission. Plan for localization from the start but it is not a blocker for initial development.

8. **`save` / `load` callbacks.** These fire when the scene collection is saved/loaded, in addition to `update`. If any source state is NOT in `obs_data_t` settings (e.g., runtime animation state), `save`/`load` are where you persist it. For MVP, all persistent state goes through properties/settings, so these can be NULL.

---

## Sources

- OBS Studio source: `libobs/obs-source.h` (obsproject/obs-studio GitHub, main branch) — training knowledge through OBS 30.x, August 2025. Confidence: MEDIUM (unverified live fetch).
- OBS Studio source: `libobs/obs-module.h` — training knowledge. Confidence: MEDIUM.
- OBS Studio source: `libobs/graphics/graphics.h` — training knowledge. Confidence: MEDIUM.
- OBS Studio source: `libobs/obs-audio-controls.h`, `libobs/media-io/audio-io.h` — training knowledge. Confidence: MEDIUM.
- OBS Plugin Development Guide (obsproject.com/docs) — cross-referenced with header knowledge. Confidence: MEDIUM.
- Reference plugin: `obs-studio/plugins/obs-browser` and `obs-studio/plugins/image-source` — patterns for `OBS_SOURCE_CUSTOM_DRAW`, texture management, property enumeration. Confidence: MEDIUM.

**Note:** All `gs_*` and `obs_*` APIs should be verified against the current `obsproject/obs-studio` `master` branch before implementation. The core struct layout and callback contracts have been stable since OBS 26, but new fields are added regularly.
