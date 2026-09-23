# Implementation status — 0.4.0 LookLock creator review

This status document is separate from the unchanged input research plan. It does not retroactively rewrite the plan's research date, performance claims, or proposed acceptance gates.

| Plan milestone | Delivered in this repository | Still required |
|---|---|---|
| M0 Feasibility/provenance | Original spatial shaders, model registry, deterministic local fixture, dependency lockfile, explicit model-use gate | Physical adapter inventory, sustained hardware benchmarks, independently approved neural model |
| M1 Capture/presentation | Local viewer, supported native-controls inline mode, explicit separate browser capture viewer, Windows picker/SDR preview source | Interactive Windows capture/resize/close/DPI/feedback validation, D3D/Vulkan interoperability |
| M2 Browser enhancement | WebGPU and WebGL2 spatial backends, presets, comparison, local settings, bounded scheduling, PNG export and diagnostics | Cleared animation/photo models, tiled neural inference, broader tested site adapters, named-hardware budgets |
| M3 Native control | Typed native messaging, opaque per-session IDs, per-user installer, D3D11 spatial preview, stop and emergency shortcut | Live extension-to-native integration tests on Windows, overlay window following, sustained device-loss/DPI/recovery validation |
| M4 NR/temporal research | Pinned MIT browser graph, dedicated-worker independent-frame adapter, GPU texture/feature/composition bridge, reviewed-manifest gate and local hash validation | Approved weights, complete neural inference/parity evidence, native/Vulkan adapter, optical flow, temporal video evaluation and physical-GPU certification |
| M5 Whole-display/hardening | Packaging/checksums, CI gates, capture-exclusion code and explicit limitations | Click-through desktop overlay, signed installer/update pipeline, full-display certification, multi-monitor validation and store publication |

No milestone is claimed complete merely because its source files exist. Ordinary CI does not satisfy the physical-GPU, long-session, native interop, or quality criteria in the plan.

## Version history and boundaries

The original 0.1.0 baseline is a spatial-enhancement application, not a reproduction of the proprietary DLSS 5 network. It did not copy a reference implementation. Version 0.2.0 added transactional source loading, comparison drag/keyboard controls, single-preview video transport, remembered presets, explicit export views, fullscreen/help, and cancellable diagnostics.

Version 0.3.0 adds pinned MIT OpenDLSS-NR browser source as a submodule; notices and loader modifications are explicit. The default build cannot activate neural inference with its empty reviewed-model register. Original media playback and model metadata inspection work independently. CI tests the GPU frame wrapper with synthetic residuals, not a trained network or vendor parity. This is a separately gated browser research path and does not complete M4. See `neural-lab.md`.

Native Windows code is unchanged. No NVIDIA runtime, weights, HDR or click-through desktop overlay is included. Keep measurements, source revision, hardware and limitations together in each evidence report.

## 0.4.0 — LookLock

A working still-frame creator workflow for external DLSS/other processed outputs: original/video-frame selection, same-frame confirmation, pixel-exact source locks, outside feather, raw change map, alpha-aware composition, independent lossless PNG output, three-panel review board, protection mask, source-bound recipes and privacy-conscious numerical reports. The synthetic demonstration is not neural output. Browser and extension entry points are included without new permissions or dependencies.

This does not complete the NR/model-rights, native, temporal, or physical-GPU milestones above. No model register is relaxed and no inference is activated. See `looklock.md` for the specific guarantees and limitations.
