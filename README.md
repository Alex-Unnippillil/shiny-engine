# Shiny Engine

Local-first media enhancement for Chrome/Edge, with an optional Windows companion.

**0.4.0 adds LookLock, a local creator review/composition workflow for imported neural outputs. Version 0.3.0 added an experimental OpenDLSS-NR browser adapter and isolated Neural Lab. No trained model is approved or bundled: live neural inference is locked in the shipped build. The default Media Lab and Windows companion remain spatial enhancement.**

The [original research plan](docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md) is preserved unchanged. [Implementation status](docs/implementation-status.md) distinguishes delivered code from proposed milestones.

## LookLock — new in 0.4.0

**Review a DLSS-treated frame without letting the treatment repaint your HUD, captions, or labels.** Open **LookLock** in the extension or Media Lab. Bring an original and a same-sized treated still, draw source-protected regions, inspect the pixel differences, and export a lossless protected composite, mask, review board, or hash-bound recipe.

Local video frame selection helps send one exact source frame to your external renderer. Mouse, touch, and keyboard coordinate controls are supported. The built-in fictional replay example is deliberately edited, **not neural-generated**. LookLock does not invoke DLSS or authenticate imported results; built-in NR inference remains gated. Read [the workflow, scope and verification notes](docs/looklock.md).

## Neural Lab

Open **Experimental Neural Lab** from the extension popup or Media Lab. The adapter includes a pinned MIT OpenDLSS-NR graph, local model inspection, SHA-256 validation, a dedicated compute worker, GPU frame conversion/composition, serial live-frame scheduling, tone/structure controls, matched-frame comparison, cancellation, PNG export and diagnostics.

This is **not an activated or validated DLSS 5 product**. The reviewed-model register is empty. No trained-model inference, NVIDIA-output parity, physical-GPU speed, native neural capture or temporal video quality has been verified here. A file selection does not unlock the gate. Do not obtain leaked assets to use this build. See [neural integration and approval](docs/neural-lab.md).

The experimental path processes independent SDR frames without optical flow or temporal history, at an explicit 320/512-pixel longest edge. These are research preview sizes, not 4K or real-time promises. The native/Vulkan integration is not implemented by this browser adapter. All reference code and shaders are bundled during build; model files are not downloaded or distributed.

## Spatial Media Lab

The spatial workbench provides WebGPU with WebGL2 fallback, restrained sharpening/smoothing, 1–2× resampling, original/split/enhanced comparison and PNG export. It supports mouse/touch/keyboard comparison, fullscreen, help, and local video playback/seek/speed/audio controls. The original player remains available on renderer failure.

Invalid files and cancelled sharing preserve the current source. Late streams are stopped and stale loads cannot replace a newer selection. Adjustments persist locally; **Try demo** preserves them and **Reset adjustments** restores Balanced without replacing media. PNG export explicitly chooses enhanced, original or comparison pixels. Still-frame diagnostics are cancellable.

These spatial shaders are original code, not DLSS or recovered ground-truth detail. Animation/photo models, frame generation, HDR and click-through desktop overlays are not enabled. No NVIDIA DLLs or model weights are included.

## Run locally

Requires Node.js 22+:

```sh
git clone --recurse-submodules https://github.com/Alex-Unnippillil/shiny-engine.git
cd shiny-engine
npm ci --ignore-scripts
npm run dev
```

For an existing checkout, update the pinned reference before building:

```sh
git submodule update --init -- third_party/opendlss-nr
```

Open `http://127.0.0.1:4173`. Neural Lab is at `/apps/nr/index.html`. In the spatial lab, **B** compares and **Escape** dismisses help/fullscreen, cancels a pending operation or stops the browser session. In Neural Lab, **Escape** stops capture and its worker.

The spatial lab accepts PNG/JPEG/WebP/AVIF images and browser-decodable MP4/WebM/Ogg videos. Images are limited to 50 MiB / 33 megapixels; videos to 2 GiB. Spatial output is capped at 4096 pixels per axis and approximately 8.3 megapixels. Actual dimensions are shown. PNG exports one still frame, not a video recording.

**Share a window** requests browser consent. Never choose this viewer as its source. Browser monitor capture is disabled to avoid feedback. Audio stays at the source application; browser streams are released on stop/page closure. Protected content may remain unavailable.

## Install the extension

Run `npm run build`. Open `chrome://extensions` or `edge://extensions`, enable **Developer mode**, select **Load unpacked**, and choose **`dist/extension`**, not the repository root. Pin its toolbar icon to open Media Lab, Neural Lab, or LookLock.

