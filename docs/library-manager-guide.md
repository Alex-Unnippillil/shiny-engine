# Enhancement Libraries — local management guide

Shiny Player 0.9 adds a persistent native library manager and an independent video preview. It manages **three first-party spatial DLL versions**, not NVIDIA DLSS. Imported NVIDIA/Streamline files remain quarantined: the application does not yet contain a compatible vendor-DLSS video adapter. No vendor runtime, neural weight, online downloader or driver replacement is included.

## Use from the Windows player

Open **Tools > Enhancement libraries**. Choose **Import bundled**, select a reference version, then **Verify**, **Stage**, and **Select next preview**. Selection starts an isolated worker and checks a known frame before recording the selection. It does not alter an existing preview or any VLC installation.

Open a seekable local video, then **Tools > Managed spatial preview (not DLSS)**. Press **Start selected version**. The left and right images are the same decoded input frame before and after conventional sharpening. The preview becomes active only after an actual processed frame returns. **Stop preview** closes its worker; the main player retains its own video and audio. The main player's Stop, source replacement and window close also tear down the preview.

This is an independent, muted, CPU-copy RGBA8 SDR comparison at up to 960 pixels on the longest side, within 518,400 pixels (720 × 720 for square images). It is not the primary player's video output, not synchronized to primary audio, not a zero-copy GPU implementation, and not certified real-time, HDR, temporal reconstruction or recovered detail. The displayed timing is CPU wall-clock worker round-trip time, not GPU execution time. Text and burned-in subtitles are part of the decoded picture; it is not a subtitle-aware restoration model.

Choose another package in the manager and reopen/start a preview to apply it. A running worker keeps its original verified files pinned and its package leased. **Roll back** selects the previous known-good package after another probe; repeating rollback is a no-op, not a version toggle. **Use original** clears the future-preview selection. Close an already-running independent preview to stop that worker immediately.

## Video studio and Adaptive detail (0.9)

The main toolbar now exposes **Video studio** and **Libraries**. The studio has
side-by-side, wipe, source-only and processed-only views. Drag the wipe divider or
focus the image and use Left/Right, Home/End. **Inspect 1:1** centers and crops the
decoded preview at one image pixel per display pixel; it does not decode the original
file at full resolution. **Compatibility renderer** switches from retained Direct2D
presentation to GDI+; **Use Direct2D** recreates the presentation resources.

Library **1.2.0 / Adaptive detail** applies a noise threshold to encoded SDR luma,
limits detail changes to 12 code values, preserves RGB-channel differences using a
shared offset, clamps to local/gamut bounds and leaves translucent neighborhoods
unchanged. It does not denoise, upsample, recover detail or run a neural network.
Versions 1.0.0 and 1.1.0 remain available for comparison and rollback.

The p95 metric is a rolling window of up to 120 completed worker round trips.
Superseded inputs count overwritten pending worker inputs, not every skipped decoder
frame. Direct2D may use a hardware or software device. Frame processing still uses
CPU copies. Windows screenshots use a compatible GDI+ paint path and do not establish
hardware acceleration. The manager honors system high-contrast colors and preserves
native control labels, keyboard navigation and focus indicators.

## Import and trust

**Import folder** accepts a strict `manifest.json` and exactly the listed recognized DLL files. **Quarantine DLL** wraps one recognized candidate in an unapproved local manifest. Neither operation executes the file, marks it vendor-approved, or modifies installed VLC. These copies stay quarantined unless their complete manifest and file bytes match the first-party catalog compiled into this exact application build.

The manager does not trust an editable `approved` field, a filename, a version resource or an old audit JSON. Every stage and launch rehashes the complete bounded bundle. The worker independently repeats validation before loading its fixed `shiny_spatial.dll` entrypoint. The client also checks the worker executable against its compiled build hash. Only that known ABI is implemented. There is no arbitrary DLL-load, shell or executable-path RPC.

The embedded catalog is a build-pinned first-party policy, **not a signed online update catalog or an Authenticode publisher approval**. The separate read-only `ShinyLibraryAudit` remains available for cached signature inspection. A cached valid signature does not prove backend compatibility, redistribution rights, or current online revocation status. Production code signing and an authenticated online update channel remain separate release work.

## CLI

The GUI and CLI share the same services. The application itself does not require Python, Node or .NET.

