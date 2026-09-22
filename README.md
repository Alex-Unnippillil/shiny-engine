# Shiny Engine

Local-first media enhancement for Chrome/Edge, with an optional Windows companion.
**Version 0.2.0 improves the browser workflow. It remains a spatial-enhancement alpha, not a DLSS 5 implementation.**

This implements the useful spatial baseline from [the supplied research plan](docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md).
The plan is retained unchanged as a historical input. [Implementation status](docs/implementation-status.md) describes what is actually delivered; roadmap proposals are not finished features.

## New in 0.2.0

The Media Lab now puts source selection above the preview, with an on-image draggable comparison divider, explicit Original / Split view / Enhanced buttons, and fullscreen. Keyboard and touch controls remain available. Help (`?`) lists shortcuts and never dismisses your media when closed with Escape.

Local videos have a single processed preview with play/pause, seek, volume/mute, and playback speed. Paused and hidden video sessions stop continuous processing. Original video controls remain available when the renderer cannot be used.

Opening corrupt images/videos or cancelling window sharing preserves the active source. Loading can be cancelled; late streams are stopped, and out-of-order file loads cannot replace the newest source. Presets, custom adjustments, scale and comparison settings persist locally. **Try demo** keeps adjustments; **Reset adjustments** restores Balanced without replacing the source.

PNG export explicitly chooses enhanced, original, or the current comparison view, with source/output dimensions and safe filenames. Still-frame diagnostics can be cancelled. The extension popup explains supported players and disables page-specific actions on unavailable tabs. Companion setup shows the extension ID when opened from the installed extension.

The Windows executable is unchanged in this browser-focused release. See [the UX change and validation notes](docs/ux-release.md) for scope and test coverage.

## What is included

- A responsive local image/video lab with WebGPU processing and a WebGL2 fallback, original/enhanced comparison, restrained sharpening, low-contrast smoothing, 1–2× spatial resampling, PNG export, and transparent frame-completion diagnostics.
- A Manifest V3 extension with a toolbar popup, explicit activation for supported top-frame HTML5 video, a separate tab-capture viewer, and optional native messaging. It requests no persistent all-site host permission.
- A C++20 Windows Graphics Capture companion with Direct3D 11 spatial processing into a **separate SDR preview window**, system source selection, pause/compare/stop controls, and the global **Ctrl+Shift+F10** emergency stop.
- Strict protocol validation, local-only processing, model-provenance gates, reproducible builds, automated browser tests, and a Windows compile/protocol/HLSL test job.

The spatial shader is original code, not NVIDIA Image Scaling, Anime4K, a neural model, or reconstructed ground-truth detail. Neural rendering, animation models, photo-restoration models, frame generation, HDR, and a click-through desktop overlay are **not implemented/enabled**. No NVIDIA DLLs or model weights are bundled.

## Run the browser lab

Install Node.js 22 or later, then:

```sh
git clone https://github.com/Alex-Unnippillil/shiny-engine.git
cd shiny-engine
npm ci --ignore-scripts
npm run dev
```

Open `http://127.0.0.1:4173`. The deterministic demo is generated locally. Use **Open image or video**, or drag a supported file into the lab. Nothing is uploaded. Use **B** to compare with the original and **Escape** or **Stop session** to release the source.

PNG, JPEG, WebP and AVIF images and browser-decodable MP4/WebM/Ogg videos are accepted. Images are limited to 50 MiB / 33 megapixels and videos to 2 GiB. The processing output is capped at 4096 pixels on an axis and approximately 8.3 megapixels. Output dimensions are shown, including any cap. Video export/recording is not provided; Export PNG saves one still frame in the selected enhanced, original, or comparison view.

**Share a window** asks for browser consent. Do not select the viewer itself. Browser monitor capture is disabled to avoid feedback. Audio remains with the original source application. Captured streams are released on stop or page closure.

## Install the Chrome or Edge extension

```sh
npm run build
```

Open `chrome://extensions` or `edge://extensions`, enable **Developer mode**, choose **Load unpacked**, and select the generated **`dist/extension`** directory. Do not select the repository root. Pin the extension and open **Media Lab** from its popup.

