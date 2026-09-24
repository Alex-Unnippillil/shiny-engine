# Shiny Player 0.8.0 — local enhancement library management

Unsigned Windows x64 prerelease. This adds actual local package management and a **first-party spatial reference backend**, not new NVIDIA DLSS support. No proprietary runtime or model weights are bundled.

## Delivered

Persistent catalog/quarantine/history; strict bounded package manifests; exact-build first-party approval; file/ancestor locks and leases; staged selection through an isolated worker; independent revalidation and known-frame probes; two reference DLL versions; durable recovery and idempotent rollback; native asynchronous Libraries window and CLI; an independent, muted VLC-decoded source/output preview; integrated per-user installer and portable packages.

Use Tools > Enhancement libraries > Import bundled, then Verify, Stage and Select next preview. Open a local video and use Tools > Managed spatial preview > Start selected version. Main video/audio remain unchanged. Read the included `library-manager-guide.md` for status meanings, storage, cancellation, upgrade and uninstall behavior.

## Boundaries

Only exact-build conventional reference DLLs can run. NVIDIA/Streamline candidates remain quarantined without a compatible implemented adapter. Preview is CPU-copy RGBA8 SDR up to 512 pixels on the long side, independent of main audio. It is not DLSS, a primary video-output replacement, HDR, temporal neural rendering, or certified real-time/GPU performance. Existing authorized local-model research remains a distinct workflow.

No certificate-backed code signing, online catalog/updater, physical GPU/audio-device or long-session certification is claimed. The release includes tested binaries, hashes, source provenance and test reports; synthetic reference frames are not trained-model or NVIDIA quality evidence. Previously published 0.7 assets are not replaced.
