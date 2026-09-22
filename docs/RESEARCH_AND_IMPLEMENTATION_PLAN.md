# Desktop and Browser Media Enhancement
## Research findings and implementation plan

**Research date:** 22 September 2026  
**Status:** Proposed architecture and backlog; no application has been built or GPU-benchmarked for this plan.  
**Initial platform assumption:** Windows 11 with Chrome or Edge. The user's actual GPU, driver, monitor configuration and performance envelope have not been established.  
**Scope:** A standalone project, not a change to an existing portfolio repository.

## 1. Decision

Build a browser extension with an optional native Windows companion. Use the extension for media selection, controls and supported browser-local processing. Use the native companion for desktop/window capture, native GPU processing and presentation. This is an architecture recommendation, not a claim that an ordinary extension can replace the Windows compositor. [S10–S16]

For the closest technical recreation of the DLSS 5 neural-rendering effect, evaluate **maanHimself/OpenDLSS-NR's native Vulkan implementation** behind an experimental backend interface. Its repository declares MIT licensing for its code but does not supply or license NVIDIA model weights. Runtime use, model provenance and distribution rights must be independently established before this backend is enabled or shipped. A user supplying a file does not, by itself, resolve those rights. [S02–S03]

For a distributable useful baseline, implement conventional spatial enhancement first and add separately cleared animation/image-restoration models. Label each backend accurately. NVIDIA Image Scaling is spatial scaling/sharpening, not AI or DLSS. Anime-specific enhancement is not a universal photographic model. A Real-ESRGAN-derived image mode is not the same effect as DLSS 5 neural rendering. [S07–S09, S17]

Do not make the first release dependent on leaked DLLs, extracted proprietary model weights, perfect desktop optical flow, universal site support, frame generation, or a browser-only real-time DLSS implementation.

### Decision outcomes

| Goal | Preferred route | Release condition |
|---|---|---|
| Closest DLSS 5 appearance experiment | OpenDLSS-NR native backend in an isolated research module | Lawful model access, required GPU features, parity and end-to-end tests |
| Browser videos and images | MV3 extension, WebGPU pipeline, supported-site adapters | Pixel access, stable playback, model/code notices and browser-store policy |
| Whole Windows desktop/window | Native capture and presenter controlled by extension or tray UI | No feedback, correct input/geometry, stable performance and visible stop control |
| Browser-only demonstration | Separate local-image or scene viewer | Explicitly offline/still-frame where real-time budget is not met |
| Broad GPU compatibility | Capability-selected open spatial/approved model backends | Tested per adapter; not inferred from a brand or a WebGPU badge |

## 2. What the headline combines

NVIDIA's 1 September 2026 research description presents DLSS 5 as a generative rendering stage conditioned on the rendered frame, motion vectors, temporal state and artistic controls. It changes the displayed appearance, rather than simply reconstructing a more expensive conventional render. [S01]

NeuralScreen is a Windows desktop-overlay application, not a browser extension. Its current README discloses a leaked NR runtime and its license limits reuse. Earlier broad RTX compatibility headlines should not replace the project's current support matrix. [S04–S05]

OpenDLSS-NR now publishes both native and browser implementations of the NR network. This is a much more relevant implementation reference than assuming the browser demo is still unpublished. Its NR implementation does not implement DLSS Super Resolution. [S02–S03]

NijiLucid is an actual browser-extension reference for local anime-video enhancement. Its core is MIT, but its default package includes LGPL-3.0-or-later CuNNy-derived components. Review file headers and notices, not only the main project's headline license. [S06]

## 3. Evidence and uncertainty

### Reported performance, not measurements from this project

| Upstream path | Hardware/workload | Reported timing | Important qualification |
|---|---|---:|---|
| OpenDLSS-NR browser port | RTX 4070 SUPER, 512×512 | Approximately 73 ms for a warm frame | Port report; not desktop capture-to-display latency |
| Same port's native comparison | Same card, 512×512 | Approximately 2.7 ms | Network comparison, not complete application |
| OpenDLSS-NR native | RTX 4070 SUPER, 1920×1080 | 7.8 ms | Minimum across 40 frames, not median or p95 |
| OpenDLSS-NR native | RTX 4070 SUPER, 3840×2160 | 29.3 ms | Same minimum-based methodology; overhead excluded |