```powershell
.\ShinyLibraryManagerCli.exe --import-bundled
.\ShinyLibraryManagerCli.exe --list
.\ShinyLibraryManagerCli.exe --verify <64-character-package-digest>
.\ShinyLibraryManagerCli.exe --stage <64-character-package-digest>
.\ShinyLibraryManagerCli.exe --activate <64-character-package-digest>
.\ShinyLibraryManagerCli.exe --probe <64-character-package-digest>
.\ShinyLibraryManagerCli.exe --rollback
.\ShinyLibraryManagerCli.exe --original
.\ShinyLibraryManagerCli.exe --history
.\ShinyLibraryManagerCli.exe --recover
.\ShinyLibraryManagerCli.exe --remove <unselected-unleased-package-digest>
```

Use `--store-dir "C:\LocalFolder\LibraryStore"` before the command for an explicit separate local store. The parent must already exist. The GUI accepts the same optional `--store-dir` argument. Ordinary player previews use the default store. Do not select system or third-party application directories as a store. A nonempty unmanaged directory is refused.

Exit 0 means the operation completed; inspect `blockReason` rather than assuming approval. Exit 2 means the operation failed. `--probe` reports a **synthetic test-frame** result, not active playback. `--list` reports a next-preview selection with `active:false`; only the preview UI can report processing in its own live session. Reports and history use package digests and fixed status codes, not full local paths or media URLs.

## Storage, recovery, cancellation and removal

The default store is `%LOCALAPPDATA%\ShinyPlayer\LibraryStore`, outside the installed program files. It contains a SQLite catalog/history, `staging`, `packages/<manifest-digest>` and lease files. A complete package is identified by the SHA-256 of its exact manifest bytes; its manifest binds every component's size and hash. Reformatting a manifest changes its identity and does not preserve build approval.

Import writes a durable pending journal, creates bounded staging files, flushes them, revalidates the staged bytes, promotes the directory, and commits the catalog row. On next open, incomplete staging is removed and completed promotions are reconciled. Selection and pending-journal removal share a SQLite transaction; an interrupted selection keeps the previous committed selection. Removal uses an exclusive package lease, never recursive deletion through user-controlled paths. Selected and rollback packages are protected.

The store serializes management operations using a named mutex derived from Windows directory identity, including path aliases. File and ancestor-directory handles resist replacement, and readers deny write/delete sharing while validating or executing. Hardlinks, reparse points, UNC/device/mapped-network paths, alternate streams, unsafe manifest names and unexpected package contents are rejected. Limits include 16 components, 32 MiB per component, 128 MiB per package, 64 packages and a 2 GiB declared store budget. No archive extraction or remote downloads are accepted by the runtime manager.

Hashing, validation and probes run off the GUI thread. **Cancel operation** requests cancellation; a private worker is terminated on cancellation or bounded IPC timeout. A failed candidate leaves original playback available. A corrupt or unexpectedly redirected store fails closed rather than deleting unknown content. Restore an intact store backup or use a new empty store when manual repair is necessary; do not bypass validation.

These controls do not create a security boundary against a hostile process already controlling the same user account or installation. The application runs without elevation. Windows ACL/account protection and eventual signed distribution remain relevant.

## Upgrade and uninstall

Installer 0.9 uses the existing per-user application ID and includes the manager, worker and all three exact-build bundles. No released 0.7 asset is overwritten. A rebuilt application can have different package hashes: previously imported bundles do not automatically gain approval in the new build. Import the new bundled versions, inspect/stage/select them, or choose Original. A stale selection fails closed in the worker and does not prevent ordinary VLC playback.

Uninstall removes installed program binaries and bundled reference files but deliberately retains the separate user library store. Do not automatically delete user-imported files during uninstall. Package removal is available in the manager for unselected, unleased packages; back up the store before manual deletion.

## Build and validation

Use Visual Studio C++ tools, CMake 3.24+, Python 3 and the Windows SDK. The pinned SQLite source is public domain; the fetch script verifies the official SHA3-256 before extracting fixed filenames. The source build generates approvals from its own three first-party DLLs and embeds the final worker hash into its clients. Do not mix clients, workers and bundles from different builds.

```powershell
python scripts/fetch-library-deps.py
cmake -S native/library-manager -B build/library-manager -A x64 "-DSHINY_SQLITE_SOURCE=$PWD/.deps/sqlite"
cmake --build build/library-manager --config Release --parallel
ctest --test-dir build/library-manager -C Release --output-on-failure
```

The integrated player build is `scripts/Build-Windows.ps1`. GitHub Actions exercises the manager's real Windows processes, known-frame DLL probes, all three versions, package leases, process-death recovery, CLI and native UI, as well as libVLC-decoded frames and installer use. Synthetic reference checks do not certify NVIDIA, trained models, hardware acceleration, physical audio devices or long-session quality.
