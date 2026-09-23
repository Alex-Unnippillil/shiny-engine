# Shiny Engine

Local-first media tools for Chrome/Edge and Windows.

## 0.6.0 — native OpenDLSS workbench and Windows setup

**[Windows installers and portable packages](https://github.com/Alex-Unnippillil/shiny-engine/releases/tag/v0.6.0)** · [Player instructions](native/vlc-player/README.md) · [Native NR boundaries](docs/native-neural.md)

The Windows player now includes a compiled, pinned MIT OpenDLSS-NR Vulkan graph in an isolated worker, real libVLC frame sampling and a dedicated Native Neural Workbench. **No trained model is approved or bundled, so trained DLSS 5 inference is not activated in this distribution.** GPU feature probing and manifest inspection work without a model; they are not parity or performance tests. The existing browser Neural Lab remains separately gated.

Windows x64 Setup adds per-user installation, shortcuts and uninstall. Its optional checkbox downloads the SHA-256-verified official VLC runtime directly from VideoLAN. Otherwise use an existing compatible VLC3.0.24+ installation. The setup and portable package are unsigned developer builds, not browser-store products. Windows x64 is the only native installer currently supplied; macOS/Linux installers are not implemented. The extension provides setup navigation, not automatic native installation or launch.

The player adds cinema view, always-on-top, in-memory source bookmarks, jump-to-time, queue reordering, output-device selection and media information. The native UI keeps local playback, enhancement status and original media distinct. Conventional VLC image filters and optional RTX VSR requests are **not DLSS5**. Full VLC opens the separately installed original interface for features not duplicated here; this custom player is not complete VLC UI parity.

The [original research plan](docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md) is preserved unchanged. [Implementation status](docs/implementation-status.md) separates source implementation from missing runtime/model/hardware validation.

## Workspaces

**Windows player:** real libVLC local/explicit-stream playback, playlists, repeat/shuffle, seek/speed/A-B loop, audio/subtitle tracks, external subtitles, track timing, equalizer presets, aspect/crop/deinterlace, chapters, frame step, fullscreen and displayed-frame snapshots. Actual support depends on media and installed VLC plugins. Imported processed-video comparison uses source audio only and best-effort—not frame-exact—synchronization. [Details](native/vlc-player/README.md).

**Native Neural Workbench:** separate muted decoder, native Vulkan probe, hash-bound model policy, private worker, independent SDR frame processing, matched-frame comparison and tone/structure/blend controls. Preparation remains blocked by the empty model register. It is a bounded research preview, not a certified real-time, temporal, 4K or audio-synchronized player backend. [Details](docs/native-neural.md).

**Spatial Media Lab:** browser WebGPU with WebGL2 fallback, restrained sharpening/smoothing, 1–2× resampling, original/split/enhanced views, drag/keyboard comparison, local video transport, fullscreen, remembered settings, explicit PNG export views and cancellable diagnostics. Invalid files/cancelled sharing preserve the working source; obsolete streams/loads are disposed. These are original spatial filters, not AI or recovered detail.

**LookLock:** import a matching original and treated still, protect selected regions with exact decoded source pixels, inspect a raw change map, and export a protected PNG, mask, review board, report or hash-bound recipe. Local video frame selection supports an external-renderer handoff. The fictional built-in example is deliberately edited, not neural output. No model is invoked and imported provenance is unverified. [Details](docs/looklock.md).

**Browser Neural Lab:** pinned MIT OpenDLSS-NR browser graph, model metadata inspection, stage SHA-256 checks, dedicated worker, GPU frame preparation/composition, bounded live-frame scheduling and comparison. No approved model is bundled; file selection alone does not enable inference. Independent SDR previews have a 320/512-pixel longest edge, no optical flow/history, and no real-time promise. [Details](docs/neural-lab.md).

## Browser development and extension installation

Requires Node.js22+:

```sh
git clone --recurse-submodules https://github.com/Alex-Unnippillil/shiny-engine.git
cd shiny-engine
npm ci --ignore-scripts
npm run dev
```

Existing checkout:

```sh
git switch main
git pull --ff-only origin main
git submodule update --init -- third_party/opendlss-nr
npm ci --ignore-scripts
npm run build
```

Open `http://127.0.0.1:4173`. Workspaces: `/apps/viewer/index.html`, `/apps/looklock/index.html`, `/apps/nr/index.html`, `/apps/player/index.html` (native setup).

At `chrome://extensions` or `edge://extensions`, enable Developer mode and **Load unpacked** `dist/extension`, not the repository root. Pin the toolbar icon. `npm run package` produces extension/site ZIPs and checksums.

Media Lab accepts PNG/JPEG/WebP/AVIF up to50MiB/33MP and browser-decodable MP4/WebM/Ogg up to2GiB. Spatial output is capped at4096 pixels/axis and about8.3MP. PNG export is a still frame, not a video recorder. LookLock has smaller explicit bounds described in its docs. No file is uploaded or recorded automatically.

Inline mode is limited to readable, unprotected, top-frame HTML5 video with native controls and supported geometry. Universal YouTube/custom-player/iframe support is not claimed. Audio remains with the original player. Restore original or the in-page stop control removes inline processing.

**Share a window** requires browser consent; never select the viewer as its own source. Browser monitor capture is disabled to avoid feedback. The separate tab viewer requires optional permission and does not render its output into the captured tab. Real picker/audio/protected-content behavior remains platform-specific. No persistent all-site host permission is requested.

## Optional Windows capture companion

`native/windows` is a separate unchanged Windows Graphics Capture / D3D11 spatial preview application, not the new libVLC player or a DLSS backend. It provides a system source picker, pause/compare/stop and Ctrl+Shift+F10 emergency stop. It is not a click-through overlay or HDR pipeline.

Requires Windows11x64, Visual Studio C++/WindowsSDK/C++WinRT and CMake3.24+:

```powershell
cmake -S native/windows -B build/native -A x64
cmake --build build/native --config Release --parallel
ctest --test-dir build/native -C Release --output-on-failure
.\build\native\Release\shiny-native.exe
```

Register the unpacked extension's32-character ID:

```powershell
.\native\windows\install.ps1 -ExtensionId 'YOUR_EXTENSION_ID' -Executable '.\build\native\Release\shiny-native.exe'
```

The **shiny-engine-windows-unsigned-alpha** CI artifact supplies a prebuilt companion. Run its adjacent install.ps1 without `-Executable`. Installation writes only per-user LocalAppData/Chrome/Edge native-host entries; no service, autostart or firewall change. Follow local execution policy without globally disabling protections.

Open Media Lab from the extension, connect the companion, grant optional native messaging and select a source. Stop through its controls or Ctrl+Shift+F10. Pause retains capture; Stop releases it. Closing the browser does not stop a separately running preview. Close the companion before uninstall.ps1; uninstall the extension separately. Changed extension IDs need re-registration.

## Validation and release process

```sh
npm run check
python -m pip install playwright==1.63.0
python -m playwright install --with-deps chromium
npm run test:browser
npm run package
```

Browser CI covers strict TypeScript, settings, protocol, media lifecycles, source recipes, real graphics output/PNG exports and actual MV3 loading. Browser/native NR arithmetic tests use **synthetic residuals**, not a trained model. Native player CI compiles pinned VideoLAN/OpenDLSS/Khronos source, decodes generated original AVI/PCM, checks real filter/bypass pixels and model rejection, and runs installed playback/uninstall tests.

A feature merge requires **browser-build**, **windows-build**, and **vlc-player** to pass on the exact head. Main release publication independently verifies the latest required checks on the exact revision before attaching versioned installers, portable/source archives and checksums. A successful build is not a model approval or physical-GPU benchmark. Missing parity/temporal/hardware tests are not counted as passed.

See [support matrix](docs/support-matrix.md), [benchmark method](docs/benchmark-method.md), [security](docs/threat-model.md), [model provenance](docs/model-provenance.md), and [release notes](docs/release-0.6.md). No trained-model execution, vendor equivalence, VSR/GPU speed, HDR, sound-device output, full codec coverage or sustained-session certification is implied by software CI.

## Source layout

```text
apps/viewer             Spatial browser workbench
apps/looklock           Source-preserving still review
apps/nr                 Gated browser neural lab
apps/player             Native installer/setup navigation
apps/extension          MV3 permissions and session routing
packages/contracts      Validated settings/native commands
packages/gpu-web        Spatial renderer and frame scheduler
packages/nr             Browser model policy and graph adapter
packages/looklock       Still composition, recipes and PNG encoder
third_party/opendlss-nr  Pinned MIT graph source; no model weights
native/windows          Separate spatial capture companion
native/vlc-player       Independent Windows libVLC player
native/nr-worker        Native Vulkan graph/process/model policy
installers/windows      Per-user setup and uninstall tests
models                  Metadata, approvals and provenance only
docs                    Research, implementation status and gates
```

Original application: MIT. Preserve [root notices](THIRD_PARTY_NOTICES.md) and [native player notices](native/vlc-player/THIRD_PARTY_NOTICES.md). Shiny Engine is independent of NVIDIA and VideoLAN. Their names identify interoperability, not endorsement.
