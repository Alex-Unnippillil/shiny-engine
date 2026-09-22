# Shiny Engine

Local-first media enhancement for Chrome/Edge, with an optional Windows companion.

**Version 0.3.0 adds an experimental OpenDLSS-NR browser adapter and isolated Neural Lab. No model is approved or bundled: live neural inference is locked in the shipped build. The default Media Lab and Windows companion remain spatial enhancement.**

The [original research plan](docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md) is preserved unchanged. [Implementation status](docs/implementation-status.md) distinguishes delivered code from proposed milestones.

## New in 0.3.0

Open **Experimental Neural Lab** from the extension popup or Media Lab. It includes a pinned MIT OpenDLSS-NR graph, local model inspection, a dedicated worker, GPU frame conversion/composition, serial live-frame scheduling, tone/structure controls, matched-frame comparison, cancellation, PNG export and diagnostics.

This is an adapter, **not an activated or validated DLSS 5 product**. The reviewed-model register is empty. No trained-model inference, NVIDIA-output parity, physical-GPU speed, native neural capture or temporal video quality has been verified here. A file selection or acknowledgment does not unlock the gate. Do not obtain leaked assets to use this build. See [neural integration and approval](docs/neural-lab.md).

The experimental path uses independent SDR frames without optical flow or temporal history, at an explicit 320/512-pixel longest edge. These are research preview sizes, not 4K or real-time promises. The planned native/Vulkan integration is not implemented by this browser adapter. Reference code and shaders are bundled during build; model files are never downloaded automatically or distributed.

## Spatial Media Lab

The workbench provides WebGPU processing with WebGL2 fallback, restrained sharpening/smoothing, 1–2× resampling, original/split/enhanced comparison and PNG export. Source selection sits above the preview, with mouse/touch/keyboard comparison controls, fullscreen and help (`?`). Local videos have one processed preview with play/pause, seek, speed, volume and mute. The original player remains available on renderer failure.

Invalid files and cancelled sharing preserve the active source. Loads can be cancelled; late streams are stopped and older results cannot overwrite newer selections. Presets, scale and comparison preferences persist locally. **Try demo** keeps adjustments; **Reset adjustments** restores Balanced without replacing the source. Export explicitly chooses enhanced, original, or current comparison pixels. Still-frame diagnostics are cancellable. See [0.2 UX notes](docs/ux-release.md).

Spatial shaders are original code, not NVIDIA Image Scaling, Anime4K, DLSS, neural inference or recovered ground-truth detail. No animation/photo models, frame generation, HDR or click-through desktop overlay are enabled. No NVIDIA DLLs or trained-model weights are included.

## Run locally

Install Node.js 22 or later:

```sh
git clone --recurse-submodules https://github.com/Alex-Unnippillil/shiny-engine.git
cd shiny-engine
npm ci --ignore-scripts
npm run dev
```

For an existing checkout, also run:

```sh
git submodule update --init -- third_party/opendlss-nr
```

Open `http://127.0.0.1:4173`. The demo is generated locally. Open or drop a supported file. Nothing is uploaded. **B** compares and **Escape** stops the spatial session (after dismissing help/fullscreen or cancelling a pending load). Neural Lab is at `/apps/nr/index.html`; its **Escape** stops capture and its worker.

The spatial lab accepts PNG/JPEG/WebP/AVIF images and browser-decodable MP4/WebM/Ogg videos. Images are limited to 50 MiB / 33 megapixels; videos to 2 GiB. Spatial output is capped at 4096 pixels per axis and approximately 8.3 megapixels. Actual dimensions are shown. PNG saves a still frame, not a video recording.

**Share a window** requests browser consent. Do not select the viewer itself. Browser monitor capture is disabled to avoid feedback. Audio stays with the source application. Browser streams are released on stop or page closure. Protected content may remain unavailable; no protection bypass is implemented.

## Install the extension

```sh
npm run build
```

Open `chrome://extensions` or `edge://extensions`, enable **Developer mode**, select **Load unpacked**, and choose **`dist/extension`**, not the repository root. Pin the extension and open the desired lab from its popup.

**Enhance supported video** is limited to readable, unprotected, top-frame HTML5 video with native controls and a supported layout. Universal YouTube/custom-player/iframe support is not claimed. Audio and native controls remain owned by the original player. **Restore original video** and the in-page stop control remove inline enhancement.

