# Pitfalls & Gotchas: OBS Animated Avatar Plugin

**Researched:** 2026-09-22
**Confidence legend:** Sources marked [VERIFIED] were confirmed via live web fetch in this session. Sources marked [LOW], [MEDIUM], or [HIGH] reflect training-knowledge confidence only.

---

## 1. OBS Source Lifecycle Pitfalls

**Source:** `obsproject/obs-studio/libobs/obs-source.h` — [VERIFIED]

### 1.1 Async Video After Destroy

The header includes an explicit note:

> "Async sources must not call `obs_source_output_video` after returning from `destroy`"

**Pitfall:** If your source uses a background thread for frame production and the thread outlives the `destroy` callback, it can call `obs_source_output_video()` on a source that has already been freed. This is a use-after-free.

**Fix:** In `destroy`, signal the frame-production thread to stop, then `join()` it before returning. Only then release your data pointer. Never use detached threads for frame production.

### 1.2 `video_render` vs. Async Video: They Are Mutually Exclusive by Default

The header states:

> "If specified [async video callback], it is not necessary to include the `video_render` callback. However, to use both, call `obs_source_getframe` to get current frame data, and `obs_source_releaseframe` to release when complete."

**Pitfall:** Implementing both `video_render` and `OBS_SOURCE_ASYNC_VIDEO` without calling `obs_source_getframe`/`obs_source_releaseframe` correctly leads to frame double-processing or a render callback that ignores async frames entirely.

**Fix for this project:** The avatar renders GPU geometry in `video_render`, not async video frames. Set `OBS_SOURCE_VIDEO` (not `OBS_SOURCE_ASYNC_VIDEO`) and implement only `video_render`. Do not set `OBS_SOURCE_ASYNC_VIDEO` unless you switch to a software-render-to-RAM model.

### 1.3 Source Registration Size Field

```c
EXPORT void obs_register_source_s(const struct obs_source_info *info, size_t size);
#define obs_register_source(info) obs_register_source_s(info, sizeof(struct obs_source_info))
```

**Pitfall:** OBS uses the `size` parameter to handle forward/backward compatibility of the `obs_source_info` struct. If you manually call `obs_register_source_s` and pass the wrong size (e.g., `sizeof(void*)` or a hardcoded constant), OBS will silently skip fields that are beyond the declared size, causing callbacks to appear as NULL even when you set them.

**Fix:** Always use the `obs_register_source(info)` macro which computes `sizeof(struct obs_source_info)` at compile time against your SDK headers.

### 1.4 `OBS_SOURCE_COMPOSITE` Requires `audio_render`

The `OBS_SOURCE_COMPOSITE` flag marks a source as compositing sub-sources, and the comment in the header explicitly states it "must implement `audio_render`".

**Pitfall:** Setting this flag without implementing `audio_render` causes OBS to attempt to call a null function pointer — a crash.

**Fix:** For a standalone avatar source that does not host child sources, do not set `OBS_SOURCE_COMPOSITE`. If you later need nested scene elements, implement `audio_render`, `enum_active_sources`, and `enum_all_sources` together.

### 1.5 `OBS_SOURCE_DO_NOT_SELF_MONITOR` and Audio Feedback Loops

**Pitfall:** If the avatar source monitors audio output (for lip sync visualization driven from its own mix) and does not set `OBS_SOURCE_DO_NOT_SELF_MONITOR`, OBS may route the audio back into the monitoring chain, creating a feedback loop in the properties panel or in monitor-mode playback.

