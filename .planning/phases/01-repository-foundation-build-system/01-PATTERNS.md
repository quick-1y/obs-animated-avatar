# Phase 1: Repository Foundation & Build System - Pattern Map

**Mapped:** 2026-09-22
**Files analyzed:** 7 new/modified files
**Analogs found:** 7 / 7

All analogs are from the obs-plugintemplate scaffold already present in the repo root. Every path below is git-tracked (verified via `git ls-files`).

---

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `CMakeLists.txt` (modify) | config | build | `CMakeLists.txt` (template) | exact — same file, rename project metadata + add sources |
| `buildspec.json` (modify) | config | build | `buildspec.json` (template) | exact — same file, update project identity fields only |
| `src/plugin-main.cpp` (rename+modify from `plugin-main.c`) | utility | request-response | `src/plugin-main.c` | exact — same role, C→C++ rename |
| `src/plugin-support.h` (keep/adapt) | utility | — | `src/plugin-support.h` | exact — keep as-is; already has `extern "C"` guards |
| `src/plugin-support.cpp` (rename+adapt from `plugin-support.c.in`) | utility | — | `src/plugin-support.c.in` | exact — same file after configure_file substitution, C→C++ rename |
| `src/avatar-source.cpp` (create new) | component | request-response | `src/plugin-main.c` | partial — same OBS integration layer; `obs_source_info` pattern has no direct analog in the greenfield repo |
| `data/locale/en-US.ini` (create or verify exists) | config | — | `data/locale/` (template installs it via `target_install_resources`) | partial — directory exists; file content is project-specific |

---

## Pattern Assignments

### `CMakeLists.txt` (modify)

**Analog:** `CMakeLists.txt` (lines 1–39 — the entire file)

**Full template content to start from** (lines 1–39):
```cmake
cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

...

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
```

**Changes required on top of the template:**
1. After `include(helpers)`, add C++20 standard lines:
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```
2. Replace the single `target_sources` call (line 37) with:
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/plugin-support.cpp
  src/avatar-source.cpp
)
```
3. After `target_link_libraries`, add the vendored headers include path:
```cmake
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/deps"
)
```
4. The `set_target_properties_plugin` call (line 39) stays as-is — `${_name}` is read from `buildspec.json` by `bootstrap.cmake`, so updating `buildspec.json` is sufficient to rename the OUTPUT_NAME.

**Do NOT change:** `cmake_minimum_required`, `bootstrap.cmake` include, `find_package(libobs)`, `target_link_libraries(... OBS::libobs)`, or the `ENABLE_FRONTEND_API`/`ENABLE_QT` options. The `${_name}` and `${_version}` variables are populated by `bootstrap.cmake` from `buildspec.json`.

---

### `buildspec.json` (modify)

**Analog:** `buildspec.json` (lines 1–45 — the entire file)

**Fields to update** (lines 39–44 only):
```json
"name": "obs-animated-avatar",
"displayName": "Animated Avatar Plugin for OBS",
"version": "0.1.0",
"author": "Your Name",
"website": "https://github.com/YOUR_USERNAME/obs-animated-avatar",
"email": "l0nglife3025@gmail.com"
```

Also update line 36 (`platformConfig.macos.bundleId`):
```json
"bundleId": "com.example.obs-animated-avatar"
```

**Do NOT change:** The entire `dependencies` block (lines 2–32), including `obs-studio.version`, `obs-studio.hashes`, `prebuilt`, and `qt6` blocks. These are SHA256-verified checksums for the build bootstrap. Changing them breaks the configure-time download.

---

### `src/plugin-main.cpp` (rename from `src/plugin-main.c`, modify)

**Analog:** `src/plugin-main.c` (lines 1–34)

**Full analog content:**
```c
#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "plugin unloaded");
}
```

**Changes required:**
1. Rename file to `plugin-main.cpp`.
2. Add forward declaration and `register_avatar_source()` call:
```cpp
#include <obs-module.h>
#include "plugin-support.h"

// Forward declaration — defined in avatar-source.cpp
void register_avatar_source();

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    register_avatar_source();
    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "plugin unloaded");
}
```
3. Include path for `plugin-support.h` changes from `<plugin-support.h>` to `"plugin-support.h"` (relative, since the file is in the same directory and no longer on a system include path).

---

### `src/plugin-support.h` (keep as-is)

