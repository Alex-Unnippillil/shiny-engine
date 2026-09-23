# Shiny Player 0.5.0 — Windows x64

Independent player using the real VideoLAN libVLC engine. **This is not an activated DLSS 5 product or a complete clone of VLC's interface.** No VLC/NVIDIA binaries or model weights are bundled.

Install official **64-bit VLC 3.0.24 or newer in the 3.0 series** from VideoLAN, then run `ShinyVlcPlayer.exe`. The default installation at `Program Files/VideoLAN/VLC` is detected; otherwise select **Media > Locate installed VLC** and choose its complete directory. Use only a trusted official installation: selecting a directory authorizes executing its native libraries and plugins. VLC 4 uses a different ABI and is not accepted.

## Playback

Open/drop files or explicitly enter a media URL (HTTP, HTTPS, RTSP, RTP, UDP or SRT). Supports queues, repeat/shuffle, seeking, A-B loops, playback speed, audio/subtitle track selection, external subtitles, timing offsets, equalizer presets, aspect/crop, deinterlace, chapters, frame step, fullscreen and PNG snapshots, where the input/runtime supports the operation. The installed VLC plugins determine format support. No universal codec, disc, device or URL claim is made.

**Video > Request driver super resolution on next open** passes VLC's own D3D11 `super` scaling option when the next item opens. On suitable NVIDIA hardware this upstream path requests RTX Video Super Resolution. It is NOT DLSS 5 neural rendering. The app reports requested configuration, not verified driver execution or image improvement. Hardware behavior is untested here. Disable the request and reopen media to return to automatic VLC output. It is off by default.

The four live adjustment sliders are VLC's conventional contrast, brightness, saturation and gamma filters. They are not AI. Disable adjustments to bypass. Filtering/HDR compatibility is not certified; this release is evaluated with SDR fixtures.

**Compare video** plays a user-imported processed file next to the source with source audio only. Confirm matching source/timeline/crop; seeking is best effort (not frame-exact), and duration mismatches over 1.5 seconds are rejected. This does not generate DLSS output or verify its provenance. Source image adjustments are disabled during comparison. Snapshots are displayed VLC frames and may contain filters/subtitles.

**Full VLC** opens the separately installed original VLC interface for advanced conversion, capture, disc menus, casting and other controls not duplicated here. Shiny playback is paused to avoid duplicate audio. Those features operate in VLC, not in this custom interface.

## Keyboard

Space play/pause; S stop; E next frame; left/right seek 5 seconds; Shift+left/right 30 seconds; N/P next/previous; M mute; F fullscreen; [ and ] set A/B; Escape leave fullscreen or stop; Ctrl+O files; Ctrl+N network.

## Privacy and limits

No media history, cloud processing or telemetry. Network input contacts the explicitly selected endpoint through VLC. Local subtitle/playlist references may be resolved by VLC. An explicitly exported M3U8 contains paths/URLs and should be treated as private. Diagnostic reports omit them. No browser or native protocol accepts arbitrary paths or executable arguments for this player. Unsigned developer build, not store distribution. No physical GPU, real audio-device, HDR, long-session or full codec certification.

The existing browser Neural Lab has separate model gates. **No native DLSS 5 inference adapter, approved runtime or trained model is included.** Loading an already enhanced video is playback, not neural inference.

## Build

Requires Windows x64, Visual Studio C++/Windows SDK, CMake 3.24+. Obtain VideoLAN `videolan/vlc` source at commit `6de05adcbaf2e8b85fe86aad4169393098628119` (3.0.24) and point at its `include` directory:

```powershell
cmake -S native/vlc-player -B build/vlc -A x64 -DVLC_INCLUDE_DIR=C:/src/vlc/include
cmake --build build/vlc --config Release --parallel
ctest --test-dir build/vlc -C Release --output-on-failure
```

CI additionally downloads the official runtime pinned to SHA-256 `fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323`, decodes original generated AVI/PCM fixtures, checks actual adjusted/bypassed pixels, subtitles, transport, a second silent decoder and native UI. Runtime binaries are not included in the resulting package. License/source details are in THIRD_PARTY_NOTICES.md.