Sources: [S02–S03]. Do not convert the native figures into promised desktop FPS. Capture, transfers, optical flow, composition, presentation and other workloads still consume time.

The browser port explicitly emulates arithmetic that the native route accelerates, and documents a costly WebGL-to-WebGPU readback in its scene demo. The related WebCuda project also characterizes its current implementation as non-real-time and distinguishes inference completion from original-output parity. These are implementation reports, not proof of an immutable browser hardware ceiling. [S03, S19]

**Not independently established:** numerical parity, benchmark reproducibility, long-session stability, image quality on ordinary videos, protected-media behavior, compatibility on the user's system, store acceptance, or NVIDIA model-use/distribution rights.

## 4. Reference selection

| Reference | Proposed use | Reuse boundary |
|---|---|---|
| maanHimself/OpenDLSS-NR | Optional native NR backend and browser numerical reference | MIT code; NVIDIA weights/IP remain separate |
| perseval-BLR/NeuralScreen | Desktop UX and capture-behavior comparison | Do not copy or modify without the required permission; no bundled leaked runtime |
| chenmozhijin/NijiLucid | Browser discovery, lazy initialization, site support and enhancement controls | Component-level MIT/LGPL review; not a blanket permissive fork |
| Anime4KWebBoost/Anime4K-WebGPU | Animation-processing candidate and texture-pipeline reference | Audit actual chosen files, generated shaders and model provenance |
| NVIDIAGameWorks/NVIDIAImageScaling | Fast spatial scaling/sharpening baseline | MIT notices; port to WGSL requires implementation and validation |
| xinntao/Real-ESRGAN | Image-restoration candidate; not a real-time guarantee | BSD-3-Clause code; independently clear exact weights and dependencies |
| Microsoft ONNX Runtime Web | Approved ONNX-model execution in the browser | Check operators, conversion parity, resource ownership and GPU placement |
| Blinue/Magpie | Native window-scaling comparison | GPLv3; do not silently import GPL code into a differently licensed application |

Sources: [S02–S09, S17, S20]. The choice is not to combine all repositories. Start with the smallest cleared dependency set.

The inspected NijiLucid `src/content.ts` supports a useful design pattern: lightweight video discovery, deferred loading of a heavier manager, settings-driven activation and explicit disposal. This is a source inspection, not a full security or correctness audit. [S21]

## 5. System architecture

```text
Browser extension UI / toolbar / keyboard shortcut
  |
  +-- Browser-local session
  |     readable video/image -> GPU texture -> selected WebGPU backend
  |                           -> canvas or separate comparison viewer
  |     original player remains owner of playback and inline audio
  |
  +-- Native session, explicitly enabled
        typed native-messaging commands (settings and session IDs only)
          -> native host / source picker
          -> Windows Graphics Capture
          -> GPU texture interoperability layer
          -> selected native backend
          -> masked composition / native presenter
```

### Browser process responsibilities

The service worker owns permissions, session routing and lifecycle, not continuous frame rendering. A content script manages supported inline media; a visible extension viewer or justified offscreen document manages capture/viewer sessions. Worker placement is selected only after verifying actual GPU/API support in that context. [S10–S12]

Never repeatedly read full frames into JavaScript pixel arrays as the production real-time path. Import accessible video frames into WebGPU, use shader conversion into the selected backend's tensor/texture format, and retain intermediate resources. ONNX Runtime's WebGPU I/O binding is relevant, but it does not make arbitrary texture-to-model pipelines automatically zero-copy. [S09, S13]

### Native process responsibilities

Use C++20 and C++/WinRT for a Windows capture/presentation core, with Vulkan behind the optional OpenDLSS adapter. Windows Graphics Capture supplies Direct3D surfaces. D3D-to-Vulkan sharing, synchronization, adapter matching and presentation must be proven in a separate interoperability spike; the upstream inference CLI is not assumed to provide this complete pipeline. [S02, S14]

Start with a user-selected single window. Add monitor capture only after the single-window path is stable. Implement source close, minimize, resize, display rotation, DPI changes, sleep/resume and device loss as explicit lifecycle events.

