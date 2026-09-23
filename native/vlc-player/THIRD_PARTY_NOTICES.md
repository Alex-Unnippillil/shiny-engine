# Third-party provenance

The Shiny Player application is original MIT-licensed integration code. It dynamically loads the libVLC engine supplied in a separately installed VLC distribution. No VideoLAN GUI source, native runtime, plugins, icons or branding assets are copied into the application package.

VideoLAN / VLC authors own VLC and libVLC. The public C headers used to compile this integration are from https://github.com/videolan/vlc at immutable commit `6de05adcbaf2e8b85fe86aad4169393098628119`, tag `3.0.24`, published September 22, 2026. The headers and libVLC are LGPL-2.1-or-later; upstream COPYING.LIB is included as LGPL-2.1.txt in the binary package. Users can replace the separately installed compatible libVLC 3.0 runtime. The complete upstream corresponding source is available at that commit and https://download.videolan.org/pub/videolan/vlc/3.0.24/ .

The VLC application is GPL-2.0-or-later, with dependency combinations that may require GPL-3.0. VLC plugins and dependencies retain their individual licenses. This project does not relicense them and does not redistribute the VLC runtime. Anyone combining/distributing a VLC runtime must satisfy that exact build's corresponding-source, notice, relinking and dependency requirements, rather than relying only on this notice.

The optional `d3d11-upscale-mode=super` setting is implemented by upstream `modules/video_output/win32/direct3d11.c` and `d3d11_scaler.cpp` at the pinned revision. Its NVIDIA branch requests RTX VSR through a video-processor extension. It is not a DLSS 5 model. No NVIDIA SDK, DLL or model is added here.

Shiny Engine is not endorsed by, affiliated with, or a replacement distribution from VideoLAN or NVIDIA. Product names identify interoperability only.
