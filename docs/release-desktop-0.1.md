# Shiny Desktop 0.1.0 — Windows, macOS and Linux

An additive C++20 / Qt desktop edition for local video and audio. The existing Shiny Player 0.10.0 Windows libVLC edition and its enhancement/research tools are unchanged.

## Install

| Platform | Package |
|---|---|
| Windows x64 | `ShinyDesktop-0.1.0-Windows-x64-Setup.exe` or complete `-Portable.zip` |
| macOS 13+ Apple Silicon | `ShinyDesktop-0.1.0-macOS-arm64.dmg` |
| macOS 13+ Intel | `ShinyDesktop-0.1.0-macOS-x64.dmg` |
| Ubuntu 24.04 x64 | `ShinyDesktop-0.1.0-Linux-x64.deb` (apt resolves dependencies) |

Windows setup is per-user. On macOS, drag the app from the DMG to Applications. The Linux TGZ additionally requires the distribution's Qt/codec packages; it is not a universal AppImage.

These are prerelease packages: **Windows is unsigned; macOS is ad-hoc signed only and is not Developer-ID signed or notarized.** Follow platform/organization execution policies. No system-security bypass is recommended. Uninstall leaves preferences and all user media/playlists intact.

## Included

Dark/system-palette UI, native keyboard controls, local open/drop, title-filtered queue with correct model indexing, play/pause/stop/seek/rate/repeat, embedded audio/subtitle tracks, fullscreen, explicit JSON playlist save/load, and atomic decoded-frame PNG export. Only volume and theme are saved automatically; no account, telemetry or automatic media history.

## Important edition distinction

This player uses **Qt Multimedia, not libVLC**. It does not port the Windows enhancement-library store, DLL switching, neural research, driver VSR, external subtitle files or stream/capture functions. Use the existing Windows edition for its advanced features. No DLSS, HDR fidelity, physical-GPU performance, universal codec, sound-device or long-session certification is claimed.

## Evidence and sources

Release publication requires the exact main revision to pass all four platform packages and the five existing application check names. Each native-host job runs policy/UI checks, actual decoded playback, pause/seek/snapshot/stop, and deployed install/use/remove checks. Each platform has an evidence ZIP, actual screenshot, source ZIP and SHA-256 checksums. Evidence ZIP checksum lists refer to the complete build artifact, including separately distributed installer files.

Application source and build instructions are included. Qt is dynamically linked under LGPLv3; dependency license/copyright notices and corresponding-source access information are installed alongside the application. Linux keeps runtime libraries managed by its package manager. Read `THIRD_PARTY_NOTICES.md` and the installed `notices/source-access.json` before redistributing a modified build.
