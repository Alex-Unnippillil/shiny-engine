# Optional DLSS 5 Swapper — external Windows tool

Available in Shiny Player **0.10.1** and Shiny Desktop **0.1.1** on Windows. This is an opt-in external-process launch bridge. It does not integrate NVIDIA neural rendering with either video pipeline.

## Use it deliberately

Open **Tools → Optional DLSS 5 Swapper (external)**. The modeless window does not pause or disable primary playback. Nothing launches on startup, when opening media, or just by opening this panel.

1. Obtain a tool you are authorized to use from its own project. The **DLSS 5 project** and **Classic project** buttons open only their fixed GitHub project pages in your browser. Shiny does not download, install or redistribute either application.
2. Use **Choose executable** to select an installed application or portable EXE—not its Setup installer. The file must be a local, regular Windows x86 or x64 executable, at most 512 MiB. The displayed architecture describes the launcher, not a contained payload or GPU compatibility. Select the whole installed/portable tool as supplied by its author; renaming an unrelated executable is not verification.
3. Review the displayed path, byte count and SHA-256 fingerprint. Inspection does not execute the candidate. The checkbox remains unchecked. A fingerprint identifies bytes; it is **not** malware scanning, a publisher signature check, license verification or backend compatibility approval. Companion files are not verified.
4. Acknowledge third-party execution and click **Launch external tool**. Shiny reopens the file with its parents pinned, rehashes it and rejects a changed file. Consent applies once and is cleared, whether launch succeeds or fails. A process-start result is not a Ready or DLSS-active result.

**Cancel check**, **Forget selection**, cancelling a new picker, and closing the panel revoke pending consent/selection. Selection is never stored in settings, the package catalog or browser state. An external tool already started is independent: closing Shiny or pressing Stop does not terminate it or undo its changes. Use the external tool's own backup/restore controls where applicable.

## Recognized tools and names

| Tool | Project | Recognized application filenames |
|---|---|---|
| DLSS 5 Swapper, community project | `rakanki911/DLSS5-Swapper` | `DLSS 5 Swapper.exe`, `DLSS5-Swapper.exe`, `DLSS5-Swapper-<major.minor.patch>-portable.exe` |
| Classic DLSS Swapper | `beeradmoore/dlss-swapper` | `DLSS Swapper.exe`, `dlss-swapper.exe` |

Recognition is case-insensitive and only reduces accidental selection. It never marks a file safe. Setup programs, DLLs, scripts, shortcuts, unknown names, relative paths, UNC/device paths, alternate streams, reparse points, hardlinks and write-locked files are refused. Mapped network drives are refused. This does not constitute a hostile-same-user or hostile-kernel security boundary.

The DLSS 5 project's v2.2.7 package manifest and release list were inspected on **28 September 2026** to confirm its installed/portable naming. These two projects are distinct. Electron portable packages use a separate launcher; x86 launchers are accepted on the Windows x64 host without pretending that the payload is a 32-bit video backend. Shiny does not pin users to that third-party version or claim that a newer build works correctly.

- [DLSS 5 Swapper project](https://github.com/rakanki911/DLSS5-Swapper)
- [Inspected v2.2.7 package manifest](https://github.com/rakanki911/DLSS5-Swapper/blob/v2.2.7/package.json)
- [v2.2.7 release information](https://github.com/rakanki911/DLSS5-Swapper/releases/tag/v2.2.7)
- [Classic Swapper project and limitations](https://github.com/beeradmoore/dlss-swapper)
- [Electron Builder portable wrapper source](https://github.com/electron-userland/electron-builder/blob/master/packages/app-builder-lib/templates/nsis/portable.nsi)

## Permissions and compatibility

The external program runs with the launching user's account permissions. **It is not sandboxed.** It can read/write user-accessible files, access network services, use companion modules and perform its own operations. It may have its own startup scanning or preferences. Review those independently. Shiny does not monitor, approve or certify that behavior.

Shiny uses a direct `CreateProcessW` call with an absolute application path, no extra arguments, no inherited handles, no shell and no automatic elevation. The selected application directory is its working directory. A tool requiring elevation is not automatically relaunched with `runas`; follow your system/organization's normal policies. No OS protections are disabled.

**No game, VLC directory, media filename, model folder, screenshot, capture region or other target is passed.** There is no browser/native-messaging command, unattended CLI or generic DLL-loading endpoint. Do not target Shiny or the installed VLC runtime with the external swapper. Its own documented compatible-game/emulator, GPU, driver, backup and anti-cheat restrictions still apply. Shiny has not validated those targets.

macOS/Linux builds show a disabled **Windows only** entry. They do not try Wine, invoke a shell or advertise native support for a Windows tool.

## This does not change the video backend

The existing [library-management plan](VLC_LIBRARY_MANAGEMENT_PLAN.md) remains in force: verified compatible packages, separate worker lifetimes, rollback and original playback. The new bridge does not approve NVIDIA/Streamline candidates in the managed store, relax local-model consent, provide render-engine motion/depth resources, inject a graphics proxy into VLC, or imply that a renamed DLL adds DLSS to a player. It has no connection to the research or capture data paths.

The primary libVLC player, the independent conventional spatial preview, the opt-in native model research and Qt Multimedia remain separate. Real DLSS video processing would require an implemented, licensed and tested rendering adapter. The external launch panel deliberately never emits a `dlssActive` state.

## Verification

`native/swapper` is shared original C++20 code, compiled with each Windows host's own runtime settings. `tests/swapper` includes portable filename/path/PE bounds tests and Windows file-integrity, cancellation, consent, tamper, hardlink, write-lock, direct process launch and modeless-UI default-state tests. Only a benign test executable is launched. It verifies that the child receives no command-line arguments and runs in its own directory. No actual third-party swapper or NVIDIA runtime is downloaded or executed in CI.

All normal host application, model, library, browser, playback and installer tests remain. These tests verify the bridge, not third-party functionality or DLSS image quality. Report executed exact-commit CI results, not the mere presence of this test source.
