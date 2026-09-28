# Shiny Player 0.10.1 — optional external swapper

This patch adds an opt-in **Tools → Optional DLSS 5 Swapper (external)** panel. It supports user-selected installed/portable DLSS 5 Swapper and classic DLSS Swapper executables, background fingerprinting, per-launch consent, cancellation, changed-file rejection and direct argument-free process launch. No vendor binaries are bundled. This is an external-tool handoff, not a DLSS video integration or sandbox. Existing playback, model approval and package rollback remain unchanged. Read `docs/optional-swapper.md` before using third-party tools.

The same feature is available in the Windows build of Shiny Desktop 0.1.1. macOS/Linux remain local-media players with this Windows-only entry disabled. Both release lines retain their independent version numbers and earlier published assets.

## Previous 0.10.0 workspace features (retained)


Unsigned Windows x64 prerelease. This update modernizes the main workspace and documentation; processing and model/library trust boundaries remain unchanged.

## Delivered

A shared dark/high-contrast-aware main theme, guided welcome state, runtime/playback status, hover/focus controls and native themed sliders. Search existing commands with Ctrl+K; unavailable commands remain blocked. A title-filtered queue maps visible rows to real sources, supports native Enter/Delete/navigation, and separates Queue from live Adjustments. Compact resizing restores focus away from hidden controls. The README now includes versioned native screenshots, architecture/package-flow Mermaid diagrams, the actual technology stack, setup, limits and troubleshooting.

Use Ctrl+O to open files, Ctrl+K for commands and Ctrl+F for the full-width queue filter. Filtering does not reorder or stop media. A running enhancement preview retains its verified package until stopped. Import the new build's bundled libraries after upgrading rather than mixing workers or older package bytes.

## Verification and scope

All existing native/browser/library/decoder/renderer/installer gates remain. New portable tests cover title matching, privacy and queue index mapping; new Windows scenarios cover onboarding, command prerequisites/cancel/execute, filtered queue actions, tabs and compact-focus recovery. Screenshots show generated decoding fixtures—not trained neural output. Consult the published exact-commit reports for executed results.

This is still an independent CPU SDR comparison studio, not a primary DLSS video-output plugin or an audio-synchronized neural pipeline. No NVIDIA runtimes or model weights are bundled. No certificate-backed signing, physical-GPU/HDR/temporal/perceptual-quality certification or native macOS/Linux installer is claimed. Published earlier releases are not replaced.
