# VLC enhancement library management — development plan

**Date:** 23 September 2026. **Analyzed baseline:** `cdf65ab2040f987563012619fd64435fe9ce85d4` (Shiny Player 0.7.0). **Scope:** a DLSS Swapper-inspired manager for the VLC-based player, not game discovery or executable injection. This supplements, and does not rewrite, `RESEARCH_AND_IMPLEMENTATION_PLAN.md` and `implementation-status.md`.

## 1. Decision and delivery boundary

Build a **compatibility-aware enhancement package manager**, not a blind DLL replacer. Preserve the installed official VLC and the working libVLC playback path. Prefer switching a validated, application-managed backend bundle between process lifetimes over overwriting DLLs in a VLC directory.

This update implements the first milestone: `native/library-manager/ShinyLibraryAudit`, a read-only Windows command-line inventory and candidate inspector, with portable policy tests and Windows integration tests. It returns bounded, path-redacted metadata and deliberately reports `swapSupported=false`, `activationSupported=false`, and `dlssBackendAvailable=false`. It neither imports nor executes candidates. There is no new GUI, runtime switcher, installer release, or DLSS playback implementation in this milestone. The existing player, its research mode and 0.7.0 release remain unchanged.

The following sections distinguish this delivered foundation from proposed work. A successful compile or a recognized DLL filename must never be used as evidence of functioning video enhancement.

## 2. What the existing code actually provides

| Existing area | Finding | Development consequence |
|---|---|---|
| `native/vlc-player/engine.cpp` | Discovers a portable runtime or installed VLC; requires executable, core DLLs and plugins; restricts DLL search; resolves the libVLC API; accepts the supported 3.0 series and rejects the different VLC 4 ABI. | Keep this loader separate from enhancement packages. Never replace `libvlc.dll` alone or treat VLC 4 as a compatible DLL update. |
| `native/vlc-player/engine.cpp`, `actions.cpp` | Real playback and video controls; optional D3D11 super-resolution request is applied when opening media and labeled unverified. Video output is assigned with `SetHwnd`. | The manager does not own the decoded GPU texture/presentation pipeline. A renderer integration spike is required; no implicit zero-copy claim. |
| `native/vlc-player/nr_client.cpp`, `nr_panel.cpp`, `native/nr-worker` | Separate native OpenDLSS-NR research workflow; explicit local-model consent and curated approval are different routes. | Preserve model-data intake and cancellation. Do not feed executable libraries into the model picker or convert user-supplied files into curated approval. |
| Browser applications and native messaging | Existing local-media, research and capture workstreams have separate contracts and safety requirements. | A browser page must not receive arbitrary path, DLL-loading, process-launch or shell commands. The first manager is native and offline. |
| `installers/windows/player.iss` | Per-user setup; optional hash-verified official VLC archive; no system VLC overwrite or required elevation. | Preserve least privilege. Future backend bundles belong under application-managed local storage, not Program Files, System32 or the driver store. |
| `.github/workflows/vlc-player.yml` | Pinned VLC/NR source, Windows builds, actual software playback, UI, model intake and installer tests. | Retain these regression gates. Add library-management tests rather than replacing existing checks with easier mocks. |

