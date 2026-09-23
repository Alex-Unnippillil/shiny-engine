# Shiny Player 0.7.0

Windows x64 player and opt-in native OpenDLSS-NR local-model research studio. Unsigned prerelease; not a certified NVIDIA DLSS product.

## Download

Use **ShinyPlayer-0.7.0-Windows-x64-Setup.exe** for per-user setup/shortcuts/uninstall, or **ShinyPlayer-0.7.0-Windows-x64-Portable.zip**. Setup optionally fetches the SHA-256-verified official VLC runtime; it does not modify system VLC. Offline installation needs an existing compatible x64 VLC3.0.24+ (3.0 series) or the matching cached archive. No model weights or NVIDIA runtimes are bundled.

## Changes

- Explicit local research mode, strict bounded manifest/tensor schema, stage hashes, replacement-resistant file/directory locks and session acknowledgment bound to the inspected manifest.
- Actual native OpenDLSS graph invocation for suitable user-supplied data/hardware; no fake sharpening fallback. Reviewed-only registries remain unchanged.
- Numerical tone/structure/mix controls, retained-input comparison, wipe/original/output views, pause, cancellation, stale-result invalidation, PNG and JSON exports.
- DPI-aware fonts, smaller-window reflow, persistent queue access, clearer player controls and installation guide.
- Reproducible pinned build script, Windows tests, versioned installer/portable/source packages and actual screenshots.

## Validation boundaries

Release publication requires browser-build, windows-build and vlc-player success on the exact current main revision. Attached reports contain real software playback, model rejection, native UI and installer results. Synthetic data validation is not trained inference. No trained-model parity, physical GPU/VSR performance, temporal quality, HDR, neural audio synchronization, all-codec compatibility or long-session certification is claimed.

Local research requires model data you are authorized to use. Selection and acknowledgment do not confer rights or certify results. The 33–512-pixel SDR preview is separate from main audio playback and explicitly transfers through CPU memory. No native macOS/Linux installers are provided.
