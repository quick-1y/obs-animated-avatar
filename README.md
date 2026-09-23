# Animated Avatar Plugin for OBS

A zero-overhead, privacy-safe native OBS avatar that reacts to user activity (keyboard, mouse, microphone amplitude) in real time without requiring a separate capture window, external application, or game engine.

## Requirements

- OBS Studio 30.0 or later (documented compatibility floor; confirmed working under OBS 32.2.2)
- Windows 10/11 x64
- Visual Studio 2022 (17.x) with the "Desktop development with C++" workload
- CMake 3.28–3.30 on PATH (the VS 2022-bundled CMake 3.29.5 works but is not on PATH by default — either add it to PATH or invoke it directly, e.g. `"D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"`)
- Windows SDK 10.0.22621 or later
- Git with submodule support

## Building

```
cmake --preset windows-x64
cmake --build --preset windows-x64
```

The first configure downloads the pinned OBS 31.1.1 sources and obs-deps prebuilt dependencies (~100-300MB) and builds libobs from source, which can take tens of minutes. Subsequent configures are fast.

The build produces `build_x64/RelWithDebInfo/obs-animated-avatar.dll`, built with the RelWithDebInfo configuration linking the Release CRT (`/MD`). A plugin built against the pinned OBS 31.1.1 headers has been confirmed to load correctly under OBS 32.2.2 at runtime.

## Installation

Stage the build output with:

```
cmake --install build_x64 --config RelWithDebInfo --prefix <staging-dir>
```

This produces a self-contained `obs-animated-avatar/` folder:

```
obs-animated-avatar/
  bin/64bit/obs-animated-avatar.dll (+ .pdb)
  data/...
```

**Per-plugin layout (recommended, no admin rights required):** copy the staged `obs-animated-avatar/` folder into `C:\ProgramData\obs-studio\plugins\`. At runtime `obs_get_module_data_path()` then resolves to `C:\ProgramData\obs-studio\plugins\obs-animated-avatar\data`, so the default character pack lands at `C:\ProgramData\obs-studio\plugins\obs-animated-avatar\data\characters\default\`.

**Legacy layout (requires admin rights):** copy `obs-animated-avatar.dll` to `C:/Program Files/obs-studio/obs-plugins/64bit/` and the `data/` contents to `C:/Program Files/obs-studio/data/obs-plugins/obs-animated-avatar/`.

## Provenance

This project was scaffolded from [`obsproject/obs-plugintemplate`](https://github.com/obsproject/obs-plugintemplate). The template import commit in this repository's history is `9dc088d` ("Initial commit").

## TODO (before release)

- The macOS `bundleId` in `buildspec.json` is still the template default `com.example.obs-animated-avatar` — no macOS support is planned for the MVP (Windows-first per project constraints), but this should be revisited if macOS support is ever added.
- `data/locale/ru-RU.ini` is missing; OBS falls back to `en-US` text with a harmless log warning (`Failed to load 'ru-RU' text for module`) when the UI language is Russian. Not blocking; a future localisation pass should add it.
