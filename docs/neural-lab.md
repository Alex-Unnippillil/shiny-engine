# Neural Lab — 0.3.0 experimental adapter

## Release status

The default release has **no approved model and no NVIDIA weights or runtime**. Consequently live neural enhancement is **not activated** by installing this extension. Opening a file shows its original pixels. The lab does not fake an enabled DLSS mode with sharpening. The independent implementation is not an NVIDIA product, SDK, or endorsement.

Implemented: an OpenDLSS-NR browser graph adapter, original media viewer, manifest inspection/verification, a dedicated compute worker, source-texture conversion, the 16-lane neural input, SDR residual composition, a bounded live loop, comparison, local export and explicit stop. This path calls the actual graph when the repository's model-approval conditions are met. It is not the planned native Vulkan real-time integration.

## Exact source and build

The source submodule `third_party/opendlss-nr` points at `maanHimself/OpenDLSS-NR`, commit `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`. Its MIT LICENSE/NOTICE are retained. `models/nr-reference-lock.json` additionally hashes every selected source file. `scripts/prepare-nr.mjs` refuses altered reference bytes and packages only that selected source set, not models, demos, arbitrary plugins or binaries.

Two reference modifications are explicit: shader fetching becomes a bundled module lookup, and model URL fetching becomes verified local-data intake. Matrix packing, graph scheduling and arithmetic remain reference code. The application additionally adapts proxy texture access and uses periodic mirror padding for small images. That adaptation and the SDR compositor need independent golden fixtures before approving a real model; upstream parity claims do not establish our adapter's parity.

```sh
git submodule update --init -- third_party/opendlss-nr
npm ci --ignore-scripts
npm run check
npm run test:browser
npm run package
```

CI source artifacts contain the main source archive and a separate pinned reference archive, because `git archive` does not expand a submodule. A recursive clone is the supported source installation path. Reference build scripts are not executed.

## Approval is not a file-picker checkbox

`models/nr-approvals.json` is the reviewed source-of-truth register, compiled into the worker; it is empty. There is no runtime setting, localStorage flag, URL parameter, or imported approval file that can turn an unknown model into an approved one.

A future approval change must identify the exact manifest SHA-256, pinned implementation, review identifier/reviewer, established runtime-use permission, and independent correctness evidence for `browser-independent-frames`. Establish rights for the precise data and use case separately from the MIT source-code license; possession of a file does not grant them. Do not distribute proprietary assets as part of that review or obtain leaked/extracted data to populate this application. Document runtime-use, conversion and distribution scope separately.

The manifest references `model/<stage.file>` paths (typically `model/stages/*.bin`) and the 71-block tensor layout. Intake rejects path traversal, invalid counts, duplicate names, inconsistent offsets, unknown model fingerprints, excessive file counts and byte budgets. Every stage is SHA-256 checked before transfer. The worker reparses original manifest bytes and rechecks every stage against the compiled register before GPU construction. Data never becomes executable code.

Adding an approval must include independently checked tensor layouts, golden input/features/head/output fixtures that can legally be used, visual comparisons and documented deviations from the engine-rendered proxy. Do not approve merely because a network produced a changed image. A synthetic validator fixture is not a model approval or an inference test.

## Frame pipeline and live behavior

A decoded `VideoFrame` or `ImageBitmap` transfers to a dedicated worker. The GPU snapshots it at the selected 320/512-pixel longest edge, preserving aspect ratio and never advertising full source resolution. The snapshot becomes the 16-lane input with the reference normalization, fixed-seed noise and explicit tone/structure values. The pinned `Network` records the complete graph. Its RGB residual is composed at a quarter scale in SDR code space, clamped and narrowed; original alpha is retained.

A GPU validation pass rejects non-finite head values. Only its four-byte flag is read back; complete video frames are not read into JavaScript pixel arrays during processing. Intermediate GPU buffers/textures are retained, and graph construction happens during preparation. Initialization has an allocation guard and a 120-second watchdog; individual processing calls have a 30-second watchdog. Timeouts terminate the worker and preserve original playback. This does not guarantee immediate interruption of a command already submitted to a GPU driver.

All frame/present/export requests share one serial queue slot; settings changes coalesce rather than building up stale work. Capture/source changes invalidate old results. Failed decoding and denied/cancelled capture preserve the current source; late captured tracks are stopped. The original player owns audio. Live capture is video-only and offers another window/tab, not the current viewer or whole-monitor replacement. No DRM or capture protection is bypassed.

Temporal history is reset **every frame**. Desktop frames are not engine motion vectors. No optical flow, reprojection, engine HDR proxy, temporal stability, frame interpolation or actual NVIDIA output equivalence is claimed. Flicker is possible. The main source and the latest neural result can differ in time; the original/neural split inside the output canvas always compares the same snapshot.

## Metrics and validation boundaries

Busy throughput divides processed frames by time spent creating/transferring/processing those frames; it is **not display FPS**. Frame completion includes worker GPU submission, computation and the four-byte validation readback. Reports omit filenames, frames, model bytes and browsing URLs. Reports include the model manifest fingerprint and mark real-model inference/physical GPU certification as unverified.

Unit tests cover parser/approval integrity, bounds, stage hashes, cancellation, worker serial correlation/timeouts and bundled import closure. Browser tests exercise the actual locked release UI, original images, capture cancellation, worker approval enforcement and responsive geometry. Software-WebGPU tests inject a **synthetic residual**, exercising proxy preprocessing, padded feature lanes, composition, alpha, external video textures and invalid-head rejection. They do not execute a trained model or prove full-network numerical parity/performance. See the exact revision's CI results; a test being present does not mean it passed.

No proprietary model is available in this development session. No full neural inference, live neural video quality, physical GPU benchmark or native DLSS integration has been verified. Those remain gates rather than a green CI badge implying completion.
