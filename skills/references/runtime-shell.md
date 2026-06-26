# Minimal SDK Runtime

Use this reference when replacing the old external `godot-minigame-sdk` dependency with the public runtime shell shipped in this skill.

## Bundled Files

- `assets/min-runtime/godot-sdk.js`
- `assets/min-runtime/godot-loader.js`
- `scripts/install_min_runtime.py`

## What Current Godot Actually Uses

The bundled public adapter depends on only these pieces from the old SDK contract:

- `GODOTSDK.audio.WEBAudio.audioContext`
  - used by `platform/web/js/libs/library_godot_audio.js`
- `fsUtils.localFetch`
  - used by `platform/web/js/engine/preloader.js`
- keyboard adapter methods on `__globalAdapter`
  - used by `platform/web/js/libs/library_godot_input.js`
- `GameGlobal.GODOTSDK` as the host object that the WXMEMFS layer augments with:
  - `releasePck`
  - `getWxPath`
  - `getGodotPath`
- `GameGlobal.nowPolyfill`
  - required by the bundled `godot_process.js` patching step
- `GameGlobal.__godotMinigamePixelRatio`
  - synchronized from `wx.getWindowInfo().pixelRatio` when available so the loader and generated Godot display code render sharply on iOS high-DPI devices
  - `godot-loader.js` installs a `window.devicePixelRatio` getter only when the host property is configurable, and otherwise avoids assigning to read-only runtime properties

Keep these compatibility entries even though the current Godot-side runtime does not call them directly:

- `GODOTSDK.startGame`
- `GODOTSDK.copy_to_fs`
- `GODOTSDK.load_pack`
- `GameGlobal.GodotLoader`

## Preserved Global Names

The minimal runtime intentionally preserves:

- `GameGlobal.GODOTSDK`
- `window.fsUtils`
- `globalThis.fsUtils`
- `window.__globalAdapter`
- `globalThis.__globalAdapter`
- `GameGlobal.nowPolyfill`
- `GameGlobal.GodotLoader`

## What The Minimal `godot-sdk.js` Contains

- `nowPolyfill`
- `fsUtils.localFetch`
- `fsUtils.loadSubpackage`
- keyboard-only `__globalAdapter` methods
- `GODOTSDK.audio.WEBAudio.audioContext`
- `GODOTSDK.startGame`
- `GODOTSDK.copy_to_fs`
- `GODOTSDK.load_pack`
- `WXWebAssembly -> WebAssembly` override

## What The Minimal `godot-loader.js` Handles

- Uses `wx.getWindowInfo().pixelRatio`, `GameGlobal.__godotMinigamePixelRatio`, or `devicePixelRatio` for high-DPI loading output
- Loads images after installing `onload`/`onerror` handlers and retries relative/absolute local paths
- Draws the loading background with cover behavior instead of stretching it
- Normalizes subpackage progress whether WeChat reports `0..100` or `0..1`
- Flushes and commits the WebGL loading frame when the runtime exposes explicit swap control

## What Was Deliberately Removed

Do not reintroduce these unless a real usage check proves they are required:

- the old full `godot-minigame-sdk` directory
- unrelated helper families from the legacy SDK
- multi-platform wrappers for non-WeChat targets
- test-only APIs

## Recommended Load Order

Load these before generated `godot.js`:

1. `godot-sdk.js`
2. `godot-loader.js`
3. generated `godot.js`

That order ensures:

- `nowPolyfill` exists before patched `godot.js` runs
- `fsUtils.localFetch` exists before `preloader.js` runs
- `GODOTSDK.audio.WEBAudio.audioContext` exists before `library_godot_audio.js` initializes
- `GameGlobal.GODOTSDK` exists before the WXMEMFS layer augments it

## Copy Strategy

Vendor both files into the downstream minigame host project and version them there. Do not keep them as a machine-local dependency.
