# Shiny Player 0.6.0 — Windows x64

Independent player using the real VideoLAN libVLC engine. **The native OpenDLSS-NR graph is compiled and included, but no trained model is approved or bundled. This is not activated DLSS 5 or a complete clone of VLC's interface.** No VLC/NVIDIA runtime binaries or model weights are bundled.

## Windows setup and portable builds

Use **ShinyPlayer-0.6.0-Windows-x64-Setup.exe** for per-user installation, Start menu shortcuts and uninstall. Its optional checkbox downloads the hash-verified official VLC runtime from VideoLAN into this app's own runtime folder. It does not modify a system VLC installation. For offline setup, use an existing compatible VLC or supply the matching cached archive with `/TASKS=vlcruntime /VLCARCHIVE="C:\path\vlc-3.0.24-win64.zip"`. The installer includes the independent application and native worker, not VLC or NVIDIA runtime data. It is unsigned.

Alternatively extract the portable ZIP and install official **64-bit VLC 3.0.24 or newer in the 3.0 series**. Run `ShinyVlcPlayer.exe`. The app-private runtime and standard `Program Files/VideoLAN/VLC` installation are detected; otherwise select **Media > Locate installed VLC**. Selecting a directory authorizes executing its native libraries/plugins. Use a trusted official installation. VLC 4 has a different ABI and is not accepted.

Download the setup, portable package, source archives, checksums and reports from the repository's **v0.6.0 release**. Windows x64 is the only native installer currently provided. macOS/Linux installers are not implemented. The browser extension's setup page does not install or launch native code.

## Native Neural Workbench

Open a seekable local video and choose **Neural workbench**. It has an independent muted VLC decoder, actual native Vulkan feature probe, local model fingerprinting, a private OpenDLSS-NR worker and matched-frame output controls. Its **Prepare** button remains disabled with the empty reviewed-model register in this distribution. Selecting a folder does not grant model approval.

See the included `native-neural.md`. A reviewed build's integration processes independent 33–512-pixel SDR frames with explicit CPU upload/readback. It has no temporal history, audio synchronization, 4K, vendor parity or real-time certification. The panel can be closed independently; stopping primary playback also closes it. The panel's Stop button cancels checks/inference but retains its original preview. Close the panel to dispose its decoder too.

## Playback

Open/drop files or explicitly enter HTTP, HTTPS, RTSP, RTP, UDP or SRT media URLs. Controls include queues, repeat/shuffle, seeking, A-B loops, speed, audio/subtitle track selection, external subtitles, timing offsets, equalizer presets, aspect/crop, deinterlace, chapters, frame step, fullscreen and PNG snapshots, subject to input/runtime support. Installed VLC plugins determine format support; no universal codec, disc, device or URL claim.

**Cinema view** hides the queue/adjustment sidebar. **Always-on-top** is in Video. **J** jumps to seconds or HH:MM:SS. **B** adds an in-memory source bookmark; changing source clears bookmarks. Playback lists them. Media reorders selected queue items. Audio enumerates active output devices. Tools shows media information without paths or credentials.

**Video > Request driver super resolution on next open** passes VLC's D3D11 `super` option on the next media open. Its NVIDIA path requests RTX Video Super Resolution on suitable hardware. This is NOT DLSS 5. The app reports a request, not verified driver execution or improved image quality. Disable and reopen media to return to automatic output. It is off by default.

The four sliders use VLC's conventional contrast, brightness, saturation and gamma filters. They are not AI. Disable adjustments to bypass. HDR/filter compatibility is not certified; tests use SDR fixtures.

**Compare video** plays an externally processed file beside the original with source audio only. Confirm matching timeline and crop. Seeking is best-effort, not frame-exact, and duration mismatches over 1.5 seconds are rejected. This does not generate neural output or verify provenance. Source adjustments are disabled during comparison. Snapshots are displayed VLC frames and may include filters/subtitles.

**Full VLC** opens the installed original VLC interface for advanced conversion, capture, disc menus, casting and features not duplicated here. Shiny playback is paused to prevent duplicate audio. These features run separately, not inside this custom interface.

## Keyboard

Space play/pause; S stop; E frame step; Left/Right seek 5 seconds; Shift+Left/Right 30 seconds; N/P next/previous; M mute; F fullscreen; C cinema; B bookmark; J go to time; [ and ] A/B; Escape leave fullscreen or stop; Ctrl+O files; Ctrl+N network. Focused buttons and sliders retain their normal keyboard behavior.

## Privacy and limits

No history, cloud processing or telemetry. Explicit network media contacts its endpoint through VLC; local subtitle/playlist references may also be resolved by VLC. Exported M3U8 playlists contain paths/URLs and may be private. Diagnostics omit those values. Native worker commands are private bounded frame messages, not a browser shell or arbitrary-executable API. Unsigned developer build; no physical GPU/VSR, sound-device, HDR, full codec or long-session certification.

## Build

Windows x64, Visual Studio C++/Windows SDK and CMake 3.24+. Compile against the `include` directory of VideoLAN source `6de05adcbaf2e8b85fe86aad4169393098628119` (3.0.24):

```powershell
cmake -S native/vlc-player -B build/vlc -A x64 -DVLC_INCLUDE_DIR=C:/src/vlc/include
cmake --build build/vlc --config Release --parallel
ctest --test-dir build/vlc -C Release --output-on-failure
```

For the complete native worker/shaders/installer build, use the pinned dependencies and explicit steps in `.github/workflows/vlc-player.yml`; the player-only command above does not build its worker. Official VLC test archive SHA-256 is `fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323`. Real tests decode generated AVI/PCM, check actual adjusted/bypassed pixels, subtitles, transport, silent decoders and model rejection. See THIRD_PARTY_NOTICES.md for source and license details.