**Enhance supported video** is deliberately limited to readable, unprotected, top-frame HTML5 videos with native controls and a supported layout. It does not claim universal YouTube/player/iframe support. It leaves the original player in control of audio and reserves its native controls. **Restore original video** and the in-page stop button remove the enhancement.

**Capture tab in separate viewer** requests the optional capture permission and opens a separate viewer. It does not inject the captured output into its source tab. Browser capture protection remains in effect. Real tab-capture consent and audio behavior require platform testing; they are not certified by the headless test suite.

Installable extension/site ZIPs are produced by `npm run package`. GitHub Actions attaches the build ZIPs, checksums, browser report and screenshots to its successful browser job. These are developer builds, not Chrome Web Store listings.

## Build or install the optional Windows companion

Requires Windows 11 x64, Visual Studio C++ build tools, a Windows SDK with C++/WinRT, and CMake 3.24 or newer. **The Windows job compiles the executable and its HLSL and tests protocol validation; it does not validate live capture or a physical GPU.** Use non-sensitive SDR test content first.

```powershell
cmake -S native/windows -B build/native -A x64
cmake --build build/native --config Release --parallel
ctest --test-dir build/native -C Release --output-on-failure
.\build\native\Release\shiny-native.exe
```

Alternatively, download **shiny-engine-windows-unsigned-alpha** from a successful Native Windows Actions run. It contains the executable, install/uninstall scripts and checksum. The build is unsigned; verify its source/checksum and follow your organization's execution policy rather than disabling protections globally.

To connect your unpacked extension, copy its 32-character extension ID from the browser extensions page and run:

```powershell
.\native\windows\install.ps1 -ExtensionId 'YOUR_EXTENSION_ID' -Executable '.\build\native\Release\shiny-native.exe'
```

For an extracted CI package, run its `install.ps1` beside its `shiny-native.exe` and omit `-Executable`. The installer writes only this user's `LocalAppData\ShinyEngine` folder and Chrome/Edge native-host registry entries. It does not create an elevated service, scheduled task, autostart entry or firewall rule.

Open the lab **from the extension**, click **Connect companion**, grant the optional native-messaging permission, then **Choose source**. The system picker stays authoritative. Stop from the lab or native window, or press **Ctrl+Shift+F10**. Pausing stops preview updates but retains capture; **Stop** releases capture. The native preview is not a click-through replacement for the original application. Close it explicitly when finished; closing the browser lab alone is not the native stop mechanism.

Run `native/windows/uninstall.ps1` (or the extracted package's `uninstall.ps1`) after closing the companion to remove its per-user registration and files. Remove the extension separately. Changing the unpacked extension ID requires reinstalling the native registration.

## Verification

```sh
npm run check
python -m pip install playwright==1.57.0
python -m playwright install chromium
npm run test:browser
npm run package
```

Browser automation executes the real spatial shader on Chromium's software graphics path and exercises file import, output resizing, PNG export, diagnostics, lifecycle and extension entry points. Software rendering is a functional test, not a physical-GPU performance result. The app's strict CSP is retained during tests. A managed browser policy may prevent local testing; do not disable that policy. Use the repository's isolated CI job instead.

See [support and known limits](docs/support-matrix.md), [benchmark method](docs/benchmark-method.md), [security/privacy](docs/threat-model.md), and [model provenance](docs/model-provenance.md). Check the exact commit's **browser-build** and **windows-build** results before using artifacts.

## Architecture

```text
apps/viewer       Local media lab, comparison, export and diagnostics
apps/extension    MV3 permissions, supported inline mode, tab viewer and host bridge
packages/gpu-web  WebGPU/WebGL2 spatial filters and bounded frame scheduling
packages/contracts Versioned validated settings/native commands
native/windows    C++20 system picker, Direct3D capture/processing/preview
models/registry.json Metadata only; neural backends explicitly disabled
docs              Preserved research, implementation status, safety and validation
```

Browser sources are imported into GPU textures without a JavaScript full-frame pixel loop. Native frames stay within the native process and are copied GPU-to-GPU before processing. Native messaging carries only validated commands and compact status data; it is not a frame transport. Temporal neural processing and D3D/Vulkan interoperability remain separate research work.

MIT for the original application. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Shiny Engine is independent and is not affiliated with or endorsed by NVIDIA.
