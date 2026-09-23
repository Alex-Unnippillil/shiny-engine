# Shiny Library Audit — first library-management milestone

This native Windows x64 **read-only** utility inventories VLC and recognized DLSS/Streamline filenames without executing them. It is the foundation for the [VLC library-management plan](../../docs/VLC_LIBRARY_MANAGEMENT_PLAN.md), **not an implemented DLL swapper or a new DLSS playback backend**. The packaged artifact also contains `VLC_LIBRARY_MANAGEMENT_PLAN.md` beside this guide. It is separate from the existing 0.7.0 player; that release and its installers are not replaced by this source update.

## Build and run

Use Visual Studio's C++ tools, CMake 3.24+, and Python 3 for tests. The application itself has no Python, Node, .NET, or downloaded SDK runtime dependency. It uses Windows BCrypt, file-version metadata, and WinVerifyTrust APIs.

```powershell
cmake -S native/library-manager -B build/library-audit -A x64
cmake --build build/library-audit --config Release --parallel
ctest --test-dir build/library-audit -C Release --output-on-failure

.\build\library-audit\Release\ShinyLibraryAudit.exe --vlc-dir "C:\Program Files\VideoLAN\VLC"
.\build\library-audit\Release\ShinyLibraryAudit.exe --candidate "C:\AuthorizedLibraries\nvngx_dlss.dll"
.\build\library-audit\Release\ShinyLibraryAudit.exe --vlc-dir "C:\Program Files\VideoLAN\VLC" > "$HOME\Desktop\vlc-library-audit.json"
```

`--candidate` is inspection, not import or permission to use the file. The path must name an existing local file matching a catalog filename. No vendor libraries are provided. Do not download DLLs from issue comments or unidentified mirrors. Save redirected output **outside** the inspected installation: shell redirection is controlled by the caller, not the auditor.

The `VLC library audit` GitHub Actions workflow builds and tests the utility and uploads `ShinyLibraryAudit-Windows-x64`, containing the executable, guide, plan, source SHA and hashes. This is an unsigned CI utility, not a signed installer or an automatic release. The synthetic test DLL is deliberately excluded.

On Linux, CMake builds only the portable metadata/policy tests. Windows filesystem, BCrypt and signature tests require Windows; Linux tests do not validate those paths.

## What a report means

Schema version 1 reports twelve **fixed relative slots**, never a recursive or whole-machine scan: VLC's executable/core libraries/D3D11 output plugin, the SR/FG/RR NVIDIA filenames, and the listed Streamline files. Absence from those slots does not establish absence elsewhere.

`vlcLayoutDetected` means the three core filename/PE-kind/x64 markers were found, not that plugins, authentic provenance or the runtime ABI were verified. `runtimeCompatibility` remains `not-established`. Ordinary x64 PE metadata is insufficient to certify a usable Windows binary.

Each record includes an SHA-256 fingerprint, architecture, byte count, untrusted file-version string, and offline Authenticode result. `valid-cached-chain-not-publisher-approved` means the Windows cache-only check succeeded; it is **not** NVIDIA publisher approval, current online revocation assurance, redistribution clearance, or evidence of VLC support. `unverified-*` may indicate missing cached trust information as well as an invalid signature. No unverified result is promoted to trusted.

`activationAllowed`, `swapAllowed`, `activationSupported`, `swapSupported`, and `dlssBackendAvailable` remain **false** regardless of filename, version, hash or signature. Driver VSR is separate from DLSS runtime swapping. OpenDLSS-NR model data continues through the existing research-mode intake, not this DLL catalog.

Exit **0** means the audit completed, including missing or rejected file records. Inspect their statuses; it does not mean a candidate passed security review. Exit **2** means invalid command/root or a fatal audit failure. Errors omit local paths. Reports contain no media names, URLs or full filesystem paths.

## Bounds and safety

The process reads files with no write/delete sharing, rejects reparse components and hardlinks, rejects UNC/device/mapped-network/alternate-stream/ambiguous paths, and bounds work to 128 MiB per file and 512 MiB per audit. It uses a bounded PE metadata parser before version/signature inspection. No candidate is passed to `LoadLibrary`, no installation or registry is written, no driver is replaced, and no administrator elevation is requested. WinVerifyTrust uses cached URL retrieval only; there is no application downloader or telemetry.

These are inspection safeguards, not a security boundary against another process already running as the same user. Future activation must independently reverify pinned bytes, publisher and backend compatibility in its own transaction; it must never trust an old JSON report as authority.

## Tests

Portable tests cover catalog classification, blocked policies, x64/x86/ARM64, executable/DLL differences, malformed/truncated/overflowing PE fields, section bounds and JSON escaping, plus deterministic header mutations. Windows tests cover SHA-256, file limits, invalid paths, junctions, hardlinks, write locks, inventory scope, no installation writes, privacy, architecture mismatch and a compiled execution-canary DLL. None of these tests constitutes GPU inference, trained-model image-quality, VSR activation or DLSS certification.
