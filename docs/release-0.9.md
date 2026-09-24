# Shiny Player 0.9.0 — Video studio and adaptive detail

Unsigned Windows x64 prerelease. Local processing, no proprietary runtime or weights.

## Delivered

A coherent dark native Libraries window with package cards, readable trust details,
keyboard focus and high-contrast system colors; direct Video studio/Libraries toolbar
access; a DPI-aware comparison studio with side-by-side, draggable/keyboard wipe,
processed-only, source-only and decoded-pixel 1:1 views.

The studio uses retained Direct2D bitmap resources and premultiplied-alpha uploads.
Direct2D chooses its available rendering device; GPU acceleration is not guaranteed.
A user-selectable GDI+ compatibility renderer and device-resource recreation retain
usable output. Screenshot-compatible paint uses GDI+, not proof of GPU rendering.

A third build-pinned library (1.2.0, Adaptive detail) adds noise-gated SDR luma detail,
a bounded common RGB offset, local-extrema/gamut limiting and transparency protection.
The earlier reference versions remain available and rollback remains supported.
The new library is tested on opaque numerical fixtures before selection, in addition
to the existing translucent known-frame probe. This is a conventional spatial filter,
not DLSS, super-resolution, denoising or recovered scene detail.

Managed decoding now allows up to 960 pixels on the long edge within a 518,400-pixel
budget (720 square). The neural workbench retains its 512-edge default. Studio metrics
show a bounded rolling p95 of worker round-trip latency and superseded queued inputs,
not GPU timings or the total decoder-drop count. Primary video and audio are unchanged.

## Use

Open Video studio from the player toolbar. In Libraries, Import bundled, select
Adaptive detail 1.2.0, Verify, Stage, then Select next preview. Start selected version
in the studio. Wipe supports dragging, Left/Right, Home/End; Inspect 1:1 shows preview
pixels, not full source-resolution pixels. Stop remains immediately available.

Do not mix clients, workers or library bundles from different builds. Re-import the
new bundled packages after upgrading. Old packages never gain approval automatically.

## Boundaries

Independent muted SDR comparison, not primary-output/audio synchronization, HDR,
temporal neural processing or a zero-copy GPU filter. NVIDIA/Streamline imports remain
quarantined. No signing certificate, remote signed catalog, physical-GPU qualification
or photographic quality benchmark is claimed. Existing 0.8 release assets are not replaced.