Source baseline: [player loader](https://github.com/Alex-Unnippillil/shiny-engine/blob/cdf65ab2040f987563012619fd64435fe9ce85d4/native/vlc-player/engine.cpp), [actions](https://github.com/Alex-Unnippillil/shiny-engine/blob/cdf65ab2040f987563012619fd64435fe9ce85d4/native/vlc-player/actions.cpp), [implementation status](https://github.com/Alex-Unnippillil/shiny-engine/blob/cdf65ab2040f987563012619fd64435fe9ce85d4/docs/implementation-status.md), [Windows CI](https://github.com/Alex-Unnippillil/shiny-engine/blob/cdf65ab2040f987563012619fd64435fe9ce85d4/.github/workflows/vlc-player.yml).

## 3. What to adapt from DLSS Swapper

The inspected upstream has typed DLL records, a catalog manager, separate imported manifests, game/provider discovery, backup asset types and swap/history logic. Its README explicitly distinguishes upgrading an existing integration from adding DLSS to software that does not support it. Use these architectural ideas, not game-specific scanning or executable patching.

| Swapper pattern | VLC adaptation |
|---|---|
| Game/library providers | Explicit selected VLC installation and the player's own managed runtime; optional later read-only registry discovery. |
| DLLRecord / typed asset families | Runtime records keyed by family, architecture, SHA-256, source and compatible backend bundle. |
| Curated versus imported manifests | Signed release catalog versus quarantined local inspection records. Import is not approval. |
| Version selection | Compatible bundle selection, not an unrestricted newest-DLL button. |
| Backup and history | Immutable previous bundle plus transaction history and last-known-good activation record. |
| Swap / restore | Stage, verify, stop the affected worker, cold activate, smoke-test, commit or roll back. |

Keep the C++20/Win32/CMake stack already used by Shiny. Reimplement these limited workflows rather than porting the C#/XAML application or adding .NET solely for management. Upstream uses GPLv3; this MIT addition copies no upstream implementation. Any later code reuse requires a component-specific licensing review and appropriate distribution obligations. Proprietary runtime/model rights are separate from a utility's source-code license.

References: [upstream README](https://github.com/beeradmoore/dlss-swapper/blob/ab9b1e2d4bb04187c48fe58eeea06c2b0ea71fbb/README.md), [DLLManager](https://github.com/beeradmoore/dlss-swapper/blob/ab9b1e2d4bb04187c48fe58eeea06c2b0ea71fbb/src/Data/DLLManager.cs), [GameAsset](https://github.com/beeradmoore/dlss-swapper/blob/ab9b1e2d4bb04187c48fe58eeea06c2b0ea71fbb/src/Data/GameAsset.cs), [license](https://github.com/beeradmoore/dlss-swapper/blob/ab9b1e2d4bb04187c48fe58eeea06c2b0ea71fbb/LICENSE). This was targeted architecture inspection, not an exhaustive upstream security audit.

## 4. Separate three different enhancement routes

**Game-oriented DLSS SR/FG/RR:** the documented Streamline DLSS Super Resolution integration expects depth, motion vectors and color buffers, initializes around the graphics device lifecycle, and performs per-adapter support checks. Ordinary decoded movie frames do not supply the original renderer's depth and motion-vector buffers. Estimated video motion is not interchangeable with that information. Copying `nvngx_dlss.dll` beside `vlc.exe` does not create these calls or resources. SR, frame generation and ray reconstruction are also distinct families; do not exchange their filenames. This SR requirement must not be generalized into an unsupported claim about every neural-rendering model.

**Video-native enhancement / driver VSR:** retain the current VLC D3D11 request as a separate feature. Evaluate NVIDIA's currently offered AI-for-Media / video SDK route for direct video enhancement, with explicit SDK access, license, API, hardware and redistribution review. The former RTX Video SDK page redirects to AI for Media; do not assume a freely redistributable SDK package or an interchangeable DLSS DLL. Driver-controlled VSR is not managed by swapping game libraries, and a requested setting is not proof that the driver processed frames.

**Existing OpenDLSS-NR research:** continue this as an independent experimental model backend. Its authorization, model format, graph, temporal behavior and output-validation questions remain separate from the executable library catalog. Do not bundle leaked/vendor runtimes or models, and do not describe independent-frame research as certified real-time DLSS video playback.

Primary references: [Streamline DLSS integration guide](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS.md), [NVIDIA AI for Media](https://developer.nvidia.com/topics/ai/generative-ai/ai-for-media), and the repository's existing research-mode and implementation-status documents. Pin the exact SDK/source revision at the implementation milestone; moving documentation is not a dependency lock.

## 5. Proposed architecture and contracts

```text
Native Libraries panel / read-only CLI
    -> InventoryService and ReportService
    -> CompatibilityPolicy (deny by default)
    -> Catalog + quarantined PackageStore
    -> ActivationCoordinator + transaction journal
        -> existing libVLC playback (always available)
        -> version-pinned enhancement worker / supported renderer adapter
```

Use the new `catalog.hpp` for fixed inspection families and `audit.cpp` for read-only observations. Extract reusable services after their Windows tests pass. Add a small native UI adapter without changing the playback engine's responsibilities. Introduce SQLite only when persistent catalog/history/transactions are implemented; it is unnecessary for this stateless audit. Use a maintained, pinned JSON/schema implementation for larger manifests instead of extending manual serialization into an arbitrary parser. Reuse Windows BCrypt, Authenticode and restricted loader APIs rather than inventing cryptography.

Proposed records should contain package ID, immutable digest, component family, machine architecture, exact file list and hashes, backend ABI, dependency-set ID, tested VLC versions, GPU/API requirements, minimum driver/OS where applicable, source/provenance, publisher policy, license disposition, approval channel and evidence references. File-version resources are display metadata, not identity. A valid signature does not by itself establish the intended publisher, redistribution rights or API compatibility.

Separate status fields: `present`, `integrityVerified`, `publisherApproved`, `backendCompatible`, `hardwareSupported`, `requested`, `active`, and `lastError`. Only a worker/backend response tied to the selected package and session may report active processing. Unknown, unavailable and failed must remain distinct from off. The current audit does not populate these future approvals.

Use opaque package/session IDs over any future IPC. Resolve paths inside the native manager after native user selection and consent. Allowlist commands such as inventory, inspect-selected-file, stage-approved-package, activate-package, rollback and cancel. Never add a generic `loadDll(path)` or arbitrary shell endpoint. Do not accept a caller-supplied manifest claiming to enable an absent backend.

## 6. Package storage, trust and compatibility

Proposed storage is `%LOCALAPPDATA%\ShinyPlayer\backends\<backend-id>\<content-digest>`, with separate staging, catalog and journal locations. Digests and IDs must be validated strings, not path fragments supplied by a webpage. Keep complete approved dependency sets together; Streamline plugins and vendor runtimes must not be mixed indiscriminately. Treat libVLC plus libvlccore and its compatible plugins as a separate complete runtime bundle.

For a future import, copy authorized local files into quarantine using bounded handle-based I/O. Reject reparse points, hardlinks, archive traversal, absolute archive names, alternate streams, duplicate/case-colliding entries, unexpected executable names and excessive expanded sizes. Hold and verify file/directory identities across the transaction, including parent components. Hash the final staged bytes, inspect architecture/import/export metadata without execution, check Authenticode and an explicit publisher policy, validate provenance and approved dependency compatibility. An offline unknown trust result is not approval. Check online revocation only under a disclosed, separate network policy.

Future downloads must be explicitly requested from an approved source catalog with signed metadata, expiration/version rules, pinned hashes, size/time limits and redirect allowlists. Do not automatically consume community mirror manifests as trusted executable updates. Do not distribute components until the selected license permits it. No downloader is included now.

The current auditor uses a cache-only [WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust) result, not an executable allowlist. Recheck everything at activation; old audit JSON must never become an authorization token.

## 7. Cold activation, rollback and crash recovery

Implement the state machine `discovered -> quarantined -> verified -> staged -> pending -> active`, with `blocked`, `failed` and `rolled-back` outcomes. Verification and staging alone must not affect playback.

Before activation, acquire a per-user named mutex and a package lease, snapshot the last-known-good selection, and write a durable pending journal. Revalidate all package identities and compatibility. Stop only the affected enhancement worker; keep normal playback and its audio available where the adapter allows it. Never overwrite a mapped DLL or unload a library while callbacks or GPU work can still reference it.

Launch a fresh worker with a fixed executable and a validated package ID. Use a restricted absolute DLL search path inside that worker, no inherited arbitrary search directories, and no shell. Require protocol/ABI handshake, exact loaded-bundle identity, adapter support and a bounded test frame. Then atomically replace the small active-selection record, mark the journal committed, and retain the previous immutable bundle. SQLite transactions cannot make external filesystem operations atomic: explicitly order flushes, renames and journal states, and test every crash point.

On timeout, failure or next-start detection of an uncommitted journal, terminate the failed worker, restore the previous selection, and use original playback. Rollback must be idempotent and must work after reboot. Garbage collection must not delete active, leased or rollback packages. Display understandable failure reasons and provide a visible restore-original action.

Acceptance requires fault injection for crashes after every transition, locked files, full disk, interrupted copy, two manager processes, stale records, tampered bytes and worker death. Do not add hot swapping just to imitate a game utility's button.

## 8. Rendering integration workstream

Library management and usable enhancement must converge only after a renderer exists. First prove a passthrough pipeline with timed frames and original-output fallback. For the current libVLC 3 path, investigate a pinned video-output/filter plugin or a deliberately separate supported integration. A VLC 4 experiment must use its own headers, ABI, packaging and tests; it is not a drop-in replacement for the existing loader.

The target pipeline is decoded surface -> explicit color/range conversion -> supported backend -> comparison/subtitle composition -> presentation. Retain libVLC as demux/decode/audio clock owner where feasible. Carry PTS, frame duration, geometry, color metadata and session generation through every stage. Preserve exactly one audible path. Bound queues and drop obsolete work rather than accumulating latency. Release textures only after consumers and GPU fences complete.

Reset temporal history on seek, source replacement, scene cut, resize and device loss. Handle subtitles, burned-in text, aspect ratio, interlacing and variable frame rates deliberately. Begin with SDR; HDR transfer functions, mastering metadata, tone mapping and display output are separate tests. Test D3D11/Vulkan surface sharing, same-adapter matching and synchronization before claiming zero-copy integration with the research worker. Record CPU readback/copy cost when it remains.

A backend contract should expose `probe`, `prepare`, `process`, `resetHistory`, `flush` and `release`, with versioned capabilities and typed errors. Measure frame latency, p95 pacing, dropped frames, VRAM, A/V skew and quality on named hardware. SDK initialization or loading success alone is not enhancement proof. Estimated motion/depth experiments require a separately documented research route, not automatic production DLSS compatibility.

## 9. Native UX and integration strategy

Add an Enhancement Libraries panel after service APIs stabilize. Show the selected VLC runtime, current backend, candidate versions, compatibility reasons, provenance and last-known-good package. Actions should evolve from Inspect and Export report to Import into quarantine, Stage, Activate after restart, and Roll back. Unsupported candidates remain inspectable but activation stays disabled with an explicit reason.

Do filesystem hashing and trust checks off the UI thread with cancellation and generation checks. Bind confirmations to the inspected digest, not a mutable path. Do not block Stop or source playback while inspecting. Avoid automatic disk-wide game discovery; explicit VLC selection is the initial contract. Keep absolute paths local to a user-requested detail view; default diagnostics export only relative slots, hashes and status codes.

The first CLI is intentionally useful without a UI rewrite. A later panel can invoke the same native services, not parse shell commands or trust external report text. No browser CSP/permission changes are needed for this milestone.

## 10. Milestones and completion gates

| Milestone | Deliverable | Completion gate |
|---|---|---|
| M0 — this update | Fixed-slot inventory, recognized local candidate inspection, bounded PE metadata, SHA-256, offline signature status, redacted JSON; plan and CI. | Linux policy tests and real Windows inspection tests pass; no candidate execution or installation mutation; existing repository checks remain green. |
| M1 — trusted catalog/store | Persistent quarantine, exact manifests, dependency groups, provenance/publisher policies and native panel. | Corrupt/untrusted imports cannot become executable; crash-safe staging, path attacks and cancellation tested; no backend activation yet. |
| M2 — renderer feasibility | Passthrough timed-frame path and one supported video enhancement adapter with verified SDK/model rights. | Actual frames processed on named hardware, stable playback/audio, fallback, quality and performance evidence. |
| M3 — managed switching | Cold activation, package leases, durable journal, last-known-good rollback and restart UX. | Two compatible, authorized bundle versions switch and recover without modifying installed VLC; every failure boundary tested. |
| M4 — release hardening | Signed Windows packages, reviewed update channel, SBOM, notices, migration/repair/uninstall and support docs. | Clean-install/upgrade/rollback certification and exact-main build provenance; no unverified hardware or DLSS claims. |

M1 and M2 can proceed independently under separate file ownership. One integrator owns shared contracts, dependency locks, workflow changes and release metadata. Do not merge M3 activation without both trust and backend gates. Existing local-model research consent must not be silently tightened into curated-only operation or weakened into approval.

## 11. Verification and release policy

The new tests use synthetic PE bytes and a compiled canary DLL whose process-attach routine exits the audit child if executed. Portable tests cover parser bounds and deterministic mutations; Windows tests cover real file APIs, SHA-256, malformed images, architecture mismatch, junctions, hardlinks, sharing locks, ambiguous/remote paths, fixed scan scope, privacy and unchanged input files. The canary is test-only and never packaged as a runtime.

Retain `npm run check`, `npm run test:browser`, Windows companion builds and VLC/NR/player/installer tests. New utility CI builds both Linux policy tests and Windows x64 code with warnings treated as errors, and publishes only the auditor, documentation, source identity and checksums. Report successful executed results, not merely test counts in source. GPU, driver-VSR, real trained-model, temporal and long-session claims require separate hardware reports.

The main-branch source update does not retag or replace the 0.7.0 installer. The auditor is an unsigned CI artifact until its own release requirements are met. Never publish a green status that conceals unsupported swapping, and never describe this milestone as a completed VLC DLSS integration.