A native overlay must preserve clicks and keyboard focus without synthesizing input into arbitrary applications. Follow source visibility and stacking, and remove the enhancement immediately when the session becomes invalid. Keep an emergency stop shortcut independent of the overlay's UI.

### Native messaging is a control channel

Chrome native messaging transports framed JSON; native-to-extension messages are limited to 1 MB. Do not move video frames or base64 images through it. Send typed commands, settings, opaque source/session IDs and compact metrics. Keep the video pipeline inside the native process. [S10]

Proposed v1 command vocabulary:

```text
hello, getCapabilities, selectSource, startSession,
updateSettings, pauseSession, resumeSession, stopSession, getMetrics
```

Do not expose arbitrary command execution, arbitrary DLL loading, arbitrary filesystem paths, arbitrary HWND selection from page input, or general-purpose network fetching.

## 6. Capture and compatibility decision tree

1. **Readable media pixels:** use inline WebGPU enhancement on a supported player. Preserve its controls, captions, seek behavior and audio.
2. **Direct access unavailable:** offer an explicit tab-capture session in a separate extension viewer, or a native window session. Never silently start screen capture.
3. **Protected/unavailable content:** show a clear unsupported reason and leave the source untouched. No DRM or capture-protection circumvention.
4. **GPU/backend unavailable:** offer a verified lighter backend or the original image. Never display an enabled state while doing nothing.

Cross-origin host permissions can authorize extension-origin network requests. They do not remove the same-origin restrictions of content scripts or automatically make an existing cross-origin video readable as a GPU texture. `importExternalTexture` can reject cross-origin video with `SecurityError`. [S12–S13]

A whole-tab capture includes what is rendered in that tab. Do not display its processed result back into the captured tab; that risks recursive feedback. A separate viewer is the browser fallback, not an invisible replacement for the desktop. [S11]

For a native full-display overlay, use and test capture exclusion for the application's own output windows. `WDA_EXCLUDEFROMCAPTURE` is an appropriate Windows mechanism, but it is not a DRM/security guarantee. Validate feedback avoidance on each supported capture path. [S15]

Inline audio should remain owned by the original player. Tab capture has different audio behavior and may require explicit reconnection to the output device. Ensure exactly one audible path, and measure added A/V skew. [S11]

### Geometry and player integration

Do not assume CSS pixels equal physical screen pixels. Test browser zoom, per-monitor scaling, browser chrome, iframe coordinates, fullscreen transitions and mixed-DPI displays. Automatic media-region handoff to a native overlay is a later feature; begin with the system picker and an explicit user-selected region.

Place enhancement only over media pixels. Use a non-intercepting canvas and preserve player controls/captions above it where the adapter supports this. Burned-in subtitles cannot be separated simply by changing DOM stacking. Unsupported players fall back rather than receiving a fragile generic overlay.

## 7. Backend contract and product modes

Expose a common lifecycle but keep browser and native GPU objects local to their own processes:

```text
probe -> prepare -> process -> resetHistory -> release
```

Record backend ID/version, model hash, supported formats, geometry constraints, history requirements and approved use/distribution status. Never identify success solely by whether a model produced a nonconstant image.

### Modes

**Original:** unmodified playback; always available.

**Balanced enhancement:** restrained spatial processing or an approved restoration model. Do not describe this as recovered ground-truth detail or as DLSS.

**Animation:** explicitly selected animation-tuned processing. Do not automatically use it on faces, photographs or general desktop text.

**Photo lab:** user-selected still images, tiled model execution, before/after comparison, cancellation and explicit export. Keep face beautification off. Browser WebGPU NR can be a separate experimental backend only after its weight-use gate is satisfied.

**Experimental neural rendering:** optional native NR backend; off by default and clearly described as generative appearance modification. Availability depends on rights, hardware and tests. Do not make it the automatic enhancer for documents, text or sensitive visual information.

### Temporal behavior

A desktop capture supplies pixels, not the game-engine motion-vector buffer described by NVIDIA. Start NR evaluation with stills and a history-reset path. For moving content, separately evaluate optical-flow estimation, disocclusion masks and scene-cut handling before enabling temporal feedback. Estimated screen motion is not equivalent to engine motion. [S01, S03]

