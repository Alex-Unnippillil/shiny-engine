# Changelog

## Unreleased — VLC library-management foundation

- Added an independent, read-only Windows x64 library auditor: fixed VLC/DLSS/Streamline slots, bounded PE metadata, SHA-256, offline Authenticode status and path-redacted JSON. Inspection never loads a candidate or modifies installed VLC.
- Added portable policy/parser coverage and Windows filesystem, execution-canary and privacy tests, with a dedicated utility build/artifact workflow.
- Added [the codebase analysis and staged development plan](docs/VLC_LIBRARY_MANAGEMENT_PLAN.md) for trusted catalogs, video backends, cold activation and rollback.
- DLL swapping, executable import, GUI management and a new enhancement backend are **not implemented** by this milestone. Existing 0.7.0 player installers and local research behavior remain unchanged. See [the audit guide](native/library-manager/README.md).

## 0.7.0 — local research and release hardening

- Added a distinct opt-in local OpenDLSS-NR research route for authorized user-supplied model data. Curated production approvals remain unchanged.
- Validated strict model schemas, tensor layouts, SHA-256 identities and replacement-resistant Windows file/directory locks before native graph loading.
- Added numerical tone, structure and mix controls, matched-input comparison modes, pause/reset, explicit PNG/report export and stale-result handling.
- Made the Windows player DPI-aware with reflowing controls and queue access in compact windows. Expanded cancellation and owner-window lifetime protection.
- Tested actual Windows decoding, model-data intake, picker cancellation, compact layout and installer playback/uninstall. No trained-model inference or physical-GPU certification is claimed.
- Prevented browser transport from accepting play/pause while initial rendering/autoplay is pending; added a deliberately delayed-GPU regression.
- Added draft-first release publication with exact-run identity, archive/portable integrity, remote asset digest verification and immutable published versions.

This is an **unsigned Windows x64 prerelease**. The native experimental preview is independent SDR processing, not audio-synchronized full-resolution neural playback. Model weights are not supplied.

## 0.6.0

Native OpenDLSS graph worker and gated workbench, expanded VLC controls, per-user Windows setup and portable distribution.

## 0.5.0

Native libVLC player with real playback, conventional image adjustments and externally processed-video comparison.

## 0.4.0

LookLock source-preserving still-frame composition and review workflow.

## 0.3.0

Separately gated browser OpenDLSS-NR adapter and Neural Lab.

## 0.2.0

Media Lab playback, comparison, transactional source loading and usability improvements.

## 0.1.0

Local spatial-enhancement baseline and Windows capture companion.