**Analog:** `src/plugin-support.h` (lines 1–37)

This file already has `extern "C"` guards (lines 21–23, 35–37) and is fully compatible with C++20 compilation. No changes needed. Copy pattern verbatim:

```cpp
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern const char *PLUGIN_NAME;
extern const char *PLUGIN_VERSION;

void obs_log(int log_level, const char *format, ...);
extern void blogva(int log_level, const char *format, va_list args);

#ifdef __cplusplus
}
#endif
```

---

### `src/plugin-support.cpp` (rename+configure from `plugin-support.c.in`)

**Analog:** `src/plugin-support.c.in` (lines 1–39)

The `.c.in` file is a CMake `configure_file` template. `@CMAKE_PROJECT_NAME@` and `@CMAKE_PROJECT_VERSION@` are substituted at configure time. The output file name must match what `target_sources` references.

**How it works** (lines 21–22 of the `.c.in`):
```c
const char *PLUGIN_NAME = "@CMAKE_PROJECT_NAME@";
const char *PLUGIN_VERSION = "@CMAKE_PROJECT_VERSION@";
```
After `configure_file(src/plugin-support.c.in src/plugin-support.cpp)`, `PLUGIN_NAME` becomes `"obs-animated-avatar"` and `PLUGIN_VERSION` becomes `"0.1.0"`.

**CMakeLists.txt addition required** — add before `target_sources`:
```cmake
configure_file(src/plugin-support.c.in src/plugin-support.cpp)
```
Then reference the generated file in `target_sources`:
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/src/plugin-support.cpp"
  src/avatar-source.cpp
)
```

**Logging pattern extracted** (lines 24–38 of `plugin-support.c.in`) — all `obs_log()` calls in new files must use this function; the `[obs-animated-avatar]` prefix is inserted automatically:
```c
void obs_log(int log_level, const char *format, ...)
{
    size_t length = 4 + strlen(PLUGIN_NAME) + strlen(format);
    char *template = malloc(length + 1);
    snprintf(template, length, "[%s] %s", PLUGIN_NAME, format);
    va_list(args);
    va_start(args, format);
    blogva(log_level, template, args);
    va_end(args);
    free(template);
}
```

---

### `src/avatar-source.cpp` (create new)

**Analog:** `src/plugin-main.c` (partial — same OBS integration layer; no `obs_source_info` analog exists in the template)

No direct analog in the codebase. The RESEARCH.md Pattern 4 (lines 308–412) is the authoritative reference. Key excerpts to copy:

**Imports pattern** — use quoted includes for project-local headers, angle brackets for OBS system headers:
```cpp
#include <obs-module.h>
#include "plugin-support.h"
```

**Context struct pattern** — plain C++ struct with OBS handle + dimensions + GPU resource pointer:
```cpp
struct AvatarSourceContext {
    obs_source_t *source;
    uint32_t width  = 320;
    uint32_t height = 240;
    gs_texture_t *placeholder_tex = nullptr;
};
```

**Graphics lock pattern in `create()`** — CRITICAL: all `gs_*` calls must be inside the lock:
```cpp
obs_enter_graphics();
uint8_t purple[4] = {128, 0, 128, 255};
const uint8_t *ptr = purple;
ctx->placeholder_tex = gs_texture_create(1, 1, GS_RGBA, 1, &ptr, 0);
obs_leave_graphics();
```

**Graphics lock pattern in `destroy()`**:
```cpp
obs_enter_graphics();
gs_texture_destroy(ctx->placeholder_tex);
obs_leave_graphics();
delete ctx;
```

**Render callback pattern** — no graphics lock needed inside `video_render`; use `obs_get_base_effect`:
```cpp
static void avatar_render(void *data, gs_effect_t * /* effect */)
{
    auto *ctx = static_cast<AvatarSourceContext *>(data);
    if (!ctx->placeholder_tex) return;
    gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_eparam_t *param = gs_effect_get_param_by_name(eff, "image");
    gs_effect_set_texture(param, ctx->placeholder_tex);
    while (gs_effect_loop(eff, "Draw")) {
        gs_draw_sprite(ctx->placeholder_tex, 0, ctx->width, ctx->height);
    }
}
```

**`obs_source_info` registration pattern** — use macro form `obs_register_source()`, never `obs_register_source_s()`:
```cpp
static struct obs_source_info avatar_source_info = {
    .id           = "animated_avatar_source",  // D-01: ONE-WAY DOOR — never change
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
};