Reset history on seek, source replacement, scene cut, resolution change and invalid motion. Measure flicker, ghosting and fine-detail instability. Do not hide problems by applying a generic blur to all frames.

## 8. Scheduling, memory and color

Use source-driven frame callbacks in the browser and frame-arrival events natively. Keep at most one processing frame and one replaceable pending frame. When overloaded, discard obsolete pending work or reduce quality; do not grow latency through an unbounded queue.

Do not reuse a GPU surface while an asynchronous consumer still owns it. Explicitly release VideoFrames, tensors, textures, buffers, native capture frames and pipelines at the right lifecycle boundary. ONNX GPU buffers have ownership rules; native capture-frame surfaces also have lifetimes that must be respected. [S09, S14]

Use actual capability queries rather than hard-coding all devices of a vendor as supported. Timestamp queries are optional; distinguish measured GPU time from CPU wall-clock time.

Ship SDR first. Define source transfer/range and output color space, and use the model's documented preprocessing rather than arbitrary normalization. Handle image orientation and alpha correctly. HDR support is a separate milestone: Windows capture documentation warns about clipping when an HDR pipeline is treated as ordinary 8-bit SDR. [S14]

## 9. Privacy and supply-chain requirements

Processing is local by default. Do not record, upload, persist or log screen frames by default. Do not send browsing URLs or media filenames in ordinary telemetry. Capture begins with a user action and has a persistent visible stop mechanism.

Use least-privilege browser permissions. Request site/capture/native capabilities only for the corresponding feature, subject to the supported permission model. Route content messages through strict schemas, validate sender context, bind commands to an authorized session and allowlist native host origins. [S10–S12]

Bundle executable extension assets, including JavaScript and WASM. Bundle shader code as a conservative packaging policy. Separate any approved model-data distribution from executable-code loading and confirm the actual store treatment before release; a remote URL must not become an arbitrary code-loading mechanism. [S16]

Pin dependencies and upstream commits. Maintain an SBOM, copyright notices, checksums and a provenance record for each model. Review downloaded build scripts before executing them. Keep upstream research code and production application code separately identifiable.

Do not package leaked NVIDIA runtimes or model data. Treat lawful access, runtime use, redistribution, model conversion and derivative packaging as separate questions. Obtain appropriate review before distributing a backend that depends on proprietary assets. NeuralScreen's license and OpenDLSS-NR's exclusion of NVIDIA rights make this a real release gate, not a documentation formality. [S02, S04–S05]

NVIDIA's current AI for Media page is an additional official video-enhancement route to evaluate. Its SDKs/services are not assumed to be a free, unrestricted drop-in backend; access and the applicable license must be checked for the selected offering. [S18]

## 10. Implementation milestones

### M0 — Feasibility and provenance

Record exact upstream commit IDs and notices. Inventory GPU features and actual display/browser configuration. Build a supported local-media fixture suite. Compare original playback, a cheap spatial shader and an approved neural backend. Attempt NR parity only when lawful weights and reference fixtures exist.

**Exit:** published local benchmark report with timing scope, hardware, software versions and known limitations; approved dependency/model register; explicit choice of supported first backend. No unsupported performance claims.

### M1 — Capture-to-presentation without neural rendering

Implement a minimal MV3 extension, local image/video viewer and supported inline passthrough canvas. In parallel, build a native single-window capture/presenter with bypass mode and emergency stop. Prove D3D/Vulkan interoperability independently where needed.

**Exit:** playback, input, source changes, resize and cleanup work; no capture feedback; no concealed CPU transfer cost; enable/disable restores the original presentation.

### M2 — Browser enhancement

Add a spatial backend and one cleared animation pipeline. Implement capability detection, adaptive frame scheduling, before/after controls and per-site settings. Add approved still-image restoration with tiling and cancellation separately from the real-time path.

**Exit:** representative supported pages remain usable; frame pacing and resource budgets are met on named hardware; unsupported media fails safely.

### M3 — Native enhancement and extension control

