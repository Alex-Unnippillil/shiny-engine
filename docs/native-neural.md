# Native OpenDLSS-NR integration — 0.7.0

This research path is independent of the browser Neural Lab and Windows capture companion. The player links pinned MIT OpenDLSS-NR source `9d08f4184bbcb9d858e2fb7a7834ec0837a9d2f1` into `ShinyNrWorker.exe`. It uses the real Vulkan graph rather than spatial filters under a DLSS name.

Version 0.7 adds explicit local-model intake and session acknowledgment. The curated production register remains unchanged. See [research-mode.md](research-mode.md) for data layout, steps, limits and interpretation.

```text
local video -> separate muted libVLC decoder -> bounded SDR RGBA
  -> private sequence-checked pipes -> validated local native model
  -> OpenDLSS graph -> same-input composition -> comparison / export
```

Tone and structure feed native preprocessing. Mix blends the returned result with its retained original. Control revisions discard outdated settings results. Worker errors do not substitute fake enhancement or stop the primary player.

Model weights and NVIDIA runtimes are not bundled. No trained-model inference, NVIDIA parity, physical-GPU performance or temporal quality is established by compilation and ordinary CI. The 512-pixel independent-frame preview is not synchronized to primary audio, and has explicit CPU upload/readback. No HDR, 4K, neural video export or desktop overlay claim.

Use `scripts/Build-Windows.ps1` to build all pinned native components on Windows. The CI workflow builds the same graph and tests actual VLC software decoding, worker rejection, UI lifecycle and installation. Additional positive synthetic data-intake tests do not constitute trained inference. Read the exact release's reports for performed tests.