Inline enhancement is limited to readable, unprotected, top-frame HTML5 video with native controls and supported geometry. Universal YouTube/custom-player/iframe support is not claimed. Audio stays with the original player. **Restore original video** or the in-page stop control removes inline processing.

The separate tab viewer requests optional capture permission and never displays its output inside the captured tab. Real picker/audio/protection behavior still requires platform testing. No persistent all-site host permission is requested.

`npm run package` creates extension/site ZIPs and checksums. Successful browser Actions jobs attach packages, reports and screenshots. These are developer builds, not browser-store listings.

## Optional Windows companion

The unchanged C++20 companion uses Windows Graphics Capture and Direct3D 11 spatial processing in a **separate SDR preview window**. It provides the system source picker, pause/compare/stop controls and **Ctrl+Shift+F10** emergency stop. No native DLSS/Vulkan backend or click-through overlay was added in 0.3.0.

Requires Windows 11 x64, Visual Studio C++ build tools, Windows SDK with C++/WinRT, and CMake 3.24+. Windows CI compiles the executable/HLSL and tests the protocol; it does not validate live capture or a physical GPU. Use non-sensitive SDR content first.

```powershell
cmake -S native/windows -B build/native -A x64
cmake --build build/native --config Release --parallel
ctest --test-dir build/native -C Release --output-on-failure
.\build\native\Release\shiny-native.exe
```

Alternatively use the **shiny-engine-windows-unsigned-alpha** artifact from a successful Windows job. Follow local execution policy; do not disable protections globally.

Register the unpacked extension's 32-character ID:

```powershell
.\native\windows\install.ps1 -ExtensionId 'YOUR_EXTENSION_ID' -Executable '.\build\native\Release\shiny-native.exe'
```

For an extracted CI package, run its `install.ps1` beside `shiny-native.exe` and omit `-Executable`. Installation writes only the user's `LocalAppData\ShinyEngine` folder and Chrome/Edge native-host entries. No elevated service, autostart task or firewall rule is created.

Open spatial Media Lab **from the extension**, connect the companion, grant optional native-messaging permission and choose a source. Stop using the lab/native window or **Ctrl+Shift+F10**. Pause retains capture; Stop releases it. Closing the browser is not the stop mechanism for a separate native preview. Close the companion before running `uninstall.ps1`; remove the extension separately. Changing extension ID requires re-registration.

## Verify

```sh
npm run check
python -m pip install playwright==1.63.0
python -m playwright install --with-deps chromium
npm run test:browser
npm run package
```

CI pins Playwright 1.63.0 / Chromium 153. Unit tests cover settings, protocol, model hashes/approvals, serial worker cancellation and bundled imports. Spatial browser tests cover actual pixels, decoding, export, playback, responsive UI and MV3 loading. Neural tests exercise the locked UI/worker gate and GPU input/output stages with **synthetic residuals**, not a trained model. Check results for the exact commit; test definitions alone are not evidence of success.

Software graphics tests do not certify physical-GPU performance. Strict application CSP is retained. Respect managed browser policies; do not disable them to run local tests. Both **browser-build** and **windows-build** gate feature merges.

See [support matrix](docs/support-matrix.md), [benchmark method](docs/benchmark-method.md), [security](docs/threat-model.md), [model provenance](docs/model-provenance.md), and [Neural Lab boundaries](docs/neural-lab.md).

## Source layout

```text
apps/viewer             Spatial Media Lab
apps/nr                 Isolated approval-gated Neural Lab
apps/looklock           Still-frame creator review and source locks
apps/extension          MV3 permissions and session routing
packages/contracts      Validated settings/native commands
packages/gpu-web        Spatial rendering and bounded scheduling
packages/nr             Model policy, worker and graph adapter
packages/looklock       CPU still-frame composition, recipes and lossless PNG
third_party/opendlss-nr  Pinned MIT source; no weights
native/windows          Windows picker and D3D11 spatial preview
models                  Metadata, approvals and source hashes only
docs                    Preserved research, status and release gates
```

Media Lab and the neural adapter use GPU textures for their rendering paths. LookLock separately uses bounded CPU/Canvas2D processing for still-frame review; it is not a live enhancement path. Native Messaging transports controls/status only. Temporal processing and D3D/Vulkan interop remain separate work.

Original application: MIT. Reference notices are preserved in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Shiny Engine is independent and not NVIDIA-affiliated or endorsed.
