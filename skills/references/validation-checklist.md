# Validation Checklist

Run these checks from the target Godot repo root after applying the public bundle.

## 0. Base Verification

Confirm the target commit:

```powershell
git rev-parse HEAD
```

Expected exact base for the default path:

- `a16e481cf424f8e39dc2cdea1a6bdc1e309acdc1`

If it differs, say so explicitly in the final report.

## 1. Apply Verification

If you used the bundled script, record the exact command:

```powershell
python <skill-dir>\scripts\apply_godot_patchset.py <target-repo>
```

If optional modules were included, record them too.

## 2. Static Anchors

Verify the expected WeChat hooks are present:

```powershell
rg -n "WXMEMFS|enableChunked|write_offset|getWindowInfo|showKeyboard|wx.exitMiniProgram|wasm_simd|wasm\\.br" platform/web modules
```

If the runtime shell is vendored downstream, also verify:

```powershell
rg -n "GODOTSDK\\.|fsUtils\\.|__globalAdapter\\.|nowPolyfill|GodotLoader" platform/web modules
```

## 3. Build Smoke Test

Build with conservative defaults:

```powershell
scons platform=web target=template_release threads=no wasm_simd=no
```

Intent:

- no browser-only threading assumptions
- no default SIMD
- standard web bundle produced before post-processing

## 4. Packaging Smoke Test

Run the bundled helpers:

```powershell
node <skill-dir>\scripts\godot_process.js
cmd /c <skill-dir>\scripts\compress_wasm.bat
```

Then confirm:

```powershell
Test-Path .\bin\.web_zip\godot.js
Test-Path .\bin\.web_zip\godot.wasm
Test-Path .\bin\.web_zip\godot.wasm.br
```

## 5. Generated JS Sanity

```powershell
rg -n "FS.mount\\(WXMEMFS|nowPolyfill|wx.exitMiniProgram|wasm\\.br" .\bin\.web_zip\godot.js platform/web/js/engine
```

## 6. File System Smoke Test

In WeChat DevTools or a minimal scene:

- write a file under `user://`
- close it
- reopen it immediately
- restart the app and read it again

Failure modes:

- stale read after write
- file exists only until restart
- `IDBFS` still mounted instead of `WXMEMFS`

## 7. Network Smoke Test

Use a payload large enough to span multiple internal chunks.

Expected:

- headers parse correctly
- bytes are not corrupted
- small payloads still work when only `res.data` is returned

## 8. Audio Smoke Test

Test:

- play once
- stop early
- replay
- loop

Expected:

- no crash on stop
- no stuck playback objects
- no leaked pooled contexts

## 9. Display And Input Smoke Test

Check:

- sharp output on high-DPI screens
- iOS high performance mode reports a real `pixelRatio` through `wx.getWindowInfo()`
- startup does not warn about assigning to read-only `window.devicePixelRatio`
- correct fullscreen/window sizing
- keyboard type/replace/confirm/dismiss behavior

## 10. Exit And Lifecycle Smoke Test

Confirm:

- exit follows `wx.exitMiniProgram()`
- disabled JS eval path does not crash the runtime

## 11. Completion Bar

Call the public package healthy only when all are true:

- exact-base or mismatch status was recorded
- apply step was recorded
- build passes
- post-processing produced expected files
- WXMEMFS persistence works
- large HTTP bodies are correct
- audio replay is stable
- display and keyboard work
- exit path uses WeChat lifecycle