void register_avatar_source()
{
    obs_register_source(&avatar_source_info);
}
```

**Name lookup pattern** — use `obs_module_text()` so OBS localizes via `en-US.ini`:
```cpp
static const char *avatar_get_name(void * /* type_data */)
{
    return obs_module_text("AnimatedAvatar");
}
```

---

### `data/locale/en-US.ini` (create)

**Analog:** No existing analog in the template's `data/locale/` directory (the template ships an empty or absent locale file for the generic plugin name).

**Required minimum content:**
```ini
AnimatedAvatar=Animated Avatar
```

This key is consumed by `obs_module_text("AnimatedAvatar")` in `avatar_get_name()`. Without this file, OBS falls back to the raw key string. The `OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")` macro in `plugin-main.cpp` registers "en-US" as the default locale; OBS loads this file from the plugin's data directory at startup.

---

## Shared Patterns

### Logging — apply to all `.cpp` files
**Source:** `src/plugin-support.h` (lines 31–33) and `src/plugin-support.c.in` (lines 24–38)

Never call `blog()` directly in new files. Always call `obs_log()` from `plugin-support.h`. The prefix `[obs-animated-avatar]` is inserted automatically:
```cpp
obs_log(LOG_INFO, "message here");  // emits: [obs-animated-avatar] message here
obs_log(LOG_WARNING, "warn: %s", detail);
obs_log(LOG_ERROR, "error in %s", __func__);
```

### Include style — apply to all `.cpp` files
**Source:** `src/plugin-main.c` (lines 19–20)

Angle brackets for OBS system headers; quotes for project-local headers:
```cpp
#include <obs-module.h>       // OBS SDK — angle brackets
#include "plugin-support.h"   // project-local — quotes
```

### CMake MODULE + set_target_properties_plugin pattern
**Source:** `CMakeLists.txt` (lines 14, 39) + `cmake/windows/helpers.cmake` (lines 8–58)

The `set_target_properties_plugin()` macro (defined in `cmake/windows/helpers.cmake` lines 8–58) handles:
- Install destination: `${target}/bin/64bit/` (line 25)
- PDB file copy (lines 27–32)
- Post-build copy to `rundir/$<CONFIG>/` (lines 38–48)
- Data directory copy via `target_install_resources()` (line 50)

The pattern to replicate in `CMakeLists.txt`:
```cmake
add_library(${CMAKE_PROJECT_NAME} MODULE)
# ... target_sources, target_link_libraries, target_include_directories ...
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
```
Never use `obs_add_module()` — it does not exist in the current template (verified via `cmake/windows/helpers.cmake`).

### Graphics context safety — apply to `avatar-source.cpp`
**Source:** RESEARCH.md Common Pitfalls §3 (authoritative); pattern confirmed by `cmake/windows/helpers.cmake` (GPU context is render-thread-affine)

- `create()` and `destroy()`: wrap ALL `gs_*` calls in `obs_enter_graphics()` / `obs_leave_graphics()`
- `video_render()`: call `gs_*` freely — context is already held by OBS render thread
- Never call `gs_*` from a background thread or timer callback

---

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `src/avatar-source.cpp` | component | request-response | No `obs_source_info` registration exists in the template. Closest analog is `src/plugin-main.c` (same OBS integration layer). Use RESEARCH.md Pattern 4 as the primary reference. |
| `data/locale/en-US.ini` | config | — | Template ships no locale file content for the generic plugin; content is project-specific. |
| `deps/nlohmann/json.hpp` | config | — | No vendored deps exist yet. Download from https://github.com/nlohmann/json/releases (single-header release). |
| `deps/stb/stb_image.h` | config | — | No vendored deps exist yet. Download from https://github.com/nothings/stb/blob/master/stb_image.h. |

---

## Metadata

**Analog search scope:** `D:/Users/qu1ck1y/Documents/pyProjects/obs_avatar/` — all git-tracked files (`git ls-files` verified)
**Files scanned:** 7 (CMakeLists.txt, buildspec.json, CMakePresets.json, src/plugin-main.c, src/plugin-support.h, src/plugin-support.c.in, cmake/windows/helpers.cmake)
**Pattern extraction date:** 2026-09-22
**Tracked-source gate:** all analog paths verified via `git ls-files`; no gitignored mirror paths used