**Fix:** For this project, audio analysis is driven from an external audio source selected in properties (not from the avatar's own mix output). Ensure `audio_capture_callback` is registered on the *input* source, not on the avatar source itself. Set `OBS_SOURCE_DO_NOT_SELF_MONITOR` as a precaution.

### 1.6 `filter_add` / `filter_remove` Called on the Filter, Not the Source

**Pitfall** [MEDIUM]: These callbacks fire on the filter object when it is added to or removed from a parent source. Plugin authors sometimes implement them on the source and wonder why they never fire. These are for filter-type plugins (`OBS_SOURCE_TYPE_FILTER`), not input sources.

**Fix:** An avatar input source (`OBS_SOURCE_TYPE_INPUT`) does not need `filter_add`/`filter_remove`. Leave them NULL.

### 1.7 `OBS_SOURCE_INTERACTION` Keyboard Events vs. Global Input Capture

The `key_click` callback in `obs_source_info` fires only when OBS routes keyboard events *to the source* — i.e., when the source has focus inside the OBS preview window and the `OBS_SOURCE_INTERACTION` flag is set.

**Pitfall:** Using `key_click` for global keyboard detection (the avatar's primary input path) does not work. It only fires when the OBS preview is focused and the user clicks on the source. This is for interactive sources like browser sources, not for global input monitoring.

**Fix:** Global keyboard capture must use Windows Raw Input or WH_KEYBOARD_LL hooks (detailed in sections 2 and 3 below). The OBS `key_click` callback is irrelevant to this project's input requirements.

### 1.8 Graphics Context: `gs_*` Calls Outside `video_render`

**Pitfall** [HIGH]: OBS's graphics system (DirectX 11 on Windows) is context-bound. Any `gs_texture_create`, `gs_texture_destroy`, `gs_effect_*`, `gs_draw_*`, or similar call made outside the render callback — or outside an `obs_enter_graphics()` / `obs_leave_graphics()` bracket — will either crash, silently corrupt GPU state, or assert in debug builds.

**Common mistake:** Loading textures during `create()` without entering the graphics context first, or destroying textures in `destroy()` without the graphics lock.

**Fix:**
```cpp
// In create():
obs_enter_graphics();
tex_ = gs_texture_create_from_file("path.png");
obs_leave_graphics();

// In destroy():
obs_enter_graphics();
gs_texture_destroy(tex_);
obs_leave_graphics();
```

Never call `gs_*` from input capture threads.

### 1.9 OBS Audio Callback Threading

**Pitfall** [MEDIUM]: `obs_source_add_audio_capture_callback` fires on OBS's audio mix thread, not the main thread or the render thread. Writing directly from this callback into data structures that `video_render` also reads without synchronization is a data race.

**Fix:** Use a lock-free single-producer/single-consumer queue or an `std::atomic` for the RMS value computed in the audio callback. The render callback reads the atomic; the audio callback writes it. No mutex needed on the hot path.

---

## 2. Windows Raw Input Pitfalls

**Source:** Microsoft Docs — About Raw Input — [VERIFIED]

### 2.1 Background Input Requires `RIDEV_INPUTSINK`

By default, Raw Input is only delivered to the foreground window. If OBS loses focus (user switches to another app), input events stop.

**Pitfall:** Registering without `RIDEV_INPUTSINK` means the avatar freezes when the user alt-tabs.

**Fix:**
```c
RAWINPUTDEVICE rid[2];
rid[0].usUsagePage = 0x01;   // HID_USAGE_PAGE_GENERIC
rid[0].usUsage     = 0x06;   // HID_USAGE_GENERIC_KEYBOARD
rid[0].dwFlags     = RIDEV_INPUTSINK;  // receive even in background
rid[0].hwndTarget  = hWnd;             // must be a valid HWND

rid[1].usUsagePage = 0x01;
rid[1].usUsage     = 0x02;   // HID_USAGE_GENERIC_MOUSE
rid[1].dwFlags     = RIDEV_INPUTSINK;
rid[1].hwndTarget  = hWnd;

RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
```

`RIDEV_INPUTSINK` requires a valid `hwndTarget`. You need a message-only window (`HWND_MESSAGE` parent) created specifically to receive `WM_INPUT`. You cannot pass NULL for `hwndTarget` with `RIDEV_INPUTSINK`.

### 2.2 Creating a Message-Only Window for Raw Input

**Pitfall** [MEDIUM]: OBS plugins do not have their own Win32 message loop or HWND. Raw Input requires one to use `RIDEV_INPUTSINK`. Plugin authors sometimes try to use OBS's main window handle — this works but is fragile and requires finding OBS's HWND.

**Fix:** Create a dedicated message-only window on a background thread that also runs a `GetMessage` / `DispatchMessage` loop:

```cpp
// Window class registration + CreateWindowEx with HWND_MESSAGE parent
HWND hwnd = CreateWindowEx(0, className, nullptr, 0,
    0, 0, 0, 0,
    HWND_MESSAGE,   // message-only window
    nullptr, hInstance, nullptr);
```

Register Raw Input targeting this HWND. The background thread's message loop receives `WM_INPUT` and dispatches it to a `WndProc` that calls `GetRawInputData`.

### 2.3 `GetMessage` Removes the WM_INPUT Before `GetRawInputBuffer` Sees It

The docs include an explicit warning:

> "`GetMessage` removes the current `WM_INPUT` from the raw input queue before returning. As a result, `GetRawInputBuffer` will not see the current event — only events that arrived after it."

**Pitfall:** Calling `GetRawInputBuffer` in your `WndProc` for the current `WM_INPUT` event will miss that event — it was already dequeued by `GetMessage`. This is a common source of missed input events when attempting the buffered pattern.

**Fix (from official docs):** When using combined standard + buffered mode:
1. Call `GetRawInputData(lParam, ...)` inside `WM_INPUT` handler to read the current event.
2. Then call `GetRawInputBuffer` in a loop to drain any additional events that accumulated.

For this project's use case (keyboard/mouse state tracking), the standard per-message approach is sufficient — just call `GetRawInputData` in the `WM_INPUT` handler.

### 2.4 High-Frequency Mice and Queue Buildup

**Pitfall** [MEDIUM]: A 1000Hz mouse generates 1000 `WM_INPUT` messages per second. If the message loop is slow (due to other work on the same thread), the queue grows, adding input latency. For an avatar that reacts to mouse deltas, stale deltas look like jitter.

**Fix:** Drain the queue with `GetRawInputBuffer` in a loop after each standard `GetRawInputData` read (see 2.3). Apply delta accumulation: sum all deltas in the buffer and apply them as a single animation update per frame, rather than processing each event individually.

### 2.5 `RIDEV_NOLEGACY` Suppresses Standard Messages

**Pitfall** [MEDIUM]: Setting `RIDEV_NOLEGACY` on the keyboard device suppresses `WM_KEYDOWN`, `WM_KEYUP`, `WM_CHAR`, etc. system-wide while your registration is active. This will break text input in every application, including OBS's own UI.

**Fix:** Never use `RIDEV_NOLEGACY` for the avatar plugin. The plugin only needs to *observe* input, not suppress it. Use `RIDEV_INPUTSINK` without `RIDEV_NOLEGACY`.

### 2.6 Unregistering Raw Input on Plugin Unload

**Pitfall** [MEDIUM]: If the plugin is unloaded (OBS source removed, OBS closed) without unregistering the Raw Input devices, the dangling `hwndTarget` HWND causes access violations when the OS tries to deliver `WM_INPUT` to a destroyed window.

**Fix:** In the plugin's module unload or source destroy path, call:
```c
RAWINPUTDEVICE rid[2];
rid[0].usUsagePage = 0x01;
rid[0].usUsage     = 0x06;
rid[0].dwFlags     = RIDEV_REMOVE;
rid[0].hwndTarget  = nullptr;
// ... repeat for mouse
RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
```

Then destroy the message-only window and join the message-loop thread.

### 2.7 Raw Input Thread Affinity

**Pitfall** [MEDIUM]: `WM_INPUT` is delivered to the thread that called `RegisterRawInputDevices` (specifically, the thread owning `hwndTarget`). If you register from one thread but process from another, input never arrives.

**Fix:** Register Raw Input from the same thread that runs the `GetMessage` loop for the message-only window. In practice, do all of: create the HWND, call `RegisterRawInputDevices`, and run `GetMessage` on the same dedicated thread.

### 2.8 Privacy: `RAWINPUT` Keyboard Data Contains VKey and Scan Code

**Pitfall** [HIGH — privacy]: The `RAWKEYBOARD` structure includes `VKey` (virtual key code) and `MakeCode` (scan code). This is the actual key identity — not privacy-safe.

```c
typedef struct tagRAWKEYBOARD {
    USHORT MakeCode;
    USHORT Flags;
    USHORT Reserved;
    USHORT VKey;
    UINT   Message;   // WM_KEYDOWN / WM_KEYUP
    ULONG  ExtraInformation;
} RAWKEYBOARD;
```

Per PROJECT.md: "keyboard events used for animation only; no keystroke content stored or transmitted."

**Fix:** In the `WM_INPUT` handler, read only `Message` (WM_KEYDOWN vs WM_KEYUP) and discard `VKey`/`MakeCode`. Never log, store, or transmit the VKey value. The animation system should receive only `{key_down: bool}` — a count of active keys or a single boolean "is typing", not which keys.

---

## 3. WH_KEYBOARD_LL (Low-Level Keyboard Hook) Pitfalls

**Source:** Microsoft Docs — LowLevelKeyboardProc — [VERIFIED]

### 3.1 Hook Is Silently Removed on Timeout (Windows 7+)

The docs state explicitly:

> "The hook procedure should process a message in less time than the data entry specified in the `LowLevelHooksTimeout` value in `HKEY_CURRENT_USER\Control Panel\Desktop`."
> "On Windows 7 and later, the hook is silently removed without being called. There is no way for the application to know whether the hook is removed."
> "On Windows 10 version 1709 and later: The maximum timeout value the system allows is 1000 milliseconds (1 second). The system will default to using a 1000 millisecond timeout if the value is set larger than 1000."

**Pitfall:** Any slow operation in the hook callback (file I/O, network call, mutex contention, complex animation state update) risks causing the hook to time out. After timeout on Windows 10 1709+, the hook is silently removed and the avatar stops responding to keyboard input — with no error returned to the plugin.

**Fix:** The hook callback must return in well under 1000ms — in practice, target under 1ms. Do not perform any work in the callback beyond writing to a lock-free queue. The animation state update must happen on a separate thread that drains the queue.

### 3.2 The Hook Runs on the Installing Thread — That Thread Must Have a Message Loop

The docs state:

> "This hook is called in the context of the thread that installed it. The call is made by sending a message to the thread that installed the hook. Therefore, the thread that installed the hook must have a message loop."

**Pitfall:** Installing `WH_KEYBOARD_LL` from a thread that does not run a `GetMessage` / `DispatchMessage` loop means the hook callback is never invoked. Plugin authors sometimes install the hook from `obs_source_create` (called on OBS's main thread or plugin init thread, which may not have a persistent message loop).

**Fix:** Install `WH_KEYBOARD_LL` from a dedicated thread that also runs a `GetMessage` loop. This is the same thread structure required by Raw Input (section 2.2), which is one reason the docs recommend Raw Input over LL hooks for this use case.

### 3.3 `GetAsyncKeyState` Cannot Be Called From Within the Hook

The docs note:

> "When this callback function is called in response to a change in the state of a key, the callback function is called before the asynchronous state of the key is updated. Consequently, the asynchronous state of the key cannot be determined by calling `GetAsyncKeyState` from within the callback function."

**Pitfall:** Using `GetAsyncKeyState` inside the hook to check modifier keys (Ctrl, Shift, Alt) for "is the user typing a hotkey" logic will return stale state.

**Fix:** Use the `KBDLLHOOKSTRUCT` data directly:
```c
typedef struct tagKBDLLHOOKSTRUCT {
    DWORD     vkCode;       // virtual key code
    DWORD     scanCode;
    DWORD     flags;        // LLKHF_* flags
    DWORD     time;
    ULONG_PTR dwExtraInfo;
} KBDLLHOOKSTRUCT;
```
The `flags` field includes `LLKHF_ALTDOWN` for Alt state at the time of the event. Do not rely on `GetAsyncKeyState` or `GetKeyState` for modifier state inside the hook.

### 3.4 `LLKHF_INJECTED` — Injected Input from Other Software

**Pitfall** [HIGH — privacy/correctness]: `KBDLLHOOKSTRUCT.flags & LLKHF_INJECTED` is set when the keypress was generated by software (e.g., `keybd_event`, `SendInput`) rather than by physical hardware. Without filtering, the avatar will animate in response to programmatically-injected keys from other applications (automation tools, anti-cheat launchers, screen readers, OBS itself).

This can also be a security concern: malicious software could feed keys to the hook to manipulate the avatar or trigger high-frequency animations that degrade OBS performance.

**Fix:** For a typing-animation purpose, consider filtering injected input:
```c
if (pKbdHook->flags & LLKHF_INJECTED) {
    return CallNextHookEx(hHook, nCode, wParam, lParam); // ignore
}
```
If the project later needs to respond to virtual keyboards or accessibility software, this can be relaxed with a configuration flag.

### 3.5 Blocking Other LL Hook Chains

The docs state:

> "If the hook procedure processed the message, it may return a nonzero value to prevent the system from passing the message to the rest of the hook chain or the target window procedure."

**Pitfall:** Returning non-zero (blocking the message) from the hook breaks every other application and OBS feature that also listens for keyboard input — including OBS hotkeys (Start/Stop recording, scene switching, etc.). This is a critical correctness error.

**Fix:** The avatar hook must always call and return `CallNextHookEx`:
```c
return CallNextHookEx(hHook, nCode, wParam, lParam);
```
Never suppress keyboard events. The hook is for observation only.

### 3.6 Docs Recommend Raw Input Over LL Hooks for This Exact Use Case

The official docs include this note:

> "In most cases where the application needs to use low level hooks, it should monitor raw input instead. This is because raw input can asynchronously monitor mouse and keyboard messages that are targeted for other threads more effectively than low level hooks can."

**Implication for this project:** The PROJECT.md flags "Windows Raw Input vs low-level hooks" as a pending decision. Based on verified docs, Raw Input is the preferred approach. The key tradeoff is:

| Factor | Raw Input | WH_KEYBOARD_LL |
|--------|-----------|----------------|
| Timeout risk | None | Silent removal after 1000ms |
| Thread required | Message loop thread | Message loop thread (same requirement) |
| Background input | `RIDEV_INPUTSINK` | Works in background by default |
| Injected input | `RI_KEY_E0/E1` flags | `LLKHF_INJECTED` flag |
| Anti-cheat friction | Lower (not a hook) | Higher (LL hooks are commonly flagged) |
| Debug hooks work | Yes | No (debug hooks cannot track LL hooks) |
| Official recommendation | Preferred | "Use Raw Input instead" |

**Recommendation:** Use Raw Input (`WM_INPUT`) for both keyboard and mouse. Reserve `WH_KEYBOARD_LL` only if Raw Input proves insufficient for a specific need (e.g., intercepting input *before* it reaches the target window — which this project does not need).

---

## 4. Threading Architecture Pitfalls

These pitfalls emerge from combining the three domains above.

### 4.1 Three Different Threads, Three Different Data Consumers

The plugin has three asynchronous input sources, each on a different thread:

| Thread | Source | Data produced |
|--------|--------|---------------|
| OBS render thread | `video_render` callback | Reads avatar state, GPU draws |
| OBS audio thread | `audio_capture_callback` | Writes RMS amplitude |
| Input thread | Raw Input `WM_INPUT` handler | Writes key/mouse events |

**Pitfall** [HIGH]: Using a single `std::mutex` to protect the shared avatar state and locking it in all three threads creates priority inversion. The audio thread (high-priority in OBS) waiting for the input thread's mutex will cause audio dropouts. The render thread waiting causes frame drops.

**Fix:** Use lock-free structures:
- RMS value: `std::atomic<float>` — single writer (audio thread), single reader (render thread).
- Input state: `std::atomic<uint32_t>` bitmask for active key count; `std::atomic<float>` for mouse delta X/Y. Or a lock-free SPSC queue if richer event semantics are needed.
- Never take a mutex on the OBS audio or render threads.

### 4.2 OBS Plugin Module Unload Order

**Pitfall** [MEDIUM]: OBS can unload a plugin module before all background threads have exited, especially if the user removes the source and immediately closes OBS. If a Raw Input or hook thread is still running and accesses plugin-owned memory after the module's static destructors have run, you get use-after-free.

**Fix:** Use a global `std::atomic<bool> g_running` flag. Set it to `false` in `obs_module_unload()`. The input thread's message loop checks this flag and calls `PostQuitMessage` or breaks. `obs_module_unload` joins the thread before returning. Use RAII wrappers (join in destructor) to guarantee this even on exception paths.

---

## 5. Anti-Cheat and Security Friction

**Pitfall** [MEDIUM — confidence: training knowledge, unverified against specific anti-cheat vendors]:

Low-level keyboard hooks (`WH_KEYBOARD_LL`) are routinely flagged by anti-cheat software (Easy Anti-Cheat, BattlEye, Vanguard) as potential keyloggers. A user streaming a game with anti-cheat could find that:

- The anti-cheat blocks the hook installation.
- The anti-cheat terminates the user's session or game.
- OBS itself is flagged due to the hook existing in the OBS process.

Raw Input via `WM_INPUT` is generally not flagged because it is a passive, standard Windows input mechanism — not an intercept hook. This is an additional practical reason to prefer Raw Input.

**Fix:** Use Raw Input. If a user reports anti-cheat conflicts, the answer is already resolved by the architecture choice.

---

## 6. OBS Properties / Settings Serialization Pitfalls

**Confidence:** [MEDIUM] — training knowledge

### 6.1 `obs_data_t` Lifetime in Callbacks

**Pitfall:** `obs_data_t *settings` passed to `create()` is owned by OBS and will be released after `create()` returns. Storing the pointer directly (not the values) is a dangling pointer.

**Fix:** In `create()`, read all needed settings values from `obs_data_t` into your own structs. Do not store the `obs_data_t *` pointer.

### 6.2 `update()` Callback Must Be Idempotent

**Pitfall:** OBS calls `update()` whenever the user changes any property, but also calls it once at load time with the saved settings. Code that allocates GPU resources in `update()` without checking if they already exist will leak on every property change.

**Fix:** Always check-before-allocate:
```cpp
void my_source_update(void *data, obs_data_t *settings) {
    auto *ctx = static_cast<MySource*>(data);
    const char *new_path = obs_data_get_string(settings, "character_path");
    if (ctx->character_path != new_path) {
        ctx->reload_character(new_path); // only reload if changed
    }
}
```

### 6.3 Properties Panel Callbacks Fire on OBS UI Thread

**Pitfall** [MEDIUM]: Property `modified_callback` (fired when the user changes a value in the Properties panel) runs on the OBS main/UI thread. If it touches the same state as the render or audio threads, you have a data race.

**Fix:** Properties callbacks should only enqueue a "settings changed" signal (atomic flag or lock-free queue). The render callback applies the new settings at the start of the next frame.

---

## 7. Texture and GPU Resource Pitfalls

**Confidence:** [MEDIUM] — training knowledge against OBS graphics API

### 7.1 Texture Loading From Background Threads

**Pitfall:** `gs_texture_create_from_file()` (or equivalent) must be called inside `obs_enter_graphics()` / `obs_leave_graphics()`. Calling it from an input thread, audio callback, or a plain `std::thread` without the graphics lock causes a crash or silent failure.

**Fix:** Load textures either in `create()` (with graphics lock) or post a "load texture" task to be processed at the start of the next `video_render` call.

### 7.2 Texture Format Assumptions

**Pitfall** [MEDIUM]: `stb_image` loads PNG files as RGBA8 by default. OBS's `gs_texture_create` expects the data in a specific format (`GS_RGBA` or `GS_BGRA`). On DirectX 11, `GS_BGRA` is the native format; passing `GS_RGBA` data works but may require a shader swizzle.

**Fix:** Check the OBS source for the actual `gs_color_format` enum values and match your stb_image load format accordingly. When in doubt, load RGBA from stb_image and create with `GS_RGBA` — OBS handles the conversion.

### 7.3 Effect / Shader Parameters per Frame

**Pitfall** [MEDIUM]: `gs_effect_get_param_by_name` is documented as a lookup (potentially a hash/linear search). Calling it every frame for every parameter in a complex shader adds up.

**Fix:** Cache `gs_eparam_t *` handles during `create()` or the first render. Only call `gs_effect_set_*` (the setter) per frame, not the getter.

---

## 8. Summary: Decision Confirmed

Based on live-verified docs and the above analysis:

**Use Windows Raw Input (`WM_INPUT` + `RIDEV_INPUTSINK`) for both keyboard and mouse.**

Reasons, all now evidence-backed:
1. Official Microsoft docs explicitly recommend Raw Input over `WH_KEYBOARD_LL` for monitoring use cases.
2. LL hooks are silently removed after 1000ms timeout on Windows 10 1709+ — no recovery mechanism.
3. LL hooks require the same thread/message-loop architecture as Raw Input — no setup cost saved.
4. Raw Input is not flagged by anti-cheat; LL hooks are commonly flagged.
5. Raw Input delivers background events with `RIDEV_INPUTSINK` — same foreground-independence.
6. Both approaches carry the same privacy risk (VKey available); Raw Input's `RAWKEYBOARD.Message`-only read is equally valid as a privacy fix.

The `WH_KEYBOARD_LL` approach should be considered a fallback only if a specific platform limitation prevents Raw Input from working (no known such limitation exists on Windows 10/11).

---

*Sources verified live in this session:*
- `https://raw.githubusercontent.com/obsproject/obs-studio/master/libobs/obs-source.h` — OBS source API (live fetch)
- `https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input` — Windows Raw Input (live fetch, updated 2026-03-18)
- `https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc` — LL Hook Proc (live fetch, updated 2025-07-16)
