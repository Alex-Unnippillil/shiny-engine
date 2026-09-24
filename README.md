# Shiny Player

## 0.8 development: local enhancement library management

Windows source builds now include **Tools > Enhancement libraries** and an independent managed spatial preview. Two first-party reference DLL versions can be imported, staged, selected and rolled back through a verified worker. These are conventional filters, **not DLSS**; NVIDIA candidates remain quarantined. See [the guide](docs/library-manager-guide.md) and [the implementation/remaining gates](docs/library-manager-status.md). Existing 0.7 download links below identify that historical release; 0.8 artifacts are published only after exact-main gates pass.


### Local video playback. Explicit neural experiments. Your original stays available.

[![Windows player](https://github.com/Alex-Unnippillil/shiny-engine/actions/workflows/vlc-player.yml/badge.svg?branch=main)](https://github.com/Alex-Unnippillil/shiny-engine/actions/workflows/vlc-player.yml)
[![Browser checks](https://github.com/Alex-Unnippillil/shiny-engine/actions/workflows/web.yml/badge.svg?branch=main)](https://github.com/Alex-Unnippillil/shiny-engine/actions/workflows/web.yml)

**[Windows x64 installer](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/ShinyPlayer-0.7.0-Windows-x64-Setup.exe)** · **[Portable ZIP](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/ShinyPlayer-0.7.0-Windows-x64-Portable.zip)** · [Release, checksums and reports](https://github.com/Alex-Unnippillil/shiny-engine/releases/tag/v0.7.0) · [Research guide](docs/research-mode.md)

Shiny Player is an independent Windows application powered by **real VideoLAN libVLC**, with a separate native **OpenDLSS-NR research studio**. Use it for ordinary playback, detailed clip review, or authorized local-model experiments with adjustable tone, structure and mix. Browser workspaces provide spatial enhancement and source-preserving still-frame review.

> **Release status: 0.7.0 prerelease, unsigned.** Ordinary playback, model-data validation, UI lifecycle and installer tests are gated in CI. Trained-model inference, NVIDIA parity, temporal quality and physical-GPU speed are not certified. A model is not included. This is not an NVIDIA-supported DLSS product or complete VLC interface parity.

![Shiny Player 0.7 Windows interface](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/player-desktop.png)
*Actual Windows test capture. The colored video is an original generated decoding fixture, not a DLSS demonstration.*

## What is useful

| Task | Tools |
|---|---|
| Watch local videos or explicit streams | VLC decoding, drag-and-drop, queues, repeat/shuffle, rate, volume and audio-device selection |
| Review a recording | Chapters, frame stepping, A–B loops, bookmarks, jump-to-time and displayed-frame snapshots |
| Work with subtitles and sound | Track selection, external subtitle files, timing offsets and equalizer presets |
| Reduce interface clutter | DPI-aware controls, reflowing toolbars, Cinema view and always-on-top |
| Test an authorized native NR model | Validated model intake, session acknowledgment, tone/structure/mix sliders, matched-input wipe/side-by-side, PNG and JSON reports |
| Compare output from another renderer | Separate original/processed video playback with source audio only; best-effort, not frame-exact sync |
| Preserve a score or caption in a still | LookLock: restore original pixels in selected regions and export hash-bound recipes |

Advanced disc menus, capture, conversion, casting and other VLC-specific controls not reproduced here remain available through **Full VLC**, which opens the separately installed original application and pauses Shiny playback.

## Install and start

1. Run the Windows installer, or extract the portable package.
2. Select the optional **official VLC runtime download** in setup, or use an existing official 64-bit VLC **3.0.24+ in the 3.0 series**. Use **Media → Locate installed VLC** for a nonstandard directory. VLC 4 has a different ABI and is not supported.
3. Launch `ShinyVlcPlayer.exe`, then open or drop a file.

Setup installs per user with shortcuts and managed uninstall; no elevated service, startup task or system VLC modification. The optional VLC archive is fetched from VideoLAN and SHA-256 checked, not bundled/rehosted in our package. Offline setup requires an existing compatible VLC or the matching cache:

```powershell
.\ShinyPlayer-0.7.0-Windows-x64-Setup.exe /TASKS=vlcruntime /VLCARCHIVE="C:\Downloads\vlc-3.0.24-win64.zip"
```

The setup and executable are **unsigned**. Check release checksums and your organization's execution policy. No protection bypass is required or recommended. The browser extension is optional and does not install or launch native code.

## OpenDLSS-NR local research

Version 0.7 no longer requires every local experiment to appear in an empty curated model register. The separate research route requires deliberate user action and validates data before calling the actual native graph:

**Open local video → DLSS-NR research → Local research → Inspect model folder → acknowledge authorized use → Prepare model.**

Tone and structure feed graph preprocessing; neural mix blends the returned result with the same original frame. Pause the independent preview to compare settings. Old results are discarded after control changes. Save an experimental PNG or a JSON report explicitly. Stop cancels the worker; close also releases the secondary decoder. Primary playback stays available.

![Native local-model research studio](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/player-research-mode.png)
*Actual interface before a model is loaded. Disabled preparation/export is intentional: no fabricated neural result is shown.*

The model must use the pinned native 71-block manifest/stage format. Arbitrary ONNX files, checkpoints and DLLs are not supported. Data validation covers JSON structure, required tensor byte layouts, stage hashes, paths, byte budgets and replacement-resistant file handles. Acknowledgment is bound to the inspected manifest. **It is not legal verification, production approval or evidence that output is correct.** The reviewed-only native path and browser approval register remain unchanged.

[Detailed model format, steps and limits](docs/research-mode.md) · [Native data flow](docs/native-neural.md)

### Enhancement types are not interchangeable

| Mode | What it actually does |
|---|---|
| VLC image adjustments | Conventional contrast/brightness/saturation/gamma filters; not AI |
| Driver super resolution | Requests VLC's D3D11 scaler, potentially RTX VSR on supported NVIDIA hardware; a request is not proof of activation |
| OpenDLSS-NR research | Actual native graph integration with user-supplied data; independent SDR appearance experiments, not certified DLSS or an upscaler |
| Imported comparison | Plays a video enhanced elsewhere; it does not run inference |

No NVIDIA runtime, model weights, extraction scripts, caller-validation bridges or DRM bypasses are distributed.

## Requirements and operating envelope

| Item | Supported scope |
|---|---|
| Native package | Windows x64; Windows 11 recommended. Installer OS floor: Windows 10 build 17763. Windows CI is not broad end-user hardware certification |
| Playback engine | Official x64 libVLC 3.0.24+ in the 3.0 series; available formats depend on the installed modules and input |
| Neural runtime | Packaged `nr/ShinyNrWorker.exe`, shaders/PTX, supported GPU drivers and authorized compatible model data |
| Neural hardware | Exact Vulkan features required by the pinned graph; use **Check GPU**. Vendor/model names alone are insufficient |
| Neural input | Opaque SDR RGBA; dimensions 33–512 pixels, maximum longest edge 512 |
| Model bounds | 2 MiB manifest; 256 stages; 1,024 tensors; 256 MiB aggregate stage data and tensor-allocation budgets |
| Scheduling | One processing frame and one replaceable pending frame; explicit CPU transfers |
| Audio/temporal | Main playback and neural preview are separate. No neural audio synchronization, motion-vector history or frame generation |
| Other operating systems | No native macOS/Linux installers; portable C++ tests are not a platform port |

No 4K/HDR neural output, live desktop overlay, video-wide enhancement export or real-time frame-rate promise is made.

![Compact Windows layout](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/player-compact.png)
*Actual compact-window test; primary controls remain available when the sidebar is hidden.*

![Cinema mode](https://github.com/Alex-Unnippillil/shiny-engine/releases/download/v0.7.0/player-cinema.png)

## Keyboard

**Space** play/pause; **S** stop; **E** frame step; **←/→** seek; **Shift+←/→** larger seek; **N/P** next/previous; **M** mute; **F** fullscreen; **C** Cinema; **B** bookmark; **J** time; **[ / ]** A–B; **Ctrl+O** files; **Ctrl+N** network. Focused controls retain ordinary keyboard behavior. **Escape** closes the research studio or leaves fullscreen/stops primary playback.

## Build and test

Windows requires Visual Studio C++ and Windows SDK, CMake 3.24+, Python 3, Git and Inno Setup 6. From the repository root:

```powershell
powershell -File scripts/Build-Windows.ps1
```

This builds the player, native graph worker, kernels, portable ZIP and installer using immutable upstream pins and archive hashes. The player-only CMake target does not build its worker. Detailed equivalent steps are in [.github/workflows/vlc-player.yml](.github/workflows/vlc-player.yml).

Browser workspaces require Node.js 22+:

```sh
git clone --recurse-submodules https://github.com/Alex-Unnippillil/shiny-engine.git
cd shiny-engine
npm ci --ignore-scripts
npm run check
python -m pip install playwright==1.63.0
python -m playwright install --with-deps chromium
npm run test:browser
npm run dev
```

Open `http://127.0.0.1:4173`. For an existing checkout, run `git submodule update --init -- third_party/opendlss-nr`. Build the unpacked Chrome/Edge extension from `dist/extension`, not the repository root. Browser Neural Lab remains reviewed-only.

## Verification and release policy

Publishing requires the **exact current main commit** to pass `browser-build`, `windows-build` and `vlc-player`. The workflow binds the newest successful runs to their checks, verifies GitHub’s archive digest, build provenance and complete package checksums, stages a draft release, and compares remote asset digests before publication. Stale builds and altered or incomplete releases are rejected; published versions are never overwritten. Reports and the actual screenshots accompany the release.

Tests cover real libVLC software decoding, filters/bypass, subtitles, pause/resume/seek, secondary decoder cleanup, model rejection, synthetic metadata/intake, responsive layouts, browser/extension routing, and Windows install/playback/uninstall. Generated zero tensors and synthetic residuals are **not trained-model tests**. Separate authorized trained fixtures, quality review, named-hardware measurements, sustained A/V testing and signing remain necessary for stronger production claims. See [release integrity](docs/release-integrity.md), [implementation status](docs/implementation-status.md) and the [unchanged original plan](docs/RESEARCH_AND_IMPLEMENTATION_PLAN.md).

## Privacy and source

No media uploads, telemetry, persistent viewing history or automatic model downloads. Explicit streams and network shares contact their chosen endpoints. Exported playlists contain file paths/URLs; reports omit these values but may include a model fingerprint. Keep original material; generative experiments are not evidentiary restoration.

| Directory | Role |
|---|---|
| `native/vlc-player` | Native UI and libVLC playback |
| `native/nr-worker` | Native graph integration, private protocol and validated model intake |
| `apps/player` | Script-free installation/research guide |
| `apps/viewer`, `apps/nr`, `apps/looklock` | Browser media, gated neural lab and still-frame review |
| `installers/windows` | Per-user installer and smoke tests |
| `tests` | Browser, C++, Windows UI and release verification |

Original integration: MIT. Pinned OpenDLSS, volk, Vulkan and VideoLAN notices remain applicable; model and runtime rights are separate. [Third-party provenance](native/vlc-player/THIRD_PARTY_NOTICES.md). This project is not affiliated with or endorsed by NVIDIA or VideoLAN.