**Open tab in a separate viewer** requests optional capture permission and does not display processed output inside its captured source tab. Real picker/audio/protection behavior still needs platform testing; headless tests do not certify it.

`npm run package` creates installable extension/site ZIPs and checksums. Successful browser Actions jobs attach packages, reports and screenshots. These are developer builds, not browser-store listings. The extension requests no persistent all-site host permission; capture/native permissions are optional.

## Optional Windows companion

The C++20 companion uses Windows Graphics Capture and Direct3D 11 spatial processing in a **separate SDR preview window**. It has system source selection, pause/compare/stop controls and **Ctrl+Shift+F10** emergency stop. It is unchanged in 0.3.0: no native DLSS/Vulkan backend or click-through overlay was added.

Requires Windows 11 x64, Visual Studio C++ build tools, Windows SDK with C++/WinRT and CMake 3.24+. Windows CI compiles the executable and HLSL and tests protocol validation; it does not validate live capture or a physical GPU. Use non-sensitive SDR content first.

```powershell
cmake -S native/windows -B build/native -A x64
cmake --build build/native --config Release --parallel
ctest --test-dir build/native -C Release --output-on-failure
.\build\native\Release\shiny-native.exe
```

Alternatively download **shiny-engine-windows-unsigned-alpha** from a successful Native Windows Actions run. Verify source/checksum and follow your organization's execution policy; do not disable protections globally.

Register the unpacked extension's 32-character ID:

```powershell
.\native\windows\install.ps1 -ExtensionId 'YOUR_EXTENSION_ID' -Executable '.\build\native\Release\shiny-native.exe'
```

For an extracted CI package, run its `install.ps1` beside `shiny-native.exe` and omit `-Executable`. Installation writes only the user's `LocalAppData\ShinyEngine` folder and Chrome/Edge native-host registry entries. No elevated service, autostart, task or firewall rule is created.

Open Media Lab **from the extension**, connect the companion, grant optional native-messaging permission and choose a source. The system picker stays authoritative. Stop from the lab/native window or use **Ctrl+Shift+F10**. Pause retains capture; **Stop** releases it. Closing the browser lab is not the stop mechanism for a separate native preview. Run `uninstall.ps1` after closing the companion to remove its per-user files/registration; remove the extension separately. A changed extension ID requires re-registering.

## Verification and scope

```sh
npm run check
python -m pip install playwright==1.57.0
python -m playwright install chromium
npm run test:browser
npm run package
```

Unit checks cover settings/protocol validation, model approvals/hashes, worker cancellation and local import closure. Browser tests cover actual spatial pixels, decoding, export, playback, lifecycle, responsive UI and MV3 loading. Neural tests exercise the locked UI, worker gate and GPU input/output stages with a **synthetic residual**, not a trained neural model. Check the exact commit's CI result; test definitions alone are not evidence of success.

Software rendering is not a physical-GPU performance result. Strict application CSP stays enabled. A managed browser policy may prohibit local testing; do not disable it. Use the isolated CI job instead.

See [support matrix](docs/support-matrix.md), [benchmark method](docs/benchmark-method.md), [privacy/security](docs/threat-model.md), [model provenance](docs/model-provenance.md), and [neural release boundaries](docs/neural-lab.md). Both **browser-build** and **windows-build** gate feature merges.

## Architecture

```text
apps/viewer          Spatial Media Lab and comparison/export
apps/nr              Isolated approval-gated Neural Lab
apps/extension       MV3 popup, permissions and source/native routing
packages/contracts   Validated settings and native commands
packages/gpu-web     Spatial backends and bounded frame scheduling
packages/nr          Local model policy, worker and OpenDLSS adapter
third_party/opendlss-nr  Pinned MIT source submodule; no weights
native/windows       C++20 system picker and D3D11 spatial preview
models               Metadata, approvals and source hashes only
docs                 Preserved research, implementation status and gates
```

Browser frames use GPU textures rather than a production full-frame JavaScript pixel loop. Native frames stay in the native process; Native Messaging carries only controls/status. Temporal processing and D3D/Vulkan interoperability remain separate research work.

MIT for the original application; reference notices are retained. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Shiny Engine is independent, not affiliated with or endorsed by NVIDIA.
