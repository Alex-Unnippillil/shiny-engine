# Third-party provenance

Shiny Player is original MIT integration code, dynamically using a separately installed libVLC engine. No VideoLAN GUI source, runtime, plugins, icons or branding assets are copied into the application package.

## VideoLAN

VLC/libVLC are owned by VideoLAN and their authors. Public C headers used here are from https://github.com/videolan/vlc at `6de05adcbaf2e8b85fe86aad4169393098628119`, tag3.0.24. Headers/libVLC use LGPL-2.1-or-later. Upstream COPYING.LIB is included as LGPL-2.1.txt. Users may replace the separately installed compatible libVLC3.0 runtime. Corresponding upstream source is at that commit and https://download.videolan.org/pub/videolan/vlc/3.0.24/ .

The full VLC application is GPL-2.0-or-later; some dependency combinations require GPL-3.0. Plugins and dependencies retain their own licenses. This project neither relicenses nor redistributes the VLC runtime. Anyone separately redistributing a combined runtime must satisfy that exact build's source, notice, relinking and dependency obligations.

The optional `d3d11-upscale-mode=super` request uses upstream `modules/video_output/win32/direct3d11.c` and `d3d11_scaler.cpp`. Its NVIDIA branch requests RTX VSR, not a DLSS5 model. No NVIDIA SDK/DLL/model is included.

## Native OpenDLSS-NR

The worker compiles source/kernels from https://github.com/maanHimself/OpenDLSS-NR at `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`, MIT, copyright the upstream authors. The truncating binary16 helper follows the pinned `src/main.cpp` output rule. OpenDLSS LICENSE/NOTICE accompany the worker. No scene assets or model data are included. Pinned upstream source is attached separately to the release. Upstream's MIT license does not grant rights to NVIDIA model weights or intellectual property.

volk from https://github.com/zeux/volk at `776893306c5d3b22b6185b5d4a258b81d94572bf` is MIT. Vulkan-Headers `6802bb4733b63ed5efd3adb308a6c885ef180ea1` supplies compile-time headers under its Apache-2.0 OR MIT terms. Their license texts are included in `nr/`. Vulkan drivers remain installed separately. glslang16.6.0 is a checksum-pinned build tool; its executable is not distributed.

## Installer

Setup offers an explicit optional download of the official VideoLAN archive into the app-private runtime folder. The user downloads it directly from VideoLAN or supplies an independently obtained matching cache; this project does not bundle or rehost it. Archive notices remain intact. Setup creates no default handlers, system VLC changes, service or startup task.

Shiny Engine is independent of VideoLAN and NVIDIA; product names identify interoperability, not affiliation or endorsement.
