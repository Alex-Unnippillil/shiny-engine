# Implementation status — Windows VLC 0.10.0 and Desktop 0.1.0

## Additive cross-platform Desktop edition

`native/desktop` implements a separate C++20 / Qt Widgets/Multimedia local-media player with Windows x64 Setup/ZIP, macOS arm64/x64 DMGs and Ubuntu 24.04 x64 DEB/TGZ packaging. It adds searchable queues, explicit atomic playlists/snapshots, transport/rate/repeat and embedded-track selection. It is not libVLC or a port of the Windows enhancement/model workers. The original Windows application and research/library plans remain unchanged.

Distribution is prerelease: unsigned Windows, ad-hoc-only macOS (not Developer ID/notarized), and dependency-managed Ubuntu rather than a universal Linux binary. Four native-host install/playback/remove checks and the existing five application check names gate the independent `desktop-v0.1.0` release. Qt is dynamically linked; dependency notices, source access and payload hashes accompany packages. Read [the Desktop guide](../native/desktop/README.md) for its feature differences and installation requirements. Successful exact-commit reports, not source tests alone, establish which targets passed.

The library-management update is described in [library-manager-status.md](library-manager-status.md): persistent local catalog/quarantine, cold selection and rollback of exact-build spatial reference DLLs, and an independent VLC-decoded comparison preview. This is not a new NVIDIA DLSS backend or completion of hardware/signing gates.

0.9 adds the themed Libraries and Video studio, four comparison modes, retained Direct2D presentation with GDI+ fallback, a bounded higher-resolution managed preview, and a third adaptive SDR spatial library. See `release-0.9.md`; no new DLSS, hardware or primary-audio claim is made.

0.10 modernizes the main workspace with a welcome state, quick-action search, title-filtered queue, dedicated adjustment view, shared theme and control-aware keyboard routing. See [workspace guide](workspace-guide.md). Processing and trust policies are unchanged.

# Prior 0.7.0 component baseline

The original research plan remains unchanged. Its proposed goals are not retroactively reported as completed.

## Delivered application components

| Area | Implemented | Unverified or deferred |
|---|---|---|
| Native player | Real libVLC playback, tracks/subtitles/equalizer, queues, transport, bookmarks, chapters, snapshots, Cinema and DPI-aware reflow | Full VLC UI parity, broad hardware/codec/network coverage, long-session and sound-device certification |
| Native NR | Pinned graph/worker, strict model-data intake, reviewed-only and explicit local-research modes, private frame protocol, sliders, matched-input composition and exports | Authorized trained-model runs and vendor parity, video-quality study, named-GPU performance, temporal reconstruction, main-audio synchronization, 4K/HDR |
| Browser | Spatial Media Lab, reviewed-only Neural Lab and LookLock still-frame protection | Universal site support, trained browser-model certification, HDR |
| Capture companion | Windows picker/D3D11 spatial preview and bounded native controls | Click-through overlay, D3D/Vulkan interop, interactive full-display/multi-monitor certification |
| Distribution | Per-user Windows x64 installer, optional verified VLC prerequisite, portable/source packages, checksums and exact-main gated releases | Code signing and updater; macOS/Linux installers now belong to the separate Qt Desktop edition above |

The owner requested a separate local educational experimentation path. A successfully validated user model can be prepared with explicit session acknowledgment even when it is absent from the curated registry. That does not add production approval or prove rights/correctness. This policy is documented in `research-mode.md`; browser approvals remain unchanged.

## Evidence

Only successful exact-commit CI and its retrieved reports count as completed release checks. Source tests themselves are not evidence of execution. Native CI compiles the real graph and executes VLC software decoding, model rejection, Windows UI, data-intake and installer tests. Synthetic residuals/zero tensor fixtures are clearly untrained. No such tests certify trained neural image quality or GPU speed.

0.1 introduced spatial baseline; 0.2 improved media lifecycle/UI; 0.3 added the browser NR adapter; 0.4 added LookLock; 0.5 added libVLC playback; 0.6 added the gated native workbench/installers; 0.7 introduces explicit local research and responsive native controls. Read the versioned release reports for verified execution rather than inferring it from this history.
