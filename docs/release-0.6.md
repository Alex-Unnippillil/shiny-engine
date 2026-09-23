## Windows x64 installer and portable player

Download **ShinyPlayer-0.6.0-Windows-x64-Setup.exe**, or the Portable ZIP. Both are unsigned developer builds. Setup supports per-user installation, shortcuts and uninstall. Its optional checkbox obtains the hash-verified official VLC 3.0.24 runtime from VideoLAN. Otherwise use an already installed compatible VLC. No NVIDIA binary or model is bundled. macOS/Linux installers are not implemented.

## Native OpenDLSS-NR workbench

This release compiles the pinned MIT native Vulkan graph and shaders/PTX into an isolated worker, with local model fingerprinting, GPU feature probing, a separate muted VLC frame sampler, matched-frame composition controls and bounded worker messaging. **No model is approved or bundled: trained DLSS 5 inference remains unavailable in the distributed build.** This is not a completed or NVIDIA-certified DLSS product. See the included native-neural.md for exact remaining model/parity/temporal/hardware requirements.

## Player improvements

Cinema view, always-on-top, in-memory source bookmarks, jump-to-time, queue reordering, audio-device selection and media information. Existing playback/subtitle/equalizer/image-filter controls remain. RTX VSR requests and ordinary VLC filters remain explicitly distinct from DLSS NR. Full VLC opens the separately installed original interface for features not duplicated here.

## Verification and limitations

Exact-commit browser, native companion and VLC/native-NR builds must pass before release publication. Native integration tests exercise actual libVLC pixels and worker model rejection, not trained inference. Installer smoke exercises installation with a verified cached VLC archive, auto-detected playback and uninstall. No physical GPU speed, trained-model parity/quality, temporal-video fidelity, HDR, complete codec parity, real audio-device or platform-wide certification is implied. Source and runtime notices, source archive and SHA-256 checksums are attached.
