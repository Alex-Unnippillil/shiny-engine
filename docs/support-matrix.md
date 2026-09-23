# Support matrix and known limits

| Capability | Status / boundary |
|---|---|
| Local still images | PNG/JPEG/WebP/AVIF, decoder/GPU/allocation limits apply |
| Local video | Browser-decodable MP4/WebM/Ogg; original controls/audio retained |
| Spatial WebGPU/WebGL2 | Runtime selection with original/WebGL2 fallback; software browser tests are not physical-adapter certification |
| Browser UI | Responsive; narrow layouts included in automated checks |
| Inline web video | Explicit opt-in; top-frame readable unprotected video with native controls, no transformed layouts |
| Custom players / cross-origin iframes | Not universally supported; use an explicit separate viewer when permitted |
| Tab/window capture | Explicit consent; interactive picker/audio/protection behavior needs platform validation |
| Native Windows | Compiled in Windows CI with protocol/HLSL tests; real capture/hardware not certified |
| Native display source | System picker may offer it; separate excluded preview, not desktop-compositor enhancement |
| SDR | Initial processing scope; native companion rejects detected PQ HDR outputs at initialization |
| HDR | Unsupported; do not enable Windows HDR during a native session; restart after display changes |
| OpenDLSS-NR browser adapter | Experimental independent-frame graph adapter; activation locked until a reviewed exact model is approved. No trained-model inference or parity validated |
| Native DLSS / temporal / animation / photo models | Unavailable; not implemented/enabled. No model weights bundled |
| Click-through desktop overlay | Not implemented |
| DRM or capture-protection bypass | Not implemented and not permitted |
| Browser stores, signed installer, automatic updates | Not published/implemented |
| macOS/Linux native companion | Not implemented; browser lab may work where supported graphics APIs are available |

Graphics labels describe the selected backend, not speed certification. Actual output dimensions are displayed. The neural research preview is capped at 320/512 pixels on its longest axis; it is not a 4K real-time claim. A responsive UI does not establish mobile capture support.

Browser tab/window capture requests video only. Audio stays at the original source. The companion preview also leaves audio with the source application. Synchronization and latency require real-platform measurements. Closing the browser lab releases browser capture, but a separate native preview must be stopped independently.

## Before publishing a production support claim

Record browser/OS/driver/GPU versions, source/display geometry, codec/color information, actual backend, p50/p95/p99 complete-pipeline timings, long-session frame deadlines and resource counts. Test source replacement, denial, seek, minimize, fullscreen, 100 lifecycle cycles, device loss, sleep/resume and mixed-DPI displays. Do not replace these checks with an Actions green badge.