Add typed native messaging and local source selection. Connect the native spatial backend, then the selected approved neural backend. Integrate window following, visibility, DPI, device-loss recovery and explicit model/backend status.

**Exit:** one complete source-selection-to-enhanced-display session works without browser frame transport, hidden capture or stuck overlays.

### M4 — Optional NR and temporal research

Only after provenance approval, adapt the OpenDLSS native interface. Establish still-frame parity with available independent fixtures, then evaluate video with explicit motion/history semantics. Add a per-region composition policy and separate model-compute, flow and presentation timing.

**Exit:** reproducible correctness evidence and a video-quality evaluation. A missing parity fixture is reported as unverified, not passed. Failure does not block the useful non-NR product.

### M5 — Whole-display and release hardening

Add full-display mode with tested capture exclusion, multi-monitor handling and safe recovery. Audit store permissions, native installer/update safety, notices and privacy documentation. Gate GPU support by actual evidence. HDR, frame generation and more operating systems remain later independent features.

**Exit:** clean install/uninstall, signed release process where applicable, regression reports, supported-platform matrix and no ambiguous backend claims.

## 11. Proposed quality gates — not measured results

| Area | Proposed acceptance gate |
|---|---|
| First real-time workload | 1280×720 input to 1920×1080 display at 30 fps, on a named baseline device |
| Processing budget | p95 at or below 25 ms for the selected 30-fps preset; otherwise downgrade or retain original |
| Optional 60-fps preset | p95 processing at or below 12 ms on a specifically certified device/preset |
| Native media latency | Target p95 capture-to-present below 50 ms; record actual result, including mode and hardware |
| Playback stability | Under 1% missed processing deadlines in a 30-minute controlled test at the advertised preset |
| A/V | Measure added skew; target at most 20 ms in controlled viewer tests, otherwise reduce quality or constrain support |
| Lifecycle | 100 enable/disable/source-change cycles without monotonically growing retained GPU allocations |
| Fidelity | Before/after review on faces, text, line art, motion, dark scenes and compression; generative modes labeled |
| Recovery | Device loss, source closure, consent denial and host crash restore usable original content |
| Privacy | No frame uploads or recording without separately enabled user consent |

The 25-ms/12-ms figures are engineering targets inside 33.3-ms/16.7-ms frame intervals, not assurances about a particular GPU or neural model. Capturing at 30 fps is not acceptable for an unrestricted desktop-interaction claim; begin with media/window use and qualify whole-desktop mode separately.

Benchmark reports must separate capture, conversion, interop, model, flow, composition, presentation, cold initialization and end-to-end latency. Include median/p95/p99, drops, memory and sustained-load behavior. Retain unprocessed baselines. Do not compare one backend's minimum GPU time with another's full wall-clock time as if they were equivalent.

## 12. Proposed repository layout

```text
desktop-media-enhancer/
  apps/
    extension/                 # MV3 UI, permissions, content adapters
    viewer/                    # Local image/video comparison and benchmark UI
  native/
    windows/                   # Capture, presenter, tray, native messaging
    backends/
      spatial/                 # Baseline shader implementation
      nr-experimental/         # Separately gated native NR adapter
  packages/
    contracts/                 # Versioned messages, settings and model metadata
    gpu-web/                   # WebGPU resources, schedulers and backends
    media-adapters/            # Supported-site integration
  models/
    registry.json              # Metadata/provenance only; no proprietary weights
  tests/
    unit/
    browser/
    native/
    gpu/
    fixtures/                  # Synthetic or explicitly licensed media
  docs/
    architecture.md
    model-provenance.md
    threat-model.md
    support-matrix.md
    benchmark-method.md
  .github/
    workflows/
    ISSUE_TEMPLATE/
```

This is a proposed new repository. No GitHub repository, issue, pull request or deployment was created during research.

## 13. Work allocation and integration

Use one integration owner for contracts, dependency pins, root configuration and releases. Parallelize only after the message/backend interfaces are agreed.

| Track | Exclusive initial ownership | Dependencies |
|---|---|---|
| Browser shell | apps/extension, packages/media-adapters | Contracts |
| WebGPU processing | packages/gpu-web | Frame/backend contract |
| Native capture/presentation | native/windows | Session contract |
| NR research | native/backends/nr-experimental | Provenance approval and native interop |
| QA | tests, benchmark fixtures and reports | Agreed observable contracts |

