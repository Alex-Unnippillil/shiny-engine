# Implementation status — 0.2.0 browser UX alpha

This status document is separate from the unchanged input research plan. It does not retroactively rewrite the plan's research date, performance claims, or proposed acceptance gates.

| Plan milestone | Delivered in this repository | Still required |
|---|---|---|
| M0 Feasibility/provenance | Original spatial shaders, model registry, deterministic local fixture, dependency lockfile, explicit no-model policy | Physical adapter inventory, sustained hardware benchmarks, independently approved neural model |
| M1 Capture/presentation | Local viewer, supported native-controls inline mode, explicit separate browser capture viewer, Windows picker/SDR preview source | Interactive Windows capture/resize/close/DPI/feedback validation, D3D/Vulkan interoperability if NR is pursued |
| M2 Browser enhancement | WebGPU and WebGL2 spatial backends, presets, comparison, local settings, bounded scheduling, PNG export and diagnostics | Cleared animation/photo restoration models, tiled neural inference, broader tested site adapters, named-hardware budgets |
| M3 Native control | Typed native messaging, opaque per-session IDs, per-user installer, D3D11 spatial preview, stop and emergency shortcut | Live extension-to-native integration tests on Windows, overlay window following, sustained device-loss/DPI/recovery validation |
| M4 NR/temporal research | Explicit unavailable backend metadata and provenance requirements | No OpenDLSS adapter, approved weights, parity fixtures, motion estimation or temporal NR implementation exists yet |
| M5 Whole-display/hardening | Packaging/checksums, CI gates, capture-exclusion code and explicit limitations | No click-through desktop overlay, signed installer/update pipeline, full-display certification, multi-monitor validation or store publication |

No milestone is claimed complete merely because its source files exist. Passing ordinary CI does not satisfy the physical-GPU, long-session, native interop, or quality criteria in the plan.

## First public code drop

The useful baseline is an original spatial-enhancement application, not a reproduction of the proprietary DLSS 5 network. It is intentionally labeled alpha. The NVIDIA, OpenDLSS-NR, NijiLucid, Anime4K, Magpie and Real-ESRGAN references were not copied into this implementation.

Subsequent work should begin with interactive validation of the native spatial path and additional browser-video/site regression fixtures before adding more backends. Keep actual measurements, source revision, hardware and limitations together in each evidence report.

## Browser UX update

The 0.2.0 update adds transactional source loading, comparison drag/keyboard controls, single-preview video transport, remembered presets, explicit export views, fullscreen/help, and cancellable diagnostics. The native binary is unchanged. These improvements do not complete the remaining M0–M5 model, hardware, temporal or overlay gates. See `ux-release.md` and CI artifacts for the tested revision.
