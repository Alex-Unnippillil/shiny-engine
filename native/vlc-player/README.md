# Shiny Player 0.7.0 — Windows x64

A local VideoLAN libVLC player with an opt-in OpenDLSS-NR research studio. The player works independently of any model. Neural experiments use local data you are authorized to use; they are not certified NVIDIA DLSS output.

## Install

Run `ShinyPlayer-0.7.0-Windows-x64-Setup.exe` for per-user installation, shortcuts and uninstall, or extract `ShinyPlayer-0.7.0-Windows-x64-Portable.zip`. No administrator rights are required. These packages are unsigned.

Setup can optionally download the hash-verified official VLC 3.0.24 x64 runtime from VideoLAN into the application's private directory. Otherwise use an existing official 64-bit VLC 3.0.24+ in the 3.0 series. Use **Media > Locate installed VLC** when it is not in a standard location. VLC 4 is a different ABI and is not supported.

For offline setup, supply an existing runtime or a matching cached archive using `/TASKS=vlcruntime /VLCARCHIVE="C:\path\vlc-3.0.24-win64.zip"`. The installer does not bundle VLC, NVIDIA binaries or models. Windows x64 is the only native build target; no macOS/Linux installers are claimed.

## Native research

Open a local video, select **DLSS-NR research**, choose **Local research**, inspect a compatible model folder, acknowledge authorized use and prepare. Tone, structure and mix sliders, paused-input comparison, wipe/original/output views, PNG export and experiment reports are provided. Preparation requires valid files and explicit session intent; local data is not silently production-approved.

See `research-mode.md` included with this package. Actual trained-model inference and GPU performance remain unverified by ordinary CI. The preview is independent SDR, up to 512 pixels per longest edge, with CPU transfers and no main-audio synchronization or temporal reconstruction.

## Playback

Queues/reordering/repeat/shuffle; seek/speed/A-B loops/bookmarks; jump-to-time; audio output/track/equalizer; subtitles/timing; chapters/frame-step; aspect/crop/deinterlace; fullscreen/cinema/always-on-top; snapshots. The DPI-aware layout reflows smaller windows and keeps queue selection available in Playback. **Full VLC** opens the original separate VLC interface for advanced features not duplicated here. Codec support depends on installed VLC modules and the input.

Conventional VLC image adjustments are not AI. The optional driver-super-resolution request uses VLC's D3D11 scaler and may invoke RTX VSR on supported hardware; it is not DLSS 5 and a request is not proof of execution. Imported comparison simply plays externally processed video.

## Keys and privacy

Space play/pause; S stop; E frame; arrows seek; F fullscreen; C cinema; B bookmark; J time; N/P queue; M mute; brackets A/B. Escape closes the research window or leaves fullscreen/stops primary playback. Focused controls retain ordinary keyboard behavior.

No automatic uploads, media history, telemetry or model download. Explicit streams and file shares contact their endpoints. Exported playlists contain selected paths/URLs; diagnostics omit them. Retain source material when experimenting.

## Build

From the repository root, `powershell -File scripts/Build-Windows.ps1`. Requires Windows x64, Visual Studio C++/SDK, CMake, Python3, Git and Inno Setup6. The player-only CMake build does not build its separate worker; the script/CI builds all components. See THIRD_PARTY_NOTICES.md for immutable source pins and rights.

## Enhancement Libraries (0.8)

Tools > Enhancement libraries opens the native local package manager. Import bundled, verify, stage and select a reference version; then use Tools > Managed spatial preview. These first-party DLLs are conventional filters, not DLSS. See `library-manager-guide.md` in the installed package for independent-preview behavior, store recovery, cancellation and preserved original playback.
