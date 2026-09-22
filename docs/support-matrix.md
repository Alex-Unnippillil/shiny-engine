# Support matrix and known limits

| Capability | Status / boundary |
|---|---|
| Local still images | Implemented; PNG/JPEG/WebP/AVIF, decoder/GPU/allocation limits apply |
| Local video | Implemented for browser-decodable MP4/WebM/Ogg; original native controls/audio retained |
| WebGPU | Implemented; detected at runtime; shader/device failures trigger the original or WebGL2 fallback |
| WebGL2 | Implemented fallback; browser CI uses software graphics, not a certified physical adapter |
| Browser UI | Responsive; 390px and desktop layouts included in automated checks |
| Inline web video | Explicit opt-in; top-frame readable unprotected video with native controls, no transformed layouts |
| Custom players / cross-origin iframes | Not universally supported; use explicit separate viewer when permitted |
| Tab/window capture | Implemented with explicit consent; interactive picker/audio/protection behavior needs platform validation |
| Native Windows | Source compiled in Windows CI; protocol and HLSL tests; real capture/hardware not certified |
| Native display source | The Windows system picker may offer it; result is a separate excluded preview, not desktop-compositor enhancement |
| SDR | Initial processing scope; native companion rejects detected PQ HDR outputs at initialization |
| HDR | Unsupported; do not enable Windows HDR during a native session; restart the companion after display changes |
| Neural/DLSS/animation/photo restoration models | Unavailable; no weights bundled and no neural inference implemented |
| Click-through desktop overlay | Not implemented |
| DRM or capture protection bypass | Not implemented and not permitted |
| Browser stores, signed installer, automatic updates | Not published/implemented |
| macOS/Linux native companion | Not implemented; the browser lab may work where supported graphics APIs are available |

Browser graphics labels describe the selected backend, not speed certification. Output is capped and the actual pixel dimensions are displayed. A desktop browser with a compatible GPU is the initial target; the responsive UI alone does not establish mobile capture support.

Browser tab/window capture requests video only. Audio stays at the original source. The companion preview also leaves audio with the source application. Synchronization and latency across these paths require real-platform measurements. Closing the browser lab releases browser capture, but a separate native preview must be stopped independently.

## Before publishing a production support claim

Record browser/OS/driver/GPU versions, source/display geometry, codec/color information, actual backend, p50/p95/p99 complete-pipeline timings, long-session frame deadlines and resource counts. Test source replacement, denial, seek, minimize, fullscreen, 100 lifecycle cycles, device loss, sleep/resume and mixed-DPI displays. Do not replace these checks with an Actions green badge.
