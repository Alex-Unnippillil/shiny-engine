# Library-management implementation status — 0.8

This implements the **local management and reference-backend path** of `VLC_LIBRARY_MANAGEMENT_PLAN.md`. The original plan and its release gates remain intact. It does not declare every vendor, hardware and distribution milestone complete.

| Area | Implemented in this update | Remaining boundary |
|---|---|---|
| Inventory | Existing read-only fixed-slot auditor retained | No whole-machine inventory or driver management |
| Catalog / quarantine | Persistent SQLite schema, strict JSON1 manifests, content-addressed copies, separate build-pinned approvals, history, verify/stage/remove | No signed remote catalog, downloader or vendor publisher allowlist |
| Safe file handling | Bounded copies, pinned file/ancestor handles, hardlink/reparse/remote-path rejection, named store mutex, shared/exclusive package leases | Not a hostile same-user/account security boundary |
| Recovery | Pending import/delete journals, atomic selected/previous database transaction, last-known-good rollback, restart recovery | Broad filesystem/power-loss/OS certification remains hardware testing; tests distinguish process death from actual power loss |
| Cold activation | Fixed isolated worker, build-bound executable integrity, worker-side bundle revalidation, versioned binary protocol, known-frame probe, no hot replacement | Only the two exact-build first-party reference DLLs implement this ABI |
| Real video | Independent libVLC software-decoded RGBA8 frames processed through either reference DLL, matched source/output, original playback preserved, bounded frame queue and cancellation | Not a primary-output plugin, not audio-synchronized, not GPU/zero-copy, not DLSS/VSR/HDR/temporal neural rendering |
| Native UX | Separate async manager window, typed CLI, player menu, independent preview controls, explicit selected-versus-active state | No automatic preview version switch or unattended update |
| Packaging | Manager, worker, two reference bundles, pinned SQLite notices, per-user player installer, checksums and exact-commit evidence | Unsigned prerelease; certificate-backed signing and production online update service absent |

## Mapping to the original gates

M1's offline first-party store and native-panel work is implemented. Imported vendor components are inspectable/quarantined, not authorized through editable metadata. The broad signed-catalog and publisher-governance path is not silently replaced by local consent.

M2 has an actual software reference-frame integration to exercise management semantics and preserve original playback. This is a deliberately labeled **reference adapter**, not completion of the plan's named-GPU/vendor-enhancement quality and performance gate. The existing OpenDLSS-NR local-model consent workflow is unchanged.

M3 is enabled only for that implemented, build-pinned reference ABI after a real isolated known-frame probe. It remains unavailable for NVIDIA DLSS/Streamline packages. Two compatible reference bundle versions can be selected and rolled back between worker lifetimes. A running preview holds its selected version's lease until stopped.

M4 has reproducible unsigned packaging and automated install/use/uninstall checks. Code signing, online update trust, broad upgrade/repair matrices, physical GPU/audio and long-session certification remain outstanding. No generic “DLSS enabled” or “production certified” state is emitted.

## Test evidence discipline

The portable suites cover PE policy, strict schemas, bounded wire formats, two independent fixed golden outputs, alpha preservation, malformed input, store transitions, tamper rejection and injected exception recovery. Windows suites separately exercise real DLL workers, filesystem leases, simultaneous managers, UI actions and **abrupt process termination** at import/selection/delete boundaries. The integrated player test uses an actual VLC-decoded fixture frame, both DLL versions, a still-running original decoder and rollback. Windows installer tests repeat package import/stage/known-frame selection from the installed location.

Use successful exact-commit GitHub Actions reports, not the existence of test source, as execution evidence. Failures block merging/release. The automated release policy now requires both library-manager matrix checks in addition to all existing browser, native Windows and VLC checks, and requires managed-video evidence and package payloads for 0.8+.
