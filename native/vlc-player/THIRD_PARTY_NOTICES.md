# Third-party provenance

Shiny Player is original MIT integration code. It dynamically uses a separately installed libVLC engine. No VideoLAN GUI source, runtime, plugins or branding assets are copied into the application package.

## VideoLAN

Public C headers are from https://github.com/videolan/vlc at `6de05adcbaf2e8b85fe86aad4169393098628119`, tag 3.0.24. Headers and libVLC are LGPL-2.1-or-later; upstream COPYING.LIB is included as LGPL-2.1.txt. Users can replace the compatible separately installed libVLC3.0 runtime. Corresponding upstream source is at that commit and https://download.videolan.org/pub/videolan/vlc/3.0.24/ .

The full VLC application is GPL-2.0-or-later, with some dependency combinations requiring GPL-3.0. Its plugins/dependencies retain their terms. This project neither relicenses nor redistributes that runtime. A separate runtime distributor must satisfy its exact build's source, notice, relinking and dependency requirements.

The optional D3D11 `super` request uses upstream direct3d11.c/d3d11_scaler.cpp. Its NVIDIA path requests RTX VSR, not DLSS5 neural rendering.

## Native OpenDLSS-NR

MIT source/kernels from https://github.com/maanHimself/OpenDLSS-NR at `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`, copyright the upstream authors. The binary16 helper follows the pinned src/main.cpp output rule. LICENSE and NOTICE accompany the worker; pinned source is a separate release asset. No scene assets or model data are included. MIT code licensing grants no rights to NVIDIA model weights or other intellectual property.

volk `776893306c5d3b22b6185b5d4a258b81d94572bf` (https://github.com/zeux/volk) is MIT. Vulkan-Headers `6802bb4733b63ed5efd3adb308a6c885ef180ea1` provides Apache-2.0 OR MIT headers. Their notices are included. GPU drivers stay separately installed. glslang16.6.0 is a checksum-pinned build tool, not a redistributed executable.

## Changes and installation

The original MIT validation/parser/layout code separates opted-in research from curated approval. scripts/prepare-native-paths.py applies hash-verified UTF-8 file-opening changes to generated build copies of nr_model.cpp, kernels.cpp and vk_context.cpp only; graph arithmetic and kernels are unchanged. ModelGuard hashes stage data while holding file/directory handles; upstream's duplicate case-sensitive hash check is disabled only after this validation.

Setup optionally downloads the official VideoLAN archive directly, or accepts a hash-matching local cache. It does not bundle/rehost the archive, modify system VLC, create a service or register default file handlers. Included archive notices stay intact. Shiny Engine is independent of VideoLAN and NVIDIA; names indicate interoperability, not endorsement.
