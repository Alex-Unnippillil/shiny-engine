# Authorized local-model research — 0.7.0

The native player can load user-supplied model data through a separate, explicit local research path. This is not a production-certified DLSS 5 backend. No trained-model output or physical NVIDIA GPU has been validated by ordinary CI. Published build reports distinguish real playback, intake and UI tests from neural correctness and performance.

## Run a local experiment

1. Open a seekable local video and select **DLSS-NR research**.
2. Select **Local research (unreviewed model)**. It is off by default and not persisted.
3. Select **Inspect model folder**, supplying the native 71-block data layout compatible with the pinned OpenDLSS-NR graph—not an arbitrary ONNX file, checkpoint or DLL.
4. After inspection succeeds, acknowledge that you are authorized to use this model and accept unverified output for this session.
5. Select **Prepare model**. The worker rechecks the exact manifest, tensor bounds and stage hashes before loading the actual native graph. An incompatible GPU or invalid model is an error, never a simulated result.
6. Adjust **Tone**, **Structure** and **Neural mix**. Compare side-by-side, wipe, original input or experimental output. Pause the independent preview to compare settings on one retained input.
7. Export an experimental PNG or JSON report explicitly. Stop releases the worker; closing the studio also disposes its muted decoder. Main-player playback stays separate.

A local model is never added to the curated approval registry. Acknowledgment is user intent, not proof of licensing, authenticity or numerical correctness. Educational use alone does not grant rights to third-party model data. No model downloads, extraction scripts, caller-validation shims or NVIDIA DLLs are included.

## Data format and validation

```text
model-folder/
  manifest.json
  model/
    stage-files.bin
```

The manifest requires `totals.blockCount: 71`, `stages` and `tensors`. Stages identify a unique `id`, relative `.bin` file, `packedByteLength` and SHA-256. Tensors identify `blockN.layerM.parameter`, block/layer/parameter, stage, stageOffset and byteLength.

Limits are 2 MiB manifest, nesting depth 16, 100,000 JSON nodes, 4,096-byte strings, 256 stages, 1,024 tensors, 256 MiB aggregate stage data and 256 MiB aggregate tensor allocations. Required graph tensors and their byte layouts are validated before the upstream reader runs. This is a pinned-graph format, not a universal checkpoint loader.

Duplicate keys, malformed UTF-8/escapes/numbers, wrong types, nonintegral ranges and trailing content are rejected. Stage filenames are bounded relative ASCII paths. Traversal, Windows device names, absolute paths and reparse links are rejected. Data and ancestor-directory handles are held to prevent replacement while loading. Altering the manifest requires renewed inspection and acknowledgment. Hash checks establish agreement with the manifest, not provenance or output quality.

The upstream model/shader readers receive a small build-time UTF-8 path adaptation. Exact normalized source fingerprints are checked before that transformation; graph arithmetic and original reference sources remain unchanged.

## Native process boundary

`--inspect` and `--serve` remain reviewed-only. `--inspect-research <folder>` validates data without GPU inference and returns a fingerprint. `--serve-research <folder> <acknowledged-sha256>` revalidates and loads the graph. The SHA-256 is not a secret or a security boundary against modifying one's own program.

Only the packaged fixed-path worker is launched. Private inherited pipes carry bounded frame messages. Model files are data, not executable plugins. Browser/native-messaging APIs gained no shell, arbitrary executable or DLL command.

## Preview and reports

The independent muted libVLC decoder supplies opaque SDR RGBA frames, 33 pixels minimum per dimension and 512 pixels maximum on the longest edge. CPU transfer overhead is explicit. There is one processing frame and one replaceable pending frame. This is not zero-copy, temporal reconstruction, 4K, neural video encoding or audio-synchronized enhanced playback.

Control revisions invalidate older results. Both comparison halves use the same retained input. Export is disabled until an actual result returns; no fallback sharpening is relabeled as neural output. Reports include model fingerprint, controls, geometry, approximate libVLC source position and worker round-trip time. They omit media paths and pixel payloads. Fingerprints can still identify a model; share deliberately.

## Production boundary

The original `RESEARCH_AND_IMPLEMENTATION_PLAN.md` is retained unchanged. At the owner's request, local educational experimentation is separated from curated production approval. This does not satisfy the plan's trained-model parity, temporal quality, named-hardware performance, HDR, signing or full-platform requirements. Independent authorized trained-model fixtures and hardware evaluation are still required before any such claims. A nonconstant image is not a parity test.