Each track produces small reviewable PRs. Shared contract changes go through the integration owner before consumers change. Do not let parallel agents independently upgrade toolchains, modify the same manifest or rewrite shared types.

Ordinary CI covers types, lint, unit tests, protocol validation and deterministic non-GPU tests. GPU correctness and Windows capture need dedicated hardware sessions. Trusted GPU runners must not execute arbitrary untrusted fork code with credentials or access to personal desktop data. An application build passing is not a GPU performance test.

## 14. Deferred scope

Frame interpolation, automatic HDR conversion, all-site compatibility, macOS/Linux native capture, multi-GPU acceleration, mobile system-wide enhancement and neural processing of every desktop pixel are not MVP commitments. Add each only with a dedicated design and benchmark. A higher displayed frame rate is not proof of lower input latency.

Do not train a new DLSS-sized model merely to avoid the first integration decisions. A independently trained model would require data rights, temporal supervision, quality evaluation and substantial experimentation, and would not automatically reproduce NVIDIA's trained model. An open restoration backend is the practical fallback, explicitly distinguished from NR.

## 15. Source register

All sources below were accessed during the 22 September 2026 research session. Repository default branches can change; pin exact revisions in M0.

- **S01 — NVIDIA research, published 1 September 2026:** `https://research.nvidia.com/labs/adlr/DLSS5/`
- **S02 — OpenDLSS-NR native README and source:** `https://github.com/maanHimself/OpenDLSS-NR`
- **S03 — OpenDLSS-NR browser-port technical README:** `https://github.com/maanHimself/OpenDLSS-NR/blob/main/ports/browser-webgpu/README.md`
- **S04 — NeuralScreen current README:** `https://github.com/perseval-BLR/NeuralScreen/blob/main/README.md`
- **S05 — NeuralScreen license:** `https://github.com/perseval-BLR/NeuralScreen/blob/main/LICENSE`
- **S06 — NijiLucid README and component-license notice:** `https://github.com/chenmozhijin/NijiLucid` and `https://github.com/chenmozhijin/NijiLucid/blob/main/LICENSE`
- **S07 — Anime4K-WebGPU:** `https://github.com/Anime4KWebBoost/Anime4K-WebGPU`
- **S08 — NVIDIA Image Scaling:** `https://github.com/NVIDIAGameWorks/NVIDIAImageScaling`
- **S09 — ONNX Runtime WebGPU:** `https://onnxruntime.ai/docs/tutorials/web/ep-webgpu.html`
- **S10 — Chrome Native Messaging:** `https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging`
- **S11 — Chrome tabCapture:** `https://developer.chrome.com/docs/extensions/reference/api/tabCapture`
- **S12 — Chrome cross-origin requests:** `https://developer.chrome.com/docs/extensions/develop/concepts/network-requests`
- **S13 — MDN external video textures:** `https://developer.mozilla.org/en-US/docs/Web/API/GPUDevice/importExternalTexture`
- **S14 — Microsoft Windows screen capture:** `https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture`
- **S15 — Microsoft capture exclusion:** `https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity`
- **S16 — Chrome remotely hosted code policy:** `https://developer.chrome.com/docs/extensions/develop/migrate/remote-hosted-code`
- **S17 — Real-ESRGAN:** `https://github.com/xinntao/Real-ESRGAN`
- **S18 — NVIDIA AI for Media:** `https://developer.nvidia.com/topics/ai/generative-ai/ai-for-media`
- **S19 — OpenDLSS-NR-WebCuda:** `https://github.com/SamG-Coder/OpenDLSS-NR-WebCuda`
- **S20 — Magpie:** `https://github.com/Blinue/Magpie`
- **S21 — Inspected NijiLucid content bootstrap:** `https://github.com/chenmozhijin/NijiLucid/blob/main/src/content.ts`
- **S22 — Original browser demo:** `https://dlss5-webgpu.dlss5-webgpu-standalone.workers.dev/` (page structure inspected; inference not executed)
