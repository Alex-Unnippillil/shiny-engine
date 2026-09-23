# Implementation status — 0.6.0 native NR workbench / Windows installer

Separate from the unchanged original research plan. This document does not retroactively change its date, upstream measurements or proposed acceptance gates.

| Plan milestone | Implemented here | Still required |
|---|---|---|
| M0 Feasibility/provenance | Original spatial shaders, deterministic fixtures, pinned source/dependencies and model-use registers | Actual GPU inventory, sustained benchmarks and independently reviewed neural model |
| M1 Capture/presentation | Browser viewers, supported inline mode, consented capture, Windows picker/D3D11 preview | Interactive capture/DPI/device recovery, full desktop behavior and D3D/Vulkan zero-copy interoperability |
| M2 Browser enhancement | Spatial WebGPU/WebGL2, controls, PNG export and diagnostics | Cleared restoration models, tiled inference, broader site compatibility and named-hardware budgets |
| M3 Native control | Typed native messaging and separate capture companion, per-user setup | Live extension/capture hardware tests, overlay following and sustained recovery validation |
| M4 NR/temporal research | Pinned browser graph and native Vulkan graph worker; separate muted VLC sampler, native model policy, bounded frame IPC, input/head composition, probes and explicit unavailable states | Reviewed weights, full trained-model execution/parity, native video quality, optical flow/history, physical-GPU performance |
| M5 Release/hardening | Exact-commit CI, source/checksum packages, unsigned Windows player installer/uninstaller and release workflow | Code signing, whole-display certification, macOS/Linux installers, HDR, store publication and multi-monitor validation |

No milestone is complete merely because source files exist. Software CI is not a physical GPU, long-session, temporal or image-quality certification.

## Version history

**0.1.0:** original spatial-enhancement baseline, not a reproduction of NVIDIA's network. **0.2.0:** transactional source loading, comparison controls, single-preview transport, preferences, explicit export views, fullscreen/help and cancellable diagnostics.

**0.3.0:** pinned MIT OpenDLSS browser submodule and independent-frame worker/graph bridge. Empty reviewed-model register keeps trained inference locked. Tests use synthetic residuals, not trained weights or vendor captures. See `neural-lab.md`.

**0.4.0 — LookLock:** external-output still review, local source/video-frame selection, exact decoded-pixel protected regions, outside feather, alpha-aware composition, lossless PNG, review boards, masks, hash-bound recipes and reports. Synthetic demonstration is not neural output. No model approval is relaxed. See `looklock.md`.

**0.5.0 — libVLC player:** independent Windows x64 player using VideoLAN C APIs and separately installed VLC3.0.24+. Playback/queue/transport/tracks/subtitles/equalizer/filters, driver-super-resolution request and imported-video comparison. Full VLC opens separately for advanced functionality. This version had no native neural adapter; RTX VSR requests were not model inference or verified hardware execution.

**0.6.0 — native NR and installable release:** actual pinned OpenDLSS-NR Vulkan graph plus GLSL/PTX kernels compiled into a fixed-path private worker. Bounded sequence-checked frame requests and kill-on-close process lifetime connect to a separate silent VLC sampler/workbench. Native Vulkan probing, local manifest fingerprinting and original sampled playback work without a model. **The reviewed native-model register is empty: trained inference is not activated, and no numerical parity, temporal stability or GPU speed is certified.** Browser approvals and original research remain unchanged.

The player adds cinema view, bookmarks, go-to-time, always-on-top, queue reordering, active audio-device selection and media information. Setup supports per-user installation, shortcuts, managed uninstall and optional official VLC retrieval with a pinned SHA-256. Existing system VLC is not overwritten. Existing VLC or an independently obtained matching cache supports offline installation. Versioned installers, portable and source packages publish only after required exact-main checks. No macOS/Linux installer, code signing or complete VLC GUI parity is supplied.

The Windows capture companion is unchanged. No proprietary NVIDIA runtime, model weights, HDR pipeline or click-through desktop overlay is added. Retain exact source revision, test scope, hardware and remaining limitations with each evidence report.
