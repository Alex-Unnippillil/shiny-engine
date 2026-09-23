# Native OpenDLSS-NR integration — 0.6.0

## Implemented path, not activated trained inference

This release compiles the real MIT OpenDLSS-NR 71-block Vulkan graph and its shader/PTX kernels into **ShinyNrWorker.exe**, pinned to `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1`. This is not a spatial filter branded as DLSS. It is also not an NVIDIA-supported DLSS implementation. No NVIDIA runtime, SDK, caller-validation shim, model weights or downloaded model is included.

**No trained model is reviewed in this release. The native approval register is empty, so model preparation is blocked.** GPU feature probing, local manifest fingerprinting, private worker error handling and a separate real-libVLC original-frame preview are functional independently. A GPU probe passing is not a neural correctness/performance test.

Open a local seekable video and choose **Neural workbench**. The secondary decoder is muted. Choose **Check GPU** to try native Vulkan creation, or **Choose model folder** to inspect the exact local manifest identity. Selecting a file is not an approval. The panel states the blocking reason rather than substituting another enhancement backend.

## Runtime pipeline

A separate silent libVLC decoder writes bounded RV32 frames to aligned storage. The workbench normalizes them to opaque SDR RGBA. A latest-frame mailbox replaces older pending work; only one private worker request is processed at a time. Preview longest edge is capped at 512, aspect preserved, minimum side 33 pixels.

The child process is fixed at `nr/ShinyNrWorker.exe` beside the player. Only its stdin/stdout and a diagnostics sink are inherited. It is in a kill-on-close Windows job, never elevated. There is no listening port, arbitrary executable selection, shell invocation or browser RPC. Stop and panel close cancel the process and dispose the decoder. The original player remains separate.

For an independently reviewed model build, the worker locks the manifest/stage files against mutation, verifies their hashes, creates the upstream model/kernels, preprocesses the frame, executes the native graph, reads its head and composes the RGB residual. Invalid/nonfinite output fails rather than being displayed. Model files are locked until resources are released. Requests and results are sequence/geometry checked.

CPU frame upload, GPU readback and composition are deliberate initial integration costs. This is **not zero-copy or certified real-time**. Timing displayed after a successful result covers the worker round-trip, not only network compute. Inputs are independent SDR frames, without optical flow/history. No temporal stability, 4K or 30/60 FPS claim. The workbench compares its own exact submitted input to the returned result; it is not frame-synchronized with the main player's audio.

## Model review requirement

The model layout is the pinned OpenDLSS native layout: `manifest.json`, `model/<stage.file>`, the exact 71-block topology and stage/tensor metadata. Reviewers must establish lawful model runtime use and independent native input/head/output fixtures. Then record the exact manifest SHA-256, review identifier, runtime-use evidence and native-parity evidence in `native/nr-worker/approvals.hpp`. Rebuild and re-run real model tests on supported hardware. Never add a placeholder approval or remove checks to create an enabled badge.

OpenDLSS's public MIT license covers its code, not NVIDIA weights/IP. This project does not provide instructions or tools for extracting or obtaining leaked assets. The existing browser register is not changed by the native register.

## Research references and differences

- NVIDIA technical report: https://research.nvidia.com/labs/adlr/DLSS5/ — current frame, engine motion, temporal state and artistic controls. Ordinary video does not include the engine motion buffer.
- Native code: https://github.com/maanHimself/OpenDLSS-NR — MIT code, same-resolution generative NR, no weights; its tool's single-frame route is this integration's reference. Upstream speed/parity reports are not this project's measurements.
- https://github.com/Zonnery/dlss5-nr-player — reference for native media workflows only. No caller-validation shim, bundled runtime or unlicensed source copied.
- VLC's driver super-resolution request is a different feature (RTX VSR on supported NVIDIA paths), not DLSS 5 NR. VLC contrast/brightness/saturation/gamma are conventional filters.

## Validation

Portable tests exercise protocol rejection, size/sequence checks, composition, transparency, nonfinite rejection and an empty approval register using explicitly synthetic residuals. Windows tests compile the real graph/kernels and exercise real VLC decoding/sample conversion plus rejection via the actual isolated worker. None of these constitutes trained-model execution, parity, temporal evaluation, physical GPU performance or visual-quality certification. Keep these missing tests explicit in release notes.
